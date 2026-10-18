#include "fakeServer/memory.h"
#include "fakeServer/numbers.h"

int FsMemory_Reserve(const FsMemory *memory,void **data,uint32_t *capacity,
    uint32_t needed,uint32_t used,uint32_t width)
{
    if (!memory || !data || !capacity || !width || used>*capacity ||
        (*capacity && !*data)) return FS_INVALID;
    uint32_t maximum=UINT32_MAX,scale=width;
    while (!(scale&1u)) { maximum>>=1;scale>>=1; }
    if (scale!=1u) maximum=FsDivideU32(UINT32_MAX,width,0);
    if (*capacity>maximum) return FS_INVALID;
    if (needed<=*capacity) return FS_OK;
    if (!memory->allocate || !memory->release || needed>maximum) return FS_NO_MEMORY;
    uint32_t growth=*capacity/2u+1u;
    uint32_t target=growth>maximum-*capacity?maximum:*capacity+growth;
    if (target<needed) target=needed;
    void *next=memory->allocate(target*width);
    if (!next && target!=needed) { target=needed;next=memory->allocate(target*width); }
    if (!next) return FS_NO_MEMORY;
    for (uint32_t i=0;i<used*width;i++) ((uint8_t *)next)[i]=((const uint8_t *)*data)[i];
    if (*data) memory->release(*data);
    *data=next;*capacity=target;
    return FS_OK;
}

int FsMemory_Text(const FsMemory *memory,char **text,uint32_t *capacity,
    const char *initial,uint32_t used,uint32_t needed)
{
    if (needed<=*capacity) return FS_OK;
    int local=*text==initial;
    void *data=local?0:*text;uint32_t next=local?0:*capacity;
    int result=FsMemory_Reserve(memory,&data,&next,needed,local?0:used,1);
    if (result!=FS_OK) return result;
    if (local) for (uint32_t i=0;i<used;i++) ((char *)data)[i]=initial[i];
    *text=data;*capacity=next;
    return FS_OK;
}
