// WNeuralNet: activation functions (inverse() must undo eval()) and
// WNeuralNetLearner.
#include <gtest/gtest.h>

#include <WClasses/WDom.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WRand.h>
#include <WClasses/WVec.h>

#include <memory>

using namespace WClasses;

// WBlockElPos is exp(x) below 0 and x + 1 from 0 up. Its inverse used to
// assign instead of compare ("y = UNKNOWN_REAL_VALUE ? ..."), so it always
// returned UNKNOWN_REAL_VALUE.
TEST(ActivationTest, ElPosInverseUndoesEval) {
  WBlockElPos block(1);
  for (double x : {-5.0, -1.0, -0.25, 0.0, 0.25, 1.0, 5.0}) {
    SCOPED_TRACE(testing::Message() << "x " << x);
    EXPECT_NEAR(block.inverse(block.eval(x)), x, 1e-12);
  }
  EXPECT_EQ(block.inverse(UNKNOWN_REAL_VALUE), UNKNOWN_REAL_VALUE);
}

// The same round trip for the other activations with an inverse.
TEST(ActivationTest, InversesUndoEval) {
  WBlockIdentity identity(1);
  WBlockTanh tanh(1);
  for (double x : {-2.0, -0.5, 0.0, 0.5, 2.0}) {
    SCOPED_TRACE(testing::Message() << "x " << x);
    EXPECT_NEAR(identity.inverse(identity.eval(x)), x, 1e-12);
    EXPECT_NEAR(tanh.inverse(tanh.eval(x)), x, 1e-9);
  }
}

TEST(NeuralNetLearnerTest, LabelRangeIsTanhs) {
  WNeuralNetLearner learner;
  double lo = 0, hi = 0;
  EXPECT_FALSE(learner.supportedLabelRange(&lo, &hi));
  EXPECT_EQ(lo, -1.0);
  EXPECT_EQ(hi, 1.0);
  learner.nn().add(new WBlockLinear(3, 2));
  lo = hi = 0;
  EXPECT_FALSE(learner.supportedLabelRange(&lo, &hi));
  EXPECT_EQ(lo, -1.0);
  EXPECT_EQ(hi, 1.0);
}

// Copying a layer copied its output and outBlame WVecWrappers with WVec's
// private copy constructor, reachable because WVecWrapper is a friend of
// WVec, and that constructor throws. So WNeuralNet::clone and copyTopology
// threw for any network. A wrapper copy now wraps the same memory.
namespace {
WNeuralNet *smallNet() {
  auto *nn = new WNeuralNet();
  nn->add(new WBlockLinear(3, 4));
  nn->add(new WBlockTanh(4));
  nn->add(new WBlockLinear(4, 2));
  return nn;
}
} // namespace

TEST(NeuralNetCopyTest, CloneAndCopyTopologyGiveTheSameOutputs) {
  WRand rand(0);
  std::unique_ptr<WNeuralNet> nn(smallNet());
  nn->init(rand);
  const WVec input({0.5, -1.0, 2.0});
  WVec expected;
  expected.copy(nn->forwardProp(input));

  std::unique_ptr<WNeuralNet> clone(nn->clone());
  WVec cloneWeights;
  cloneWeights.copy(nn->weights);
  clone->init(cloneWeights);
  const WVec &cloned = clone->forwardProp(input);
  ASSERT_EQ(cloned.size(), 2u);
  EXPECT_EQ(cloned[0], expected[0]);
  EXPECT_EQ(cloned[1], expected[1]);

  WNeuralNet copy;
  copy.copyTopology(*nn);
  WVec copyWeights;
  copyWeights.copy(nn->weights);
  copy.init(copyWeights);
  const WVec &copied = copy.forwardProp(input);
  ASSERT_EQ(copied.size(), 2u);
  EXPECT_EQ(copied[0], expected[0]);
  EXPECT_EQ(copied[1], expected[1]);
  // The original is unchanged by its copies
  EXPECT_EQ(nn->forwardProp(input)[1], expected[1]);
}

// The interpolate demo shows a network as it trains: it copies the topology
// once, inits the copy so it has weights of its own (copyTopology's layers are
// still bound to the original's), then copies the weights across each frame.
TEST(NeuralNetCopyTest, CopiedTopologyHasItsOwnWeightsAfterInit) {
  WRand rand(0);
  std::unique_ptr<WNeuralNet> nn(smallNet());
  nn->init(rand);
  const WVec input({0.5, -1.0, 2.0});

  WNeuralNet copy;
  copy.copyTopology(*nn);
  copy.init(rand);
  ASSERT_NE(copy.weights.data(), nn->weights.data());

  copy.weights.copy(nn->weights);
  WVec expected;
  expected.copy(nn->forwardProp(input));
  EXPECT_EQ(copy.forwardProp(input)[0], expected[0]);
  EXPECT_EQ(copy.forwardProp(input)[1], expected[1]);

  // Training the original (here, changing its weights) leaves the copy as it was
  nn->weights.fill(0.0);
  EXPECT_EQ(copy.forwardProp(input)[0], expected[0]);
  EXPECT_NE(nn->forwardProp(input)[0], expected[0]);
}

// A net saved to a DOM loads back with the same outputs, whether built from
// the node or loaded into an existing net (which uses each layer's
// WBlock::deserializeBlock to make its blocks).
TEST(NeuralNetSerializeTest, LoadsBackWithTheSameOutputs) {
  WRand rand(0);
  std::unique_ptr<WNeuralNet> nn(smallNet());
  nn->init(rand);
  const WVec input({0.5, -1.0, 2.0});
  WVec expected;
  expected.copy(nn->forwardProp(input));

  WDom doc;
  WDomNode *pNode = nn->serialize(&doc);
  WNeuralNet built(pNode, rand);
  const WVec &fromBuilt = built.forwardProp(input);
  ASSERT_EQ(fromBuilt.size(), 2u);
  EXPECT_EQ(fromBuilt[0], expected[0]);
  EXPECT_EQ(fromBuilt[1], expected[1]);

  WNeuralNet loaded;
  loaded.deserialize(pNode, rand);
  const WVec &fromLoaded = loaded.forwardProp(input);
  ASSERT_EQ(fromLoaded.size(), 2u);
  EXPECT_EQ(fromLoaded[0], expected[0]);
  EXPECT_EQ(fromLoaded[1], expected[1]);
}
