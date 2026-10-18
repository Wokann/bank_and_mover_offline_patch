#include "fakeServer/fake_servers.h"
#include "fakeServer/numbers.h"

#define UNLOCK_MODULUS 100000000u

static int sameDescriptor(const FsTransactionParameters *a,const FsTransactionParameters *b)
{
    return a->dataId==b->dataId && a->token==b->token && a->size==b->size &&
        a->currentVersion==b->currentVersion && a->updateVersion==b->updateVersion;
}

static int pending(const FsTransactionRecord *record)
{
    return record->operation==FS_TRANSACTION_SOURCE_SAVE && record->serverTransaction.token &&
        (record->phase==FS_TRANSACTION_SOURCE_PENDING || record->phase==FS_TRANSACTION_STAGED);
}

int FsServer_Support(FsServer *server,uint32_t challenge,FsSupportResponse *out)
{
    if (!FsServer_Connected(server) || !out || challenge>=UNLOCK_MODULUS) return FS_INVALID;
    FsTransactionRecord record;
    int status=FsTransaction_ReadRecord(server->transactions,&record);
    if (status!=FS_TRANSACTION_PRESENT || !pending(&record)) {
        server->rollbackCodes.candidateCount=0; server->rollbackAuthorized=0;
        return status==FS_TRANSACTION_MISSING || status==FS_TRANSACTION_PRESENT?
            FS_REMOTE_NOT_FOUND:FS_IO_ERROR;
    }
    if (server->rollbackCodes.candidateCount==3 &&
        server->rollbackCodes.challenge==challenge && server->rollbackTransaction==record.transaction &&
        sameDescriptor(&server->rollbackDescriptor,&record.serverTransaction)) {
        *out=server->rollbackCodes;
        return FS_OK;
    }
    server->rollbackCodes.candidateCount=0; server->rollbackAuthorized=0;
    if (!FsTransaction_CanRollback(server->transactions,&record,1)) return FS_LOCKED;
    /* Codes are random, not file fingerprints. The cached challenge and
       descriptor below bind authorization to the exact active reservation. */
    /* 密码继续随机生成，不作为文件摘要；缓存的挑战和描述符绑定当前预留。 */
    uint32_t entropy[3];
    if (!server->transactions->randomBytes ||
        !server->transactions->randomBytes(entropy,sizeof(entropy))) return FS_IO_ERROR;
    out->challenge=challenge; out->candidateCount=3;
    for (uint32_t i=0;i<3;i++) {
        uint32_t code;
        FsDivideU32(entropy[i],UNLOCK_MODULUS,&code);
        for (uint32_t j=0;j<i;) {
            if (code==out->candidates[j]) { code=code+1==UNLOCK_MODULUS?0:code+1; j=0; }
            else j++;
        }
        out->candidates[i]=code;
    }
    server->rollbackCodes=*out;
    server->rollbackDescriptor=record.serverTransaction;
    server->rollbackTransaction=record.transaction;
    return FS_OK;
}

int FsServer_AuthorizeRollback(FsServer *server,uint32_t challenge,uint32_t enteredCode)
{
    if (!FsServer_Connected(server)) return FS_INVALID;
    server->rollbackAuthorized=0;
    if (enteredCode>=UNLOCK_MODULUS || server->rollbackCodes.candidateCount!=3 ||
        challenge!=server->rollbackCodes.challenge) return FS_TRANSACTION_MISMATCH;
    uint32_t accepted=0;
    for (uint32_t i=0;i<3;i++)
        if (enteredCode==server->rollbackCodes.candidates[i]) accepted=1;
    if (!accepted) return FS_TRANSACTION_MISMATCH;
    FsTransactionRecord record;
    if (FsTransaction_ReadRecord(server->transactions,&record)!=FS_TRANSACTION_PRESENT)
        return FS_IO_ERROR;
    if (!pending(&record) || server->rollbackTransaction!=record.transaction ||
        !sameDescriptor(&server->rollbackDescriptor,&record.serverTransaction))
        return FS_TRANSACTION_MISMATCH;
    /* Native keyboard validation precedes this API, but the service still
       authorizes only the challenge and reservation it actually issued. */
    /* 原键盘先验证输入；服务仍仅授权自己签发的挑战和对应预留。 */
    server->rollbackAuthorized=1;
    return FS_OK;
}
