#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/DataDependencies.hpp>

#include <gtest/gtest.h>
// This will best be tested by implementing a dummy System subclass 
// This will be used to receive messages and send them back to the Padd for testing messages


// Test sending a simple message from one basic padd to another
// ensure it is received properly and is of the correct type
TEST(MessageTest, TestSendingSimpleMessage) {

}

// Test a message to padd that does not need to respond to it
TEST(MessageTest, TestSendingIgnoredMessage) {

}
