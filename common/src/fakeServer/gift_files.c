#include "fakeServer/gifts.h"

int FsGift_FilePath(const FsGift *gift,uint32_t index,char path[FS_GIFT_FILE_PATH_SIZE])
{
    if (!gift || !path) return 0;
    uint32_t length=0,begin=0,used=0;
    while (length<sizeof(gift->file) && gift->file[length]) length++;
    if (!length || length==sizeof(gift->file)) return 0;
    if (gift->fileList) {
        if (index>=6 || !gift->folder[0]) return 0;
        uint32_t folderLength=0;
        while (folderLength<sizeof(gift->folder) && gift->folder[folderLength]) folderLength++;
        if (folderLength==sizeof(gift->folder)) return 0;
        for (uint32_t i=0;i<folderLength;i++) {
            if (used+1>=FS_GIFT_FILE_PATH_SIZE) return 0;
            path[used++]=gift->folder[i];
        }
        if (used+1>=FS_GIFT_FILE_PATH_SIZE) return 0;
        path[used++]='/';
        while (index) {
            while (begin<length && gift->file[begin]!=',') begin++;
            if (begin==length) return 0;
            begin++; index--;
        }
    } else if (index) return 0;
    if (begin==length || gift->file[begin]==',') return 0;
    for (;begin<length && (!gift->fileList || gift->file[begin]!=',');begin++) {
        if (used+1>=FS_GIFT_FILE_PATH_SIZE) return 0;
        path[used++]=gift->file[begin];
    }
    path[used]=0;
    return 1;
}
