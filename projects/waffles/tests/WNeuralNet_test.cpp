// GNeuralNet: activation functions (inverse() must undo eval()) and
// GNeuralNetLearner.
#include <gtest/gtest.h>

#include <WClasses/GDom.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GRand.h>
#include <WClasses/GVec.h>

#include <memory>

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

// Copying a layer copied its output and outBlame GVecWrappers with GVec's
// private copy constructor, reachable because GVecWrapper is a friend of
// GVec, and that constructor throws. So GNeuralNet::clone and copyTopology
// threw for any network. A wrapper copy now wraps the same memory.
namespace {
GNeuralNet *smallNet() {
  auto *nn = new GNeuralNet();
  nn->add(new GBlockLinear(3, 4));
  nn->add(new GBlockTanh(4));
  nn->add(new GBlockLinear(4, 2));
  return nn;
}
} // namespace

TEST(NeuralNetCopyTest, CloneAndCopyTopologyGiveTheSameOutputs) {
  GRand rand(0);
  std::unique_ptr<GNeuralNet> nn(smallNet());
  nn->init(rand);
  const GVec input({0.5, -1.0, 2.0});
  GVec expected;
  expected.copy(nn->forwardProp(input));

  std::unique_ptr<GNeuralNet> clone(nn->clone());
  GVec cloneWeights;
  cloneWeights.copy(nn->weights);
  clone->init(cloneWeights);
  const GVec &cloned = clone->forwardProp(input);
  ASSERT_EQ(cloned.size(), 2u);
  EXPECT_EQ(cloned[0], expected[0]);
  EXPECT_EQ(cloned[1], expected[1]);

  GNeuralNet copy;
  copy.copyTopology(*nn);
  GVec copyWeights;
  copyWeights.copy(nn->weights);
  copy.init(copyWeights);
  const GVec &copied = copy.forwardProp(input);
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
  GRand rand(0);
  std::unique_ptr<GNeuralNet> nn(smallNet());
  nn->init(rand);
  const GVec input({0.5, -1.0, 2.0});

  GNeuralNet copy;
  copy.copyTopology(*nn);
  copy.init(rand);
  ASSERT_NE(copy.weights.data(), nn->weights.data());

  copy.weights.copy(nn->weights);
  GVec expected;
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
// GBlock::deserializeBlock to make its blocks).
TEST(NeuralNetSerializeTest, LoadsBackWithTheSameOutputs) {
  GRand rand(0);
  std::unique_ptr<GNeuralNet> nn(smallNet());
  nn->init(rand);
  const GVec input({0.5, -1.0, 2.0});
  GVec expected;
  expected.copy(nn->forwardProp(input));

  GDom doc;
  GDomNode *pNode = nn->serialize(&doc);
  GNeuralNet built(pNode, rand);
  const GVec &fromBuilt = built.forwardProp(input);
  ASSERT_EQ(fromBuilt.size(), 2u);
  EXPECT_EQ(fromBuilt[0], expected[0]);
  EXPECT_EQ(fromBuilt[1], expected[1]);

  GNeuralNet loaded;
  loaded.deserialize(pNode, rand);
  const GVec &fromLoaded = loaded.forwardProp(input);
  ASSERT_EQ(fromLoaded.size(), 2u);
  EXPECT_EQ(fromLoaded[0], expected[0]);
  EXPECT_EQ(fromLoaded[1], expected[1]);
}
