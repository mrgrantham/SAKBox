// GNeuralNet: activation functions (inverse() must undo eval()) and
// GNeuralNetLearner.
#include <gtest/gtest.h>

#include <WClasses/GNeuralNet.h>

using namespace GClasses;

// GBlockElPos is exp(x) below 0 and x + 1 from 0 up. Its inverse used to
// assign instead of compare ("y = UNKNOWN_REAL_VALUE ? ..."), so it always
// returned UNKNOWN_REAL_VALUE.
TEST(ActivationTest, ElPosInverseUndoesEval) {
  GBlockElPos block(1);
  for (double x : {-5.0, -1.0, -0.25, 0.0, 0.25, 1.0, 5.0}) {
    SCOPED_TRACE(testing::Message() << "x " << x);
    EXPECT_NEAR(block.inverse(block.eval(x)), x, 1e-12);
  }
  EXPECT_EQ(block.inverse(UNKNOWN_REAL_VALUE), UNKNOWN_REAL_VALUE);
}

// The same round trip for the other activations with an inverse.
TEST(ActivationTest, InversesUndoEval) {
  GBlockIdentity identity(1);
  GBlockTanh tanh(1);
  for (double x : {-2.0, -0.5, 0.0, 0.5, 2.0}) {
    SCOPED_TRACE(testing::Message() << "x " << x);
    EXPECT_NEAR(identity.inverse(identity.eval(x)), x, 1e-12);
    EXPECT_NEAR(tanh.inverse(tanh.eval(x)), x, 1e-9);
  }
}

TEST(NeuralNetLearnerTest, LabelRangeIsTanhs) {
  GNeuralNetLearner learner;
  double lo = 0, hi = 0;
  EXPECT_FALSE(learner.supportedLabelRange(&lo, &hi));
  EXPECT_EQ(lo, -1.0);
  EXPECT_EQ(hi, 1.0);
  learner.nn().add(new GBlockLinear(3, 2));
  lo = hi = 0;
  EXPECT_FALSE(learner.supportedLabelRange(&lo, &hi));
  EXPECT_EQ(lo, -1.0);
  EXPECT_EQ(hi, 1.0);
}
