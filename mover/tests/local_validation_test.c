/* Host-only harness for the production classifier; never imported by armips. */
/* 仅在主机测试生产分类器的入口，不会被 armips 导入补丁。 */
#include "local_validation.h"

static const u8 *testBlockOrder;
static const MoverGen5RecordView *testRecords;
static MoverGameManagerView *testManager;

static void *testCandidateBuffer(void *gameData)
{
    (void)gameData;
    return (void *)testRecords;
}

#undef MOVER_RUNTIME_SINGLETON
#undef MOVER_GET_GEN5_CANDIDATE_BUFFER
#undef MoverGen5_BlockOrderTable
#define MOVER_RUNTIME_SINGLETON (&testManager)
#define MOVER_GET_GEN5_CANDIDATE_BUFFER testCandidateBuffer
#define MoverGen5_BlockOrderTable testBlockOrder

#include "../src/local_validation.c"

#ifdef _WIN32
#define TEST_API __declspec(dllexport)
#else
#define TEST_API __attribute__((visibility("default")))
#endif

TEST_API void testSetBlockOrder(const u8 *table)
{
    testBlockOrder=table;
}

TEST_API unsigned int testRecordSize(void)
{
    return sizeof(MoverGen5RecordView);
}

TEST_API int testRecord(const void *record)
{
    return gen5RecordValidationResult((const MoverGen5RecordView *)record);
}

TEST_API int testBox(const void *records,int missing,s32 *results,u32 *substate)
{
    MoverStateView state={0};
    MoverGameManagerView manager={0};
    MoverValidationSharedView shared={0};
    u32 index;
    int result;

    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        shared.result[index]=250+(s32)index;
    state.sharedData=missing==4?0:&shared;
    state.substate=2;
    manager.gameData=missing==2?0:&manager;
    testManager=missing==1?0:&manager;
    testRecords=missing==3?0:(const MoverGen5RecordView *)records;
    result=OfflinePatch_PrepareGen5Validation(missing==5?0:&state);
    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        results[index]=shared.result[index];
    *substate=state.substate;
    return result;
}

TEST_API void testVc(s32 *results)
{
    MoverStateView state={0};
    MoverValidationSharedView shared={0};
    u32 index;
    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        shared.result[index]=250+(s32)index;
    state.sharedData=&shared;
    OfflinePatch_PrepareGen12Validation(&state);
    for (index=0;index<MOVER_VALIDATION_SLOT_COUNT;index++)
        results[index]=shared.result[index];
}
