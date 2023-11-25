#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

// Demonstrate implementing shouldUseMainThread() to return true for all padds makes a padd run in main thread event loop
TEST(ThreadingTest, TestAllMainThread) {

}

// Demonstrate implementing shouldUseMainThread() to return false for all padds makes a padd run in main thread event loop
TEST(ThreadingTest, TestAllBackgroundThreads) {
    
}

// Demonstrate implementing shouldUseMainThread() to differently for different padds makes has the correct behavior for each padd
TEST(ThreadingTest,TesMixedlBackgroundAndMainThreads) {

}