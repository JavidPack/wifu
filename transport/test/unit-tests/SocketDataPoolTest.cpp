#include "gtest/gtest.h"
#include "ObjectPool.h"
#include "defines.h"
#include "../applib/SocketData.h"

using namespace std;

#define pool ObjectPool<SocketData>::instance()

namespace {

    TEST(SocketDataPoolTest, A) {
        list<SocketData*, gc_allocator<SocketData*> > list_;

        ASSERT_EQ(POOL_INITIAL_SIZE, pool.size());
        ASSERT_EQ(POOL_INITIAL_SIZE, pool.capacity());

        SocketData* a = pool.get();
        list_.push_back(a);

        ASSERT_EQ(POOL_INITIAL_SIZE, pool.capacity());
        ASSERT_EQ(POOL_INITIAL_SIZE - 1, pool.size());

        do {
            SocketData* temp = pool.get();
            ASSERT_NE(a, temp);
            list_.push_back(temp);
        } while (pool.size() > 0);

        ASSERT_EQ(0, pool.size());
        ASSERT_EQ(POOL_INITIAL_SIZE, pool.capacity());

        list_.push_back(pool.get());

        ASSERT_EQ(POOL_INITIAL_SIZE - 1, pool.size());
        ASSERT_EQ(POOL_INITIAL_SIZE * 2, pool.capacity());

        while (!list_.empty()) {
            pool.release(list_.front());
            list_.pop_front();
        }

        ASSERT_EQ(POOL_INITIAL_SIZE * 2, pool.size());
        ASSERT_EQ(POOL_INITIAL_SIZE * 2, pool.capacity());

    }
}
