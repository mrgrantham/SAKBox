/*
  The contents of this file are dedicated by all of its authors, including

    Stephen C. Ashmore,
    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or pay it forward in their own field. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include <exception>
#include <iostream>
#include <vector>
#include <WClasses/WApp.h>
#include <WClasses/WError.h>
#include <WClasses/WDecisionTree.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WKNN.h>
#include <WClasses/WNaiveBayes.h>
#include <WClasses/WEnsemble.h>

using namespace WClasses;
using std::cerr;
using std::cout;
using std::vector;
using std::endl;

// SAKBox: ported from the old layer API (WLayerClassic and friends, removed
// upstream) to blocks. A classic layer is a WBlockLinear followed by its
// activation (tanh unless another was given), and layer sizes are explicit:
// 784 pixels in, and 10 outputs, one per digit, which WAutoFilter makes of
// the nominal label. WNeuralNetLearner::train uses its own SGD optimizer, so
// the old learning rate (0.01) and momentum (0.1) became its defaults (0.05
// and none). The R-Prop and dropout examples, already switched off below,
// used training methods that no longer exist and are gone.
const size_t kPixels = 28 * 28;
const size_t kDigits = 10;

void predictWith(WNeuralNetLearner* pNN, WMatrix& features, WMatrix& labels, WVec& test_features, WVec& predicted_labels)
{
	WAutoFilter af(pNN);
	af.train(features, labels);
	af.predict(test_features, predicted_labels);
}

void do_fullyconnected(WMatrix& features, WMatrix& labels, WVec& test_features, WVec& predicted_labels)
{
	WNeuralNetLearner* pNN = new WNeuralNetLearner();
	pNN->nn().add(new WBlockLinear(kPixels, 100), new WBlockTanh(100));
	pNN->nn().add(new WBlockLinear(100, 40), new WBlockTanh(40));
	pNN->nn().add(new WBlockLinear(40, kDigits), new WBlockTanh(kDigits));
	predictWith(pNN, features, labels, test_features, predicted_labels);
}

void do_convolutional(WMatrix& features, WMatrix& labels, WVec& test_features, WVec& predicted_labels)
{
	// 8 filters of 5x5 (the old layer's were 4x4), each giving a 28x28 map
	const size_t maps = 28 * 28 * 8;
	WNeuralNetLearner* pNN = new WNeuralNetLearner();
	pNN->nn().add(new WBlockConv({28, 28}, {5, 5, 8}, {28, 28, 8}), new WBlockBentIdentity(maps));
	pNN->nn().add(new WBlockLinear(maps, 80), new WBlockRectifier(80));
	pNN->nn().add(new WBlockLinear(80, kDigits), new WBlockBentIdentity(kDigits));
	predictWith(pNN, features, labels, test_features, predicted_labels);
}

void do_autoencoder(WMatrix& features, WMatrix& labels, WVec& test_features, size_t& countMissedPixels)
{
	const size_t sizes[] = {kPixels, 100, 40, 20, 40, 100, kPixels};
	WNeuralNetLearner nn;
	for(size_t i = 0; i + 1 < std::size(sizes); i++)
		nn.nn().add(new WBlockLinear(sizes[i], sizes[i + 1]), new WBlockTanh(sizes[i + 1]));

	// SAKBox: trained on pixels scaled to 0..1 (tanh can't output 255, so
	// nearly every pixel used to count as missed)
	WMatrix scaled(features);
	scaled.multiply(1.0 / 255.0);
	WUniformRelation pixels(kPixels);
	nn.beginIncrementalLearning(pixels, pixels);
	WNeuralNetOptimizer& optimizer = nn.optimizer();
	WRandomIndexIterator iter(scaled.rows(), nn.rand());
	size_t index;
	for(size_t epoch = 0; epoch < 600; epoch++)
	{
		iter.reset();
		while(iter.next(index))
			optimizer.optimizeIncremental(scaled[index], scaled[index]);
	}

	WVec scaledTest;
	scaledTest.copy(test_features);
	scaledTest *= 1.0 / 255.0;
	WVec predicted_feature(test_features.size());
	nn.predict(scaledTest, predicted_feature);

	countMissedPixels = 0;
	for(size_t i = 0; i < test_features.size(); i++)
	{
		double miss = test_features[i] - predicted_feature[i] * 255.0;
		if(!(miss < 20 && miss > -20))
			countMissedPixels++;
	}
}

void do_bendactivation(WMatrix& features, WMatrix& labels, WVec& test_features, WVec& predicted_labels)
{
	WNeuralNetLearner* pNN = new WNeuralNetLearner();
	pNN->nn().add(new WBlockLinear(kPixels, 100), new WBlockBentIdentity(100));
	pNN->nn().add(new WBlockLinear(100, 40), new WBlockBentIdentity(40));
	pNN->nn().add(new WBlockLinear(40, kDigits), new WBlockBentIdentity(kDigits));
	predictWith(pNN, features, labels, test_features, predicted_labels);
}

void doit()
{
	WMatrix train;
	train.loadArff("projects/waffles/demos/neural_nets/mnist_train.arff"); // SAKBox: where `bazel run` puts it
	WDataColSplitter sTrain(train, 1);

	WMatrix& features = sTrain.features();
	WMatrix& labels = sTrain.labels();

	WMatrix test;
	test.loadArff("projects/waffles/demos/neural_nets/mnist_test.arff");

	// Make a test vector
	WVec test_features(784), predicted_feature(784);
	WVec predicted_labels(1), correctLabel(1);
	test_features.copy(0, test[0], 0, test.cols() - 1);
	correctLabel.copy(0, test[0], test.cols() - 1, 1);

	cout << "This demo trains and tests several different neural network architectures using an extremely small subset of the MNIST hand-written digits dataset. Each feature example is a grey-scale picture of a hand-written digit, like the number 4 etc." << endl;

	// Use several models to make predictions
	do_fullyconnected(features, labels, test_features, predicted_labels);
	cout << "A typical neural net consists of layers of fully-connected neurons. This example has "
	     << "a two layer network." << endl;
	cout << "It predicted a: " << predicted_labels[0] << " where " << correctLabel[0] << " was correct." << endl;

	do_bendactivation(features, labels, test_features, predicted_labels);
	cout << "You can also replace what activation function is used on the neurons. This allows greater flexibility "
	     << "to fit some data, and can improve accuracy and performance in some cases. Waffles supports a large "
	     << "set of various activation functions. Here we try the Bent Identity function on each of our layers. "
	     << "Note: Waffles supports mixed layers of various activation functions." << endl;
	cout << "It predicted a: " << predicted_labels[0] << " where " << correctLabel[0] << " was correct." << endl;

	do_convolutional(features, labels, test_features, predicted_labels);
	cout << "Waffles also supports convolution, which excels at visual tasks. This example is a very "
	     << "simple convolutional network with one layer of convolutional neurons." << endl;
	cout << "It predicted a: " << predicted_labels[0] << " where " << correctLabel[0] << " was correct." << endl;

	size_t countMissedPixels = 0;
	do_autoencoder(features, labels, test_features, countMissedPixels);
	cout << "Autoencoders are a bit different, rather than try to predict the label autoencoders "
	     << "typically try to reproduce the features by first reducing the feature space to a smaller dimension."
	     << endl;
	cout << "Having only seen 35 example images of a handwritten digit, this simple autoencoder only missed: " << endl;
	cout << countMissedPixels << " out of " << test_features.size() << " possible pixels." << endl;
}

int main(int argc, char* argv[])
{
#ifdef _DEBUG
	WApp::enableFloatingPointExceptions();
#endif
	int nRet = 0;
	try
	{
		doit();
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}
