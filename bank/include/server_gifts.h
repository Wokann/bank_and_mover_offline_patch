#ifndef BANK_SERVER_GIFTS_H
#define BANK_SERVER_GIFTS_H

#include "bankdata_redirect.h"
#include "fakeServer/gifts.h"

/* Native recipient state; unknown fields stay opaque. / 原生接收状态的局部视图。 */
typedef struct BankGiftStateView {
    u8 reserved00[8];
    BankFlowView *flow;
    void *allocator;
    u32 substate;
    u8 reserved14[0x28];
    void *resourceObserver;
    void *messageView;
    void *rewardView;
    u32 requestId;
    u32 pass;
    u32 mileage;
    u32 pokemonCount;
    u32 itemCount;
    u8 reserved5C[0x0C];
    u32 cardCount;
} BankGiftStateView;

typedef struct BankGiftRecipientView {
    u32 *vtable;
} BankGiftRecipientView;
typedef struct BankGiftReplyView {
    u8 reserved00[0x14];
    u32 requestId;
} BankGiftReplyView;

typedef void (*BankGift_ReceiveFn)(BankGiftStateView *,BankGiftReplyView *,u32,u8 *);
typedef void *(*BankGift_GetLinkFn)(BankGiftRecipientView *);
#define BankGift_Receive ((BankGift_ReceiveFn)0x002AB17Cu)

typedef union BankGiftGameLayout {
    struct { u8 reserved00[0x1C384]; BankGiftRecipientView link; } gen6;
    struct { u8 reserved00[0xAD43C]; u8 cards[1]; } gen7;
} BankGiftGameLayout;

typedef struct BankGiftObserverView { u32 *vtable; } BankGiftObserverView;
typedef struct BankBossResourceView {
    u8 reserved00[4];
    void *allocator;
    u8 reserved08[0x450];
    u8 *body;
    u8 reserved45C[0x44];
    BankGiftObserverView *observers[16];
} BankBossResourceView;
#define BankGift_BossStorage ((BankBossResourceView **)0x003AB940u)
int ServerGifts_Request(BankBossResourceView *,u32,const char *,u32);
int ServerGifts_Update(BankGiftStateView *);
int ServerGifts_OriginalUpdate(BankGiftStateView *);

#endif
