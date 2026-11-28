// Tests of the thread local storage of the Glide context on Windows.
//
// Every Glide function gets its context with getThreadValueFast(). On 32-bit
// Windows it reads the TLS slot directly from the thread environment block
// (TEB) with inline assembly, instead of calling TlsGetValue(). If it reads
// the wrong address, every Glide function works on a garbage context, and
// the game fails in grSstWinOpen or grGlideShutdown.

#include <gtest/gtest.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

#include "thread_storage_access.h"

namespace {

class ThreadStorage : public ::testing::Test {
protected:
    void SetUp() override { tsaInitThreadStorage(); }
    void TearDown() override { tsaFreeThreadStorage(); }
};

// Contexts, filled with a pattern that is no valid pointer, so that a read
// from a wrong address within them does not return the context.
struct FakeContext {
    std::uintptr_t words[64];

    FakeContext()
    {
        for (auto &word : words) {
            word = 0xDEADBEEF;
        }
    }
};

TEST_F(ThreadStorage, FastReadReturnsTheValueSetByGlide)
{
    FakeContext context;
    tsaSetThreadValue(&context);

    EXPECT_EQ(tsaGetThreadValueFast(), &context);
}

TEST_F(ThreadStorage, FastReadReturnsTheValueOfTlsGetValue)
{
    FakeContext first, second;

    TlsSetValue(tsaTlsIndex(), &first);
    EXPECT_EQ(tsaGetThreadValueFast(), &first);

    TlsSetValue(tsaTlsIndex(), &second);
    EXPECT_EQ(tsaGetThreadValueFast(), &second);
    EXPECT_EQ(TlsGetValue(tsaTlsIndex()), &second);
}

struct ThreadResult {
    FakeContext context;
    void *readBeforeSet;
    void *readAfterSet;
};

DWORD WINAPI otherThread(LPVOID parameter)
{
    auto *result = static_cast<ThreadResult *>(parameter);
    result->readBeforeSet = tsaGetThreadValueFast();
    tsaSetThreadValue(&result->context);
    result->readAfterSet = tsaGetThreadValueFast();
    return 0;
}

TEST_F(ThreadStorage, EachThreadHasItsOwnValue)
{
    FakeContext mainContext;
    tsaSetThreadValue(&mainContext);

    ThreadResult result;
    HANDLE thread = CreateThread(nullptr, 0, otherThread, &result, 0, nullptr);
    ASSERT_NE(thread, nullptr);
    ASSERT_EQ(WaitForSingleObject(thread, 10000), WAIT_OBJECT_0);
    CloseHandle(thread);

    EXPECT_EQ(result.readBeforeSet, nullptr);
    EXPECT_EQ(result.readAfterSet, &result.context);
    EXPECT_EQ(tsaGetThreadValueFast(), &mainContext);
}

} // namespace
