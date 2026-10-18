#include "fakeServer/fake_servers.h"

static int payloadShape(const FsGift *gift,uint32_t size)
{
    if (gift->generation==6) return size==FS_GIFT_MAX_SIZE;
    if (gift->generation==7)
        for (uint32_t cards=1;cards<=6;cards++)
            if (size==cards*FS_GIFT_CARD_SIZE) return 1;
    return 0;
}

static uint32_t read16(const uint8_t *value)
{
    return (uint32_t)value[0]|((uint32_t)value[1]<<8);
}

static void write16(uint8_t *value,uint32_t number)
{
    value[0]=(uint8_t)number; value[1]=(uint8_t)(number>>8);
}

static int suffix(const char *path,const char *ending)
{
    uint32_t length=0,count=0;
    while (path[length]) length++;
    while (ending[count]) count++;
    if (length<count) return 0;
    path+=length-count;
    for (uint32_t i=0;i<count;i++) {
        char value=path[i];
        if (value>='A' && value<='Z') value=(char)(value-'A'+'a');
        if (value!=ending[i]) return 0;
    }
    return 1;
}

/* PL6 and normal/full Wonder Cards are input assets, not Bank responses. Legacy
   .bin assets retain their already-framed response shape and checksum. */
/* PL6 与普通／Full 卡片是输入素材，不是 Bank 响应；旧 .bin 保留封装和校验约定。 */
static int cardShape(const FsGift *gift,const char *path,uint32_t size,uint32_t *offset,uint32_t *length)
{
    if (!gift->fileList && suffix(path,".bin")) {
        if (!payloadShape(gift,size)) return 0;
        *offset=0; *length=size;
        return 1;
    }
    if (!gift->fileList && gift->generation==6 && suffix(path,".pl6") && size==FS_GIFT_PL6_SIZE) {
        *offset=0; *length=FS_GIFT_MAX_SIZE;
        return 3;
    }
    int ordinary=suffix(path,gift->generation==6?".wc6":".wc7");
    int full=suffix(path,gift->generation==6?".wc6full":".wc7full");
    if ((!ordinary && !full) || size!=(ordinary?FS_GIFT_ORDINARY_SIZE:FS_GIFT_CARD_SIZE))
        return 0;
    *offset=ordinary?0:0x208;
    *length=gift->generation==6?FS_GIFT_MAX_SIZE:FS_GIFT_CARD_SIZE;
    return 2;
}

static uint16_t crc(const uint8_t *body,uint32_t size,int framed)
{
    uint16_t value=0xffff;
    uint32_t offset=0;
    for (uint32_t i=0;i<size;i++) {
        int cleared=framed && (offset==0x202 || offset==0x203);
        uint8_t byte=cleared?0:body[i];
        value^=(uint16_t)((uint16_t)byte<<8);
        for (uint32_t bit=0;bit<8;bit++)
            value=(uint16_t)((value&0x8000)?((uint32_t)value<<1)^0x1021u:(uint32_t)value<<1);
        if (++offset==FS_GIFT_CARD_SIZE) offset=0;
    }
    return value;
}

/* Reuse the caller's response buffer. Copy backwards before clearing frames,
   since PL6 source records and the expanded Bank packet share this buffer. */
/* 复用调用方正文缓冲：从末槽向前复制，再清封装，避免覆盖同一缓冲中的 PL6 源记录。 */
static int frameLink(uint8_t *bytes)
{
    if (read16(bytes+0xA45)!=crc(bytes+1,0xA44,0)) return FS_INVALID;
    if (read16(bytes+0x4A1) || read16(bytes+0x4A3)) return FS_UNSUPPORTED;
    uint8_t items[24];
    for (uint32_t i=0;i<sizeof(items);i++) items[i]=bytes[0x489+i];
    for (uint32_t i=0;i<6;i++) {
        uint32_t start=0x99+i*0xA8;
        /* The native receiver resets per-slot flags and has no tail fields. */
        /* 原接收器会重置逐槽标记，且无法表达模板尾字段。 */
        for (uint32_t j=0;j<4;j++) if (bytes[start+j]) return FS_UNSUPPORTED;
        for (uint32_t j=0x84;j<0xA4;j++) if (bytes[start+j]) return FS_UNSUPPORTED;
    }
    for (uint32_t i=6;i--;) {
        uint8_t *frame=bytes+i*FS_GIFT_CARD_SIZE;
        const uint8_t *entity=bytes+0x9D+i*0xA8;
        int present=read16(entity+0x1A)!=0;
        for (uint32_t j=0;j<0x80;j++) frame[0x270+j]=present?entity[j]:0;
        for (uint32_t j=0;j<0x270;j++) frame[j]=0;
        for (uint32_t j=0x2F0;j<FS_GIFT_CARD_SIZE;j++) frame[j]=0;
        write16(frame+0x26E,read16(items+i*4));
        write16(frame+0x26C,read16(items+i*4+2));
    }
    return FS_OK;
}

static int readOffer(FsServer *server,uint32_t index,FsGiftOffer *offer)
{
    const FsGift *gift=&server->configuration.gifts[index];
    uint32_t length=0,count=0,shape=0,id=gift->nativeId;
    if (gift->fileList) { count=1; for (const char *p=gift->file;*p;p++) if (*p==',') count++; }
    else count=1;
    if (count>6) return FS_LIMIT;
    for (uint32_t i=0;i<count;i++) {
        char path[FS_GIFT_FILE_PATH_SIZE]; uint8_t nativeId[2];
        uint32_t handle=0,size=0,offset=0,cardLength=0;
        if (!FsGift_FilePath(gift,i,path)) return FS_INVALID;
        int opened=server->storage->openRead(path,&handle,&size);
        if (opened!=1) return opened?FS_IO_ERROR:FS_MISSING;
        shape=cardShape(gift,path,size,&offset,&cardLength);
        if (gift->fileList && shape!=2) shape=0;
        int read=shape && (shape==3 || server->storage->read(handle,shape==1?0x208:offset,nativeId,2));
        if (!server->storage->close(handle)) return FS_IO_ERROR;
        if (!shape) return FS_INVALID;
        if (!read) return FS_IO_ERROR;
        if (!i && !id && shape!=3) id=read16(nativeId);
        length=cardLength;
    }
    if (gift->fileList && gift->generation==7) length=count*FS_GIFT_CARD_SIZE;
    if (!id) return FS_INVALID;
    offer->index=index; offer->length=length; offer->nativeId=id;
    offer->inputFormat=shape;
    return FS_OK;
}

static int requestValid(const FsServer *server,const FsGiftRequest *request)
{
    return server && server->ready && server->owner==FS_TRANSACTION_OWNER_BANK &&
        server->configurationStatus==FS_OK &&
        request && request->game<=FS_GAME_UM && FsDate_Valid(request->date) &&
        request->nativeReady<=1;
}

static int windowMatches(const FsGiftWindow *window,const FsGiftRequest *request)
{
    uint32_t scope=window->flags>>6;
    return scope==FS_GIFT_GLOBAL || (scope==FS_GIFT_REGION && request->region<=6 &&
        window->selector==request->region) || (scope==FS_GIFT_COUNTRY && request->country &&
        window->selector==request->country);
}

static int eligible(const FsConfiguration *config,const FsGift *gift,const FsGiftRequest *request)
{
    if (!gift->enabled || !(gift->games&(1u<<request->game))) return 0;
    uint32_t event=(uint32_t)(gift-config->gifts),scope=0;
    int allowed=0,selected=0;
    for (uint32_t i=0;i<config->giftWindowCount;i++) {
        const FsGiftWindow *window=config->giftWindows+i;
        if (window->event!=event || !windowMatches(window,request)) continue;
        int inside=!(window->flags&FS_GIFT_WINDOW_DISABLED) &&
            request->date>=window->starts && request->date<=window->ends;
        if (window->flags&FS_GIFT_WINDOW_EXCLUDE) { if (inside) return 0; continue; }
        uint32_t current=window->flags>>6;
        if (!selected || current>scope) { scope=current; allowed=0; selected=1; }
        if (current==scope && inside) allowed=1;
    }
    return selected?allowed:(gift->starts && gift->ends &&
        request->date>=gift->starts && request->date<=gift->ends);
}

static int recipientReady(const FsGiftRequest *request)
{
    return request->nativeReady;
}

/* Stateless gift provider: return the configured content, even for a known ID.
   Only the stock receiver/save/transaction path decides adoption and recovery. */
/* 无礼物专用账本、缓存或服务端防重；即使编号已知也照常返回，由原版接收器判断。 */
static int queryGift(FsServer *server,const FsGiftRequest *request,FsGiftOffer *offer,uint32_t first)
{
    uint32_t applied=0;
    int result=first?FS_OK:FsServer_ReloadConfiguration(server,&applied);
    if (result!=FS_OK) return result;
    FsGiftRequest located;
    if(request){located=*request;FsServer_UserLocation(server,&located.region,&located.country);request=&located;}
    if (!requestValid(server,request) || !offer) return FS_INVALID;
    if (!recipientReady(request)) return FS_NO_GIFT;
    if (server->configuration.giftError.result!=FS_OK) return server->configuration.giftError.result;
    for (uint32_t i=first;i<server->configuration.giftCount;i++) {
        const FsGift *gift=&server->configuration.gifts[i];
        if (!eligible(&server->configuration,gift,request)) continue;
        result=readOffer(server,i,offer);
        if (result==FS_MISSING || result==FS_INVALID || result==FS_UNSUPPORTED) continue;
        if (result!=FS_OK) return result;
        return result;
    }
    return FS_NO_GIFT;
}

int FsGifts_Query(FsServer *server,const FsGiftRequest *request,FsGiftOffer *offer)
{
    return queryGift(server,request,offer,0);
}

static int prepareGift(FsServer *server,const FsGiftRequest *request,uint32_t index,
    void *body,uint32_t capacity,FsGiftOffer *offer)
{
    FsGiftRequest located;
    if(request){located=*request;FsServer_UserLocation(server,&located.region,&located.country);request=&located;}
    if (!requestValid(server,request) || !body || !offer) return FS_INVALID;
    if (!recipientReady(request)) return FS_NO_GIFT;
    int access=FsServer_GiftDeliverySupport(server);
    if (access!=FS_OK) return access;
    if (server->configuration.giftError.result!=FS_OK) return server->configuration.giftError.result;
    if (index>=server->configuration.giftCount) return FS_INVALID;
    const FsGift *gift=&server->configuration.gifts[index];
    if (!eligible(&server->configuration,gift,request)) return FS_NO_GIFT;
    int result=readOffer(server,index,offer);
    if (result!=FS_OK) return result;
    uint32_t length=offer->length,shape=offer->inputFormat;
    uint8_t *bytes=body;
    if (capacity<length) return FS_LIMIT;
    for (uint32_t i=0;i<length;i++) bytes[i]=0;
    uint32_t count=1;
    if (shape==2 && gift->fileList)
        for (uint32_t i=0;gift->file[i];i++) if (gift->file[i]==',') count++;
    for (uint32_t i=0;i<count;i++) {
        char path[FS_GIFT_FILE_PATH_SIZE];
        if (!FsGift_FilePath(gift,i,path)) return FS_INVALID;
        uint32_t handle=0,size=0,offset=0,cardLength=0;
        int opened=server->storage->openRead(path,&handle,&size);
        if (opened!=1) return opened?FS_IO_ERROR:FS_MISSING;
        int actualShape=cardShape(gift,path,size,&offset,&cardLength);
        int bounded=(uint32_t)actualShape==shape && (shape!=1 || cardLength==length);
        int read=bounded && server->storage->read(handle,offset,
            shape==2?bytes+i*FS_GIFT_CARD_SIZE+0x208:bytes,shape==2?FS_GIFT_ORDINARY_SIZE:size);
        if (!server->storage->close(handle)) return FS_IO_ERROR;
        if (!bounded) return FS_INVALID;
        if (!read) return FS_IO_ERROR;
    }
    offer->index=index; offer->length=length;
    offer->nativeId=gift->nativeId?gift->nativeId:read16(bytes+0x208);
    offer->inputFormat=shape;
    if (!offer->nativeId) return FS_INVALID;
    if (shape==3) {
        result=frameLink(bytes);
        if (result!=FS_OK) return result;
    } else if (shape==1) {
        if (gift->generation==6) {
            if (read16(bytes+0x202)!=crc(bytes,length,1)) return FS_INVALID;
        } else for (uint32_t i=0;i<length;i+=FS_GIFT_CARD_SIZE) {
            if (read16(bytes+i+0x202)!=crc(bytes+i,FS_GIFT_CARD_SIZE,1)) return FS_INVALID;
        }
    } else for (uint32_t card=0;card<length;card+=FS_GIFT_CARD_SIZE) {
        uint8_t *template=bytes+card;
        uint32_t kind=template[0x259];
        if (kind>1) return FS_UNSUPPORTED; /* Beans/BP are not item cards. / 豆子／BP 不是物品卡。 */
        if (gift->generation==6 && kind==1) {
            uint32_t item=read16(template+0x270),quantity=read16(template+0x278);
            for (uint32_t i=0x270;i<FS_GIFT_CARD_SIZE;i++) template[i]=0;
            write16(template+0x26C,quantity); write16(template+0x26E,item);
        } else if (gift->generation==6) {
            /* The original Bank receiver copies 0x80 template bytes only.
               Do not silently drop unsupported WC6 tail fields, and never
               complete a game buffer from outside the original receiver. */
            /* 原接收器仅复制 0x80 模板字节；不丢弃不可表达的 WC6 尾字段，
               也不在原接收器之外补写游戏缓冲区。 */
            for (uint32_t i=0x2F0;i<FS_GIFT_CARD_SIZE;i++)
                if (template[i]) return FS_UNSUPPORTED;
            write16(template+0x26C,0); write16(template+0x26E,0);
        }
        /* Gen6 has no stored Wonder Card header; its receiver uses Link fields. */
        /* 六代不保存奇迹卡头；只封装原接收器使用的 Link 字段。 */
        if (gift->generation==6)
            for (uint32_t i=0x208;i<0x26C;i++) template[i]=0;
    }
    int hasContent=0,hasPokemon=0,hasItems=0;
    for (uint32_t i=0;i<length;i+=FS_GIFT_CARD_SIZE) {
        int cardContent=0;
        if (gift->generation==6) {
            if (read16(bytes+i+0x28A)>721) return FS_INVALID;
            if (read16(bytes+i+0x28A)) { hasPokemon=1; cardContent=1; }
            if (read16(bytes+i+0x26C) && read16(bytes+i+0x26E)) { hasItems=1; cardContent=1; }
        } else {
            if (bytes[i+0x259]>1) return FS_UNSUPPORTED;
            if (!bytes[i+0x259]) {
                if (read16(bytes+i+0x28A)>807) return FS_INVALID;
                if (read16(bytes+i+0x28A)) cardContent=1;
            } else for (uint32_t j=0;j<6;j++)
                if (read16(bytes+i+0x270+j*4) && read16(bytes+i+0x272+j*4)) cardContent=1;
        }
        if (shape==2 && i<count*FS_GIFT_CARD_SIZE && !cardContent) return FS_INVALID;
        if (cardContent) hasContent=1;
    }
    /* Native Gen6 drops all items whenever a Pokemon is present. */
    /* 原六代接收器有宝可梦时会清空全部物品；拒绝混批，不能静默丢失。 */
    if (hasPokemon && hasItems) return FS_UNSUPPORTED;
    if (!hasContent) return FS_INVALID;
    write16(bytes+0x208,offer->nativeId);
    if (gift->generation==6) write16(bytes+0x202,crc(bytes,length,1));
    else for (uint32_t i=0;i<length;i+=FS_GIFT_CARD_SIZE)
        write16(bytes+i+0x202,crc(bytes+i,FS_GIFT_CARD_SIZE,1));
    return FS_OK;
}

int FsGifts_Prepare(FsServer *server,const FsGiftRequest *request,uint32_t index,
    void *body,uint32_t capacity,FsGiftOffer *offer)
{
    int result=prepareGift(server,request,index,body,capacity,offer);
    /* Missing/invalid optional material means no content; real I/O still fails. */
    /* 可选素材缺少／损坏时不发放；真实 I/O 错误仍返回失败。 */
    if ((result==FS_MISSING || result==FS_INVALID || result==FS_UNSUPPORTED) &&
        requestValid(server,request) && body && offer && index<server->configuration.giftCount) return FS_NO_GIFT;
    return result;
}

int FsGifts_Reply(FsServer *server,const FsGiftRequest *request,uint32_t pass,
    void *body,uint32_t capacity,uint32_t *size)
{
    if (!size) return FS_INVALID;
    *size=0;
    FsGiftOffer offer;
    int status=FsGifts_Query(server,request,&offer);
    while (status==FS_OK) {
        uint32_t next=offer.index+1;
        status=FsGifts_Prepare(server,request,offer.index,body,capacity,&offer);
        if (status!=FS_NO_GIFT) break;
        status=queryGift(server,request,&offer,next);
    }
    if (status==FS_OK) { *size=offer.length; return FS_OK; }
    if (status!=FS_NO_GIFT) return status;
    if (!request || !body) return FS_INVALID;
    uint32_t length=request->game<=FS_GAME_AS?FS_GIFT_MAX_SIZE:FS_GIFT_CARD_SIZE;
    if (capacity<length) return FS_LIMIT;
    uint8_t *bytes=body;
    for (uint32_t i=0;i<length;i++) bytes[i]=0;
    /* A valid packet with an already-known content ID takes the native
       no-new-gift branch. An empty body would not be a valid native response. */
    /* 合法正文使用已知批次号，由原接收器进入无新礼物分支；空正文不是有效响应。 */
    write16(bytes+0x208,request->nativeContentIds[pass?1:0]);
    write16(bytes+0x202,crc(bytes,length,1));
    *size=length;
    return FS_OK;
}
