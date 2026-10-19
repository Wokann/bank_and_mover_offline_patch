#include "fs_system/title_users.h"
#include "fs_system/utf_path.h"
#include "fakeServer/native_transport.h"
#include "fakeServer/numbers.h"
#include "fakeServer/memory.h"
#include "fakeServer/sd_adapter.h"
#include "fakeServer/fake_servers.h"
#include "fakeServer/fake_server_client.h"
#include "title_user_sdk.h"
#include "system_time.h"
#include <string.h>

enum { TITLE_USERS_REQUEST=0x405, TITLE_TEXT_UNITS=128 };
typedef struct TitleText { uintptr_t reserved; const uint16_t *text; } TitleText;
static struct {
    void *view;
    uint32_t previousKeys,mode,refresh,pending,valid,visible,dirty;
    uint32_t heldKeys,blockedKeys;
    uint64_t heldSince,lastStep,lastChange,retryAfter;
    uint32_t action,submittedUser,savedUser,uncertain,position,capacity,stringsReady,exitChecked;
    FsUserEntry *users;
    FsUserMenu menu,automatic;
    uint16_t label[TITLE_TEXT_UNITS],hint[TITLE_TEXT_UNITS];
} titleUsers __attribute__((section(".data.title_users")))={0};

static void *backend(void *view)
{ return view?*(void **)((uint8_t *)view+0x5c):0; }
static void *pane(void *view,uint32_t id)
{
    void *ui=backend(view);if(!ui)return 0;
    void *screen=((void *(*)(void *,uint32_t))TITLE_USER_GET_SCREEN)(ui,1);
    return screen?((void *(*)(void *,uint32_t))TITLE_USER_FIND_PANE)(screen,id):0;
}
static void show(void *view,uint32_t visible)
{
    for(uint32_t id=TITLE_USER_STATUS_PANE;id<=TITLE_USER_ID_PANE;id++) {
        uint8_t *text=pane(view,id);
        /* The stock Pane visibility setter changes only bit zero at +0xB7. */
        if(text)text[0xb7]=(uint8_t)((text[0xb7]&0xfe)|visible);
    }
}
static uint32_t appendMessage(void *view,uint16_t *out,uint32_t used,uint32_t message)
{
    void *ui=backend(view);
    void *buffer=((void *(*)(void *,uint32_t))TITLE_USER_GET_BUFFER)(ui,1);
    if(!buffer)return used;
    ((void (*)(void *,void *,uint32_t))TITLE_USER_LOAD_MESSAGE)(ui,buffer,message);
    const uint16_t *text=*(const uint16_t **)((uint8_t *)buffer+4);
    if(text)while(*text && used+1<TITLE_TEXT_UNITS)out[used++]=*text++;
    out[used]=0;return used;
}
static uint32_t appendNumber(uint16_t *out,uint32_t used,uint32_t value)
{
    uint16_t digits[10];uint32_t count=0;
    do {uint32_t digit;value=FsDivideU32(value,10,&digit);digits[count++]=(uint16_t)('0'+digit);}while(value);
    while(count && used+1<TITLE_TEXT_UNITS)out[used++]=digits[--count];
    out[used]=0;return used;
}
static void render(void *view,uint32_t id,const uint16_t *text)
{
    void *target=pane(view,id);if(!target)return;
    /* Both native renderers read only StrBuf+4, copy the UTF-16 text through
       TextBox's original setter, and retain no pointer to this stack view. */
    const TitleText buffer={0,text};
    ((void (*)(void *,const TitleText *,uint32_t))TITLE_USER_RENDER_TEXT)(target,&buffer,UINT32_MAX);
}
static void redraw(void *view)
{
    uint16_t text[TITLE_TEXT_UNITS]={0};uint32_t used=0;
    if(!titleUsers.stringsReady) {
        appendMessage(view,titleUsers.label,0,TITLE_USER_LABEL_MESSAGE);
        appendMessage(view,titleUsers.hint,0,TITLE_USER_HINT_MESSAGE);
        titleUsers.stringsReady=1;
    }
    while(titleUsers.label[used] && used+1<TITLE_TEXT_UNITS) {
        text[used]=titleUsers.label[used];used++;
    }
    if(!titleUsers.menu.activeUser) {
        static const uint16_t automatic[]={'a','u','t','o','(',0};
        for(uint32_t i=0;automatic[i] && used+1<TITLE_TEXT_UNITS;i++)text[used++]=automatic[i];
        used=appendNumber(text,used,titleUsers.menu.number);
        if(used+1<TITLE_TEXT_UNITS)text[used++]=')';
        text[used]=0;
    } else used=appendNumber(text,used,titleUsers.menu.activeUser);
    if(titleUsers.menu.count>1) {
        for(uint32_t i=0;titleUsers.hint[i] && used+1<TITLE_TEXT_UNITS;i++)text[used++]=titleUsers.hint[i];
        text[used]=0;
    }
    render(view,TITLE_USER_STATUS_PANE,text);
    used=appendNumber(text,0,titleUsers.menu.number);
    text[used++]='=';
    uint16_t id[FS_USER_ID_SIZE];uint32_t length=0;
    if(FsUtf8_ToUtf16(titleUsers.menu.id,id,FS_USER_ID_SIZE,&length))
        for(uint32_t i=0;i<length && used+1<TITLE_TEXT_UNITS;i++)text[used++]=id[i];
    text[used]=0;render(view,TITLE_USER_ID_PANE,text);
}
void TitleUsers_Initialize(void *view)
{
    FakeServerClient_EndSession();
    NativeTransport_Cancel((uintptr_t)&titleUsers);
    /* Reuse only the process-heap allocation, never a previous title's values.
       Allocation/release is serialized with the worker when a reply arrives. */
    FsUserEntry *users=titleUsers.users;uint32_t capacity=titleUsers.capacity;
    titleUsers=(__typeof__(titleUsers)){.view=view,.previousKeys=0xc0,.mode=UINT32_MAX,.refresh=1,.dirty=1,
        .users=users,.capacity=capacity};
    show(view,0);
}
static void choose(uint32_t position)
{
    titleUsers.position=position;
    titleUsers.exitChecked=0;
    titleUsers.menu=titleUsers.automatic;titleUsers.menu.activeUser=0;
    if(position) {
        const FsUserEntry *entry=&titleUsers.users[position-1];
        titleUsers.menu.activeUser=titleUsers.menu.number=entry->number;
        strcpy(titleUsers.menu.id,entry->id);
    }
    titleUsers.dirty=1;
}
static int poll(int closing)
{
    NativeTransportScope scope __attribute__((cleanup(NativeTransport_Leave)))={NativeTransport_Lock(0)};
    if(scope.status==FS_ASYNC_BUSY)return 1;
    if(scope.status!=FS_ASYNC_ACCEPTED)goto failed;
    FsServiceRequest request={.operation=FS_SERVICE_TITLE_USERS};
    request.input.users.action=titleUsers.action;
    request.input.users.activeUser=titleUsers.submittedUser;
    uint32_t size=titleUsers.action?titleUsers.menu.count*sizeof(FsUserEntry):FS_BANK_SIZE;
    FsAsyncDelivery delivery;
    int received=NativeTransport_PollRequest((uintptr_t)&titleUsers,TITLE_USERS_REQUEST,
        &request,titleUsers.action?titleUsers.users:0,size,&delivery);
    if(received==FS_ASYNC_BUSY || received==FS_ASYNC_ACCEPTED)return 1;
    if(received!=FS_ASYNC_READY)goto failed;
    int result=delivery.result;
    FsUserMenu menu=delivery.reply.output.users;
    if(result==FS_OK && !titleUsers.action) {
        uint32_t count=menu.count;
        if(count>delivery.reply.bodySize/sizeof(FsUserEntry) ||
            delivery.reply.bodySize!=count*sizeof(FsUserEntry))result=FS_INVALID;
        else {
            const FsMemory memory={FsSdStorage.allocate,FsSdStorage.release};
            result=FsMemory_Reserve(&memory,(void **)&titleUsers.users,&titleUsers.capacity,
                count,0,sizeof(FsUserEntry));
            if(result==FS_OK && count) {
                int copied=NativeTransport_CopyReply((uintptr_t)&titleUsers,&delivery,
                    titleUsers.users,delivery.reply.bodySize,16384);
                if(copied==FS_ASYNC_BUSY)return 1;
                if(copied!=FS_ASYNC_READY)result=FS_INVALID;
            }
            for(uint32_t i=0;result==FS_OK && i<count;i++) {
                const FsUserEntry *entry=&titleUsers.users[i];
                if(!entry->number || entry->number>0x7fffffffu ||
                    (i && titleUsers.users[i-1].number>=entry->number) ||
                    !FsUser_ValidId(entry->id))result=FS_INVALID;
            }
        }
    }
    int released=NativeTransport_Release(&delivery);
    if(released==FS_ASYNC_BUSY)return 1;
    if(released!=FS_ASYNC_ACCEPTED || result!=FS_OK)goto failed;
    if(titleUsers.action) {
        if(menu.activeUser!=titleUsers.submittedUser)goto failed;
        titleUsers.savedUser=titleUsers.submittedUser;titleUsers.uncertain=0;
        if(closing && titleUsers.menu.activeUser==titleUsers.submittedUser)titleUsers.exitChecked=1;
    } else {
        titleUsers.automatic=menu;titleUsers.automatic.activeUser=0;
        titleUsers.savedUser=menu.activeUser;titleUsers.uncertain=0;
        uint32_t position=0;
        for(uint32_t i=0;i<menu.count;i++)if(titleUsers.users[i].number==menu.activeUser)position=i+1;
        choose(position);titleUsers.valid=1;
    }
    titleUsers.pending=0;titleUsers.retryAfter=0;return 0;
failed:
    NativeTransport_Cancel((uintptr_t)&titleUsers);
    titleUsers.pending=0;
    if(titleUsers.action)titleUsers.uncertain=1;
    else {titleUsers.valid=0;titleUsers.refresh=1;titleUsers.dirty=1;}
    /* A failed acknowledgement may follow an effective publish. Retrying the
       current desired selection is idempotent; never enter under a stale one.
       Back off on actual I/O failure instead of spinning/replaying each frame. */
    titleUsers.retryAfter=SystemTime_GetMonotonicTicks()+SYSTEM_TIME_TICKS_PER_SECOND/2u;
    return 1;
}
static int inputDirection(uint32_t keys,int enabled)
{
    /* HID bits 30/31 are Circle Pad up/down, 6/7 are D-pad up/down.
       Merge before edge/repeat detection; C-stick bits 26/27 are excluded. */
    uint32_t directions=(keys&0xc0u)|((keys>>24)&0xc0u);
    uint32_t edges=directions&~titleUsers.previousKeys;
    titleUsers.previousKeys=directions;
    if(!enabled || !directions || directions==0xc0u) {
        titleUsers.heldKeys=titleUsers.blockedKeys=0;return 0;
    }
    if(edges==0x40u || edges==0x80u) {
        titleUsers.heldKeys=directions;titleUsers.blockedKeys=0;
        titleUsers.heldSince=titleUsers.lastStep=SystemTime_GetMonotonicTicks();
    } else {
        if(titleUsers.heldKeys!=directions || titleUsers.blockedKeys==directions)return 0;
        uint64_t now=SystemTime_GetMonotonicTicks();
        if(now<titleUsers.heldSince || now<titleUsers.lastStep) {
            titleUsers.heldSince=titleUsers.lastStep=now;return 0;
        }
        uint64_t held=now-titleUsers.heldSince;
        uint64_t interval=held>=SYSTEM_TIME_TICKS_PER_SECOND*2u?
            SYSTEM_TIME_TICKS_PER_SECOND*60u/1000u:SYSTEM_TIME_TICKS_PER_SECOND*150u/1000u;
        if(held<SYSTEM_TIME_TICKS_PER_SECOND/2u ||
            now-titleUsers.lastStep<interval)return 0;
        /* One in-memory selection per interval, never a catch-up burst. */
        titleUsers.lastStep=now;
    }
    return directions==0x40u?-1:1;
}
int TitleUsers_Update(void *state,uint32_t mode)
{
    uint8_t *bytes=state;if(!bytes)return 0;
    void *view=*(void **)(bytes+0x38);
    if(view!=titleUsers.view)TitleUsers_Initialize(view);
    uint32_t keys=0;
    const uint8_t *hid=*(const uint8_t **)(TITLE_USER_HID+4);
    if(hid) {
        uint32_t index=*(const uint32_t *)(hid+0x10);
        if(index<=7)keys=*(const uint32_t *)(hid+0x28+index*16);
    }
    uint32_t userMode=mode<32u && ((TITLE_USER_SELECTABLE_MODES>>mode)&1u);
    if(mode!=titleUsers.mode) {
        titleUsers.mode=mode;titleUsers.dirty=1;
        if(userMode)titleUsers.refresh=1;
    }
    int interactive=!bytes[0x3c] && (!TITLE_USER_EXTRA_EXIT || !bytes[0x3d]);
    if(TITLE_USER_ACTIVE_VIEW)interactive=interactive && view && ((const uint8_t *)view)[0x2d];
    if(interactive)titleUsers.exitChecked=0;
    uint32_t previousDirections=titleUsers.previousKeys;
    int direction=inputDirection(keys,userMode && interactive && titleUsers.valid &&
        titleUsers.menu.count>1 && !titleUsers.refresh && !(keys&0x300));
    if(direction) {
        uint32_t position=titleUsers.position;
        if(direction<0){if(position)position--;}
        else if(position<titleUsers.menu.count)position++;
        if(position!=titleUsers.position) {
            choose(position);titleUsers.lastChange=SystemTime_GetMonotonicTicks();
        } else titleUsers.blockedKeys=direction<0?0x40u:0x80u;
    }
    uint64_t now=SystemTime_GetMonotonicTicks();
    if(previousDirections && !titleUsers.previousKeys)titleUsers.lastChange=now;
    if(now<titleUsers.lastChange)titleUsers.lastChange=now;
    int unsaved=titleUsers.valid && (titleUsers.uncertain || titleUsers.menu.activeUser!=titleUsers.savedUser ||
        (userMode && !interactive && titleUsers.menu.count && !titleUsers.exitChecked));
    int force=!interactive || !userMode || titleUsers.refresh;
    if(!titleUsers.pending && now>=titleUsers.retryAfter) {
        uint32_t directions=(keys&0xc0u)|((keys>>24)&0xc0u);
        if(unsaved && (force || (!directions &&
            now-titleUsers.lastChange>=SYSTEM_TIME_TICKS_PER_SECOND*150u/1000u))) {
            titleUsers.action=1;titleUsers.submittedUser=titleUsers.menu.activeUser;titleUsers.pending=1;
        } else if(!unsaved && userMode && titleUsers.refresh) {
            titleUsers.action=0;titleUsers.submittedUser=0;titleUsers.pending=1;titleUsers.refresh=0;
        }
    }
    if(titleUsers.pending)(void)poll(!interactive);
    uint32_t visible=userMode && titleUsers.valid && titleUsers.menu.count>0;
    if(visible && titleUsers.dirty)redraw(view);
    if(titleUsers.dirty || visible!=titleUsers.visible)show(view,visible);
    titleUsers.visible=visible;titleUsers.dirty=0;
    unsaved=titleUsers.valid && (titleUsers.uncertain || titleUsers.menu.activeUser!=titleUsers.savedUser ||
        (userMode && !interactive && titleUsers.menu.count && !titleUsers.exitChecked));
    return titleUsers.pending || unsaved || (userMode && titleUsers.refresh);
}
