/*
  The contents of this file are dedicated by all of its authors, including

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
#include <WClasses/GApp.h>
#include <WClasses/GError.h>
#include <WClasses/GMatrix.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GOptimizer.h>
#include <WClasses/GRand.h>
#include <WClasses/GVec.h>
#include "series_networks/LinearWeights.h"
#include "series_networks/SeriesPlot.h"
#include <math.h>
#include <cstdlib>
#include <cstring>
#include <string>

// SAKBox: ported from the 2015 neural network API (GLayerClassic,
// GLayerMixed, GActivation*), which upstream removed without updating this
// demo, to the current block API, as fourier_net was: the mixed hidden layer
// is one GBlockLinear holding all its units' weights, followed by sine,
// softplus and identity blocks side by side. Progress is drawn to
// progress.svg where it is run (it used to open Firefox on it).

using namespace GClasses;
using std::cerr;
using std::cout;
using std::string;

#define SINE_NODES 64
#define SOFTPLUS_NODES 2
#define IDENTITY_NODES 4
#define SOFTPLUS_SHIFT 5
#define PERTURBATION 1e-4
#define TRAINING_EPOCHS 50000
#define REGULARIZATION_TERM 0.01
#define LEARNING_RATE 1e-3

namespace
{

// Where data is read from (under bazel run) and the picture is written to
string g_dataDir = "projects/waffles/demos/time_series/bin";
string g_outDir = ".";

double rmse(GNeuralNet& nn, const GMatrix& features, const GMatrix& labels)
{
	return sqrt(nn.measureLoss(features, labels) / labels.rows());
}

} // namespace

void doit(size_t epochs)
{
	// Load the data
	GMatrix trainLab;
	GMatrix testLab;
	trainLab.loadArff((g_dataDir + "/train.arff").c_str());
	testLab.loadArff((g_dataDir + "/test.arff").c_str());
	double dataMin = trainLab.columnMin(0);
	double dataMax = trainLab.columnMax(0);
	trainLab.normalizeColumn(0, dataMin, dataMax, -5.0, 5.0);
	testLab.normalizeColumn(0, dataMin, dataMax, -5.0, 5.0);
	GMatrix trainFeat(trainLab.rows(), 1);
	for(size_t i = 0; i < trainLab.rows(); i++)
		trainFeat[i][0] = (double)i / trainLab.rows() - 0.5;
	GMatrix testFeat(testLab.rows(), 1);
	for(size_t i = 0; i < testLab.rows(); i++)
		testFeat[i][0] = (double)(i + trainLab.rows()) / trainLab.rows() - 0.5;

	// Make a neural network: a hidden layer of sine, softplus and identity
	// units, and an identity output
	const size_t sn = SINE_NODES, sp = SOFTPLUS_NODES, id = IDENTITY_NODES;
	GNeuralNet nn;
	nn.add(new GBlockLinear(1, sn + sp + id));
	nn.add(new GBlockSine(sn));
	nn.concat(new GBlockSoftPlus(sp), sn);
	nn.concat(new GBlockIdentity(id), sn + sp);
	nn.add(new GBlockLinear(sn + sp + id, trainLab.cols()));
	GRand rand(0);
	GSGDOptimizer optimizer(nn, rand); // allocates nn's weights
	optimizer.setLearningRate(LEARNING_RATE);
	LinearWeights hidden{nn.layer(0).block(0)};
	LinearWeights output{nn.layer(2).block(0)};

	// Initialize the weights of the sine units to match the frequencies used by the Fourier transform.
	for(size_t i = 0; i < sn / 2; i++)
	{
		hidden.w(0, 2 * i) = 2.0 * M_PI * (i + 1);
		hidden.bias(2 * i) = 0.5 * M_PI;
		hidden.w(0, 2 * i + 1) = 2.0 * M_PI * (i + 1);
		hidden.bias(2 * i + 1) = M_PI;
	}

	// Initialize all the non-periodic nodes to approximate the identity function, then perturb a little bit
	hidden.setIdentity(sn, sp);
	for(size_t i = 0; i < sp; i++)
	{
		hidden.bias(sn + i) += SOFTPLUS_SHIFT;
		output.renormalizeInput(sn + i, 0.0, 1.0, SOFTPLUS_SHIFT, SOFTPLUS_SHIFT + 1.0);
	}
	hidden.setIdentity(sn + sp, id);
	hidden.perturb(rand, PERTURBATION, sn, sp);
	hidden.perturb(rand, PERTURBATION, sn + sp, id);

	// Randomly initialize the weights on the output layer
	for(size_t i = 0; i < output.block.weights.size(); i++)
		output.block.weights[i] = 0.0;
	output.perturb(rand, PERTURBATION, 0, output.outputs());

	const string progress = g_outDir + "/progress.svg";
	cout << "Progress is drawn to " << progress << " (open it in a browser and reload).\n";

	// Do some training
	GRandomIndexIterator ii(trainLab.rows(), rand);
	for(size_t epoch = 0; epoch < epochs; epoch++)
	{
		// Visit each sample in random order
		ii.reset();
		size_t i;
		while(ii.next(i))
		{
			// Regularize
			output.regularize(LEARNING_RATE * REGULARIZATION_TERM, LEARNING_RATE * REGULARIZATION_TERM, 0, output.outputs());

			// Train
			optimizer.optimizeIncremental(trainFeat[i], trainLab[i]); // One iteration of stochastic gradient descent
		}

		// Report progress
		if(epoch % std::max<size_t>(epochs / 100, 1) == 0 || epoch + 1 == epochs)
		{
			cout << "prog=" << to_str((double)(epoch + 1) * 100.0 / epochs) << "%	rmse=" << to_str(rmse(nn, trainFeat, trainLab)) << "	val=" << to_str(rmse(nn, testFeat, testLab)) << "\n";
			plotSeries(progress, nn, trainFeat, trainLab, testFeat, testLab);
		}
	}
}

// Usage: time_series [--epochs N] [--data DIR]
// Trains for 50,000 epochs unless --epochs is given. Reads train.arff and
// test.arff from DIR, and draws progress.svg where it is run.
int main(int argc, char* argv[])
{
#ifdef _DEBUG
	GApp::enableFloatingPointExceptions();
#endif
	int nRet = 0;
	try
	{
		size_t epochs = TRAINING_EPOCHS;
		for(int i = 1; i < argc; i++)
		{
			if(strcmp(argv[i], "--epochs") == 0 && i + 1 < argc)
				epochs = (size_t)strtoul(argv[++i], nullptr, 10);
			else if(strcmp(argv[i], "--data") == 0 && i + 1 < argc)
				g_dataDir = argv[++i];
			else
				throw Ex("Usage: time_series [--epochs N] [--data DIR]");
		}
		if(const char* dir = getenv("BUILD_WORKING_DIRECTORY"))
			g_outDir = dir;
		doit(epochs);
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}
