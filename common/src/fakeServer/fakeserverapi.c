#include "fakeServer/fake_servers.h"

int FakeServerApi_Connect(FsServer *server,uint32_t capacity,uint32_t owner,uint32_t backend,
    const FsStorageIo *storage,const FsTransactionIo *transactions,FsConfigError *error)
{
    if (!server || capacity<sizeof(*server)) return FS_LIMIT;
    /* Backend zero is local storage, not an assertion about the title's mode.
       A remote backend is not implemented by this embedded library. */
    /* 后端零表示本地存储，并非应用模式编号；此嵌入库不实现远端后端。 */
    if (backend!=0) { FsServer_Disconnect(server); return FS_UNSUPPORTED; }
    return FsServer_Open(server,owner,storage,transactions,error);
}

int FsServer_Connected(const FsServer *server)
{
    return server && server->ready && server->transactions;
}
