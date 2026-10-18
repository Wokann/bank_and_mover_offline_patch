#ifndef COMMON_FAKE_SERVER_GIFTS_H
#define COMMON_FAKE_SERVER_GIFTS_H

#include "types.h"

enum FsGiftConstant {
    FS_GIFT_LIMIT = 16,
    FS_EVENT_NAME_SIZE = 32,
    FS_GIFT_PATH_SIZE = 96,
    FS_GIFT_FOLDER_SIZE = 64,
    FS_GIFT_FILE_PATH_SIZE = 138,
    FS_GIFT_WINDOW_LIMIT = 32,
    FS_GIFT_ORDINARY_SIZE = 0x108,
    FS_GIFT_PL6_SIZE = 0xA47,
    FS_GIFT_CARD_SIZE = 0x310,
    FS_GIFT_MAX_SIZE = 6 * FS_GIFT_CARD_SIZE,
    FS_GIFT_GEN6_LINK_SIZE = 0xC48,
    FS_GIFT_GEN7_CARD_LIMIT = 48
};

enum FsGiftGame {
    FS_GAME_X,FS_GAME_Y,FS_GAME_OR,FS_GAME_AS,
    FS_GAME_SUN,FS_GAME_MOON,FS_GAME_US,FS_GAME_UM
};

typedef struct FsGift {
    char name[FS_EVENT_NAME_SIZE];
    char file[FS_GIFT_PATH_SIZE];
    uint32_t enabled;
    uint32_t generation;
    uint32_t games;
    uint32_t starts;
    uint32_t ends;
    uint32_t nativeId; /* Optional stable ID override / 可选的固定批次号。 */
    char folder[FS_GIFT_FOLDER_SIZE]; /* Empty for a legacy single file. / 单文件兼容路径为空。 */
    uint32_t fileList; /* Ordered WC basenames, rather than one legacy path. / 有序 WC 文件名列表。 */
} FsGift;

/* Date rules belong to one activity, not separate regional receipt identities. */
/* 日期规则归属同一活动，不为不同地区建立独立领取身份。 */
enum FsGiftWindowScope { FS_GIFT_GLOBAL, FS_GIFT_REGION, FS_GIFT_COUNTRY };
enum FsGiftWindowFlag {
    FS_GIFT_WINDOW_START=1, FS_GIFT_WINDOW_END=2,
    FS_GIFT_WINDOW_RANGE=4, FS_GIFT_WINDOW_EXCLUDE=8, FS_GIFT_WINDOW_DISABLED=16
};
typedef struct FsGiftWindow {
    uint32_t starts,ends;
    uint16_t selector;
    uint8_t event,flags; /* Scope in the upper two bits. / 高两位保存匹配层级。 */
} FsGiftWindow;

int FsGift_FilePath(const FsGift *,uint32_t index,char path[FS_GIFT_FILE_PATH_SIZE]);

typedef struct FsGiftRequest {
    uint32_t game;
    uint32_t date; /* Local calendar YYYYMMDD / 本地日历日期。 */
    uint32_t nativeReady; /* Observed native recipient readiness, not a policy decision. */
    uint32_t nativeContentIds[2]; /* Observed Bank receipt IDs / 银行现有领取批次号。 */
    uint32_t region;  /* CFG region; UINT32_MAX when unavailable. / CFG 地区，读取失败为 UINT32_MAX。 */
    uint32_t country; /* Two ASCII country letters, low byte first; 0 unknown. / 两字母国家码，低字节在前。 */
} FsGiftRequest;

typedef struct FsGiftOffer {
    uint32_t index;
    uint32_t length;
    uint32_t nativeId;
    uint32_t inputFormat; /* 1 Bank packet, 2 WC/Full, 3 PL6. / 1 响应正文，2 卡片，3 Link。 */
} FsGiftOffer;

struct FsServer;
int FsGifts_Query(struct FsServer *,const FsGiftRequest *,FsGiftOffer *);
int FsGifts_Prepare(struct FsServer *,const FsGiftRequest *,uint32_t index,
    void *body,uint32_t capacity,FsGiftOffer *);
/* Return a native resource packet, including the native no-new-content case.
   All recipient writes, counts, messages and checksums belong to the client. */
/* 返回原生资源正文，包括无新内容响应；接收写入、计数、提示及存档校验归原客户端。 */
int FsGifts_Reply(struct FsServer *,const FsGiftRequest *,uint32_t pass,
    void *body,uint32_t capacity,uint32_t *size);

#endif
