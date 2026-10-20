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
#include <WClasses/GFourier.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GOptimizer.h>
#include <WClasses/GPlot.h>
#include <WClasses/GBits.h>
#include <WClasses/GVec.h>
#include <WClasses/GTime.h>
#include <WClasses/GRand.h>
#include <math.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// SAKBox: ported from the 2015 neural network API (GLayerClassic,
// GLayerMixed, GActivation*, GNeuralNet::fourier), which upstream removed in
// 2016-2019 without updating this demo, to the current block API:
//  - a GLayerMixed of classic layers becomes a GBlockLinear holding all of
//    their weights, followed by a layer of activation blocks side by side;
//  - GNeuralNet::fourier is reimplemented below from upstream's last version
//    of it (2017, before it was deleted in 9628b7f);
//  - the network's weights live in one vector, so the two learners at
//    different learning rates become one network trained from a backup.

using namespace GClasses;
using std::cerr;
using std::cout;
using std::string;

#define SOFTPLUS_NODES 2
#define IDENTITY_NODES 2
#define SOFTPLUS_SHIFT 10
#define PERTURBATION 1e-4
#define TIGHTNESS_GOOD 0.05
#define TIGHTNESS_BAD 0.25

namespace
{

// Where data is read from and pictures are written to
string g_dataDir = "projects/waffles/demos/fourier_net/data"; // under bazel run
string g_outDir = ".";

string outPath(const char* name) { return g_outDir + "/" + name; }

// One fully connected linear block's weights: biases first, then one row of
// output weights per input (GBlockLinear's layout).
struct Linear
{
	GBlock& block;
	double& bias(size_t out) { return block.weights[out]; }
	double& w(size_t in, size_t out) { return block.weights[block.outputs() * (1 + in) + out]; }
	size_t inputs() const { return block.inputs(); }
	size_t outputs() const { return block.outputs(); }

	// The old GLayerClassic::setWeightsToIdentity, for outputs [start, start + count)
	void setIdentity(size_t start, size_t count)
	{
		for(size_t o = start; o < start + count; o++)
		{
			bias(o) = 0.0;
			for(size_t i = 0; i < inputs(); i++)
				w(i, o) = (i == o - start ? 1.0 : 0.0);
		}
	}

	// GLayerClassic::perturbWeights, for outputs [start, start + count)
	void perturb(GRand& rand, double deviation, size_t start, size_t count)
	{
		for(size_t o = start; o < start + count; o++)
		{
			bias(o) += deviation * rand.normal();
			for(size_t i = 0; i < inputs(); i++)
				w(i, o) += deviation * rand.normal();
		}
	}

	// GLayerClassic::scaleWeights then diminishWeights (L2 then L1, biases
	// included), for outputs [start, start + count)
	void regularize(double l2, double l1, size_t start, size_t count)
	{
		auto shrink = [l2, l1](double& x)
		{
			x *= 1.0 - l2;
			x = x > l1 ? x - l1 : (x < -l1 ? x + l1 : 0.0);
		};
		for(size_t o = start; o < start + count; o++)
		{
			shrink(bias(o));
			for(size_t i = 0; i < inputs(); i++)
				shrink(w(i, o));
		}
	}

	// GLayerClassic::renormalizeInput: input `in` used to range over
	// [oldMin, oldMax] and now ranges over [newMin, newMax]
	void renormalizeInput(size_t in, double oldMin, double oldMax, double newMin, double newMax)
	{
		double f = (oldMax - oldMin) / (newMax - newMin);
		double g = oldMin - newMin * f;
		for(size_t o = 0; o < outputs(); o++)
		{
			bias(o) += w(in, o) * g;
			w(in, o) *= f;
		}
	}
};

// The deviation GLayerClassic::resize gave new weights: `deviation` scaled by
// the RMS of the weights kept, and shrunk when the new area is the larger.
double resizeDeviation(const std::vector<double>& kept, size_t newArea, double deviation)
{
	if(kept.size() >= 8)
	{
		double d = 0.0;
		for(double x : kept)
			d += x * x;
		deviation *= sqrt(d / kept.size());
		if(newArea > kept.size())
			deviation *= (double)kept.size() / newArea;
	}
	return deviation;
}

// A Fourier network for `series` (one column, rows a power of 2) whose rows
// span `period` units of time: sine units at the FFT's frequencies, combined
// by the FFT's coefficients. Returns, per sine unit, its frequency and phase,
// and the output's weights and bias. (Upstream's GNeuralNet::fourier, 2017,
// for a single column.)
struct FourierFit
{
	std::vector<double> freq, phase, outWeight;
	double outBias = 0.0;
};

FourierFit fourierFit(const GMatrix& series, double period)
{
	size_t n = series.rows();
	if(!GBits::isPowerOfTwo((unsigned int)n))
		throw Ex("The training series must have a power-of-2 number of rows");
	FourierFit fit;
	fit.freq.resize(n);
	fit.phase.resize(n);
	fit.outWeight.resize(n);
	for(size_t i = 0; i < n / 2; i++)
	{
		fit.freq[2 * i] = 2.0 * M_PI * (i + 1) / period;
		fit.phase[2 * i] = 0.5 * M_PI;
		fit.freq[2 * i + 1] = 2.0 * M_PI * (i + 1) / period;
		fit.phase[2 * i + 1] = M_PI;
	}
	std::vector<ComplexNumber> f(n);
	for(size_t i = 0; i < n; i++)
	{
		f[i].real = series[i][0];
		f[i].imag = 0.0;
	}
	GFourier::fft(n, f.data(), true);
	const size_t halfN = n / 2; // exact: n is a power of 2 (checked above)
	for(size_t i = 0; i < halfN; i++)
	{
		fit.outWeight[2 * i] = f[1 + i].real / (double)halfN;
		fit.outWeight[2 * i + 1] = f[1 + i].imag / (double)halfN;
	}
	fit.outBias = f[0].real / n;
	// The FFT doubles up the values in the last complex element
	fit.outWeight[n - 2] *= 0.5;
	fit.outWeight[n - 1] *= 0.5;
	return fit;
}

double rmse(GNeuralNet& nn, const GMatrix& features, const GMatrix& labels)
{
	return sqrt(nn.measureLoss(features, labels) / labels.rows());
}

void plot_it(const char* filename, GNeuralNet& nn, GMatrix& trainFeat, GMatrix& trainLab, GMatrix& testFeat, GMatrix& testLab)
{
	GSVG svg(1000, 500);
	double xmin = trainFeat[0][0];
	double xmax = testFeat[testFeat.rows() - 1][0];
	svg.newChart(xmin, std::min(trainLab.columnMin(0), testLab.columnMin(0)), xmax, std::max(trainLab.columnMax(0), testLab.columnMax(0)));
	svg.horizMarks(20);
	svg.vertMarks(20);
	double prevx = xmin;
	double prevy = 0.0;
	double step = (xmax - xmin) / 500.0;
	GVec x(1);
	for(x[0] = prevx; x[0] < xmax; x[0] += step)
	{
		double y = nn.forwardProp(x)[0];
		if(prevx != x[0])
			svg.line(prevx, prevy, x[0], y, 0.3);
		prevx = x[0];
		prevy = y;
	}
	for(size_t i = 0; i < trainLab.rows(); i++)
		svg.dot(trainFeat[i][0], trainLab[i][0], 0.4, 0xff000080);
	for(size_t i = 0; i < testLab.rows(); i++)
		svg.dot(testFeat[i][0], testLab[i][0], 0.4, 0xff800000);

	std::ofstream ofs;
	ofs.open(outPath(filename));
	svg.print(ofs);
}

void doit(size_t maxEpochs)
{
	cout << "\n\nThis demo is described at http://arxiv.org/abs/1405.2262\n\n";

	cout << "For efficiency reasons, this demo differs from the paper the following ways:\n";
	cout << " * It uses one fewer layers.\n";
	cout << " * It only uses L1 regularization during the last 20% of training.\n\n";

	// Load the data
	GMatrix trainLab;
	GMatrix testLab;
	trainLab.loadArff((g_dataDir + "/train.arff").c_str());
	testLab.loadArff((g_dataDir + "/test.arff").c_str());

	GMatrix trainFeat(trainLab.rows(), 1);
	for(size_t i = 0; i < trainLab.rows(); i++)
		trainFeat[i][0] = (double)i / trainLab.rows();
	GMatrix testFeat(testLab.rows(), 1);
	for(size_t i = 0; i < testLab.rows(); i++)
		testFeat[i][0] = (double)(i + trainLab.rows()) / trainLab.rows();

	// The Fourier transform needs a power-of-2 number of rows: pad the
	// training series with copies of its first row and stretch the period to
	// match, as fourier() did.
	GMatrix padded(trainLab);
	while(padded.rows() & (padded.rows() - 1))
		padded.newRow().copy(padded.row(0));
	const size_t n = padded.rows();
	const double period = (double)n / trainLab.rows();

	// The network. Layer 1: softplus and identity units; layer 2: the Fourier
	// sine units plus more softplus and identity units; layer 3: identity.
	const size_t sp = SOFTPLUS_NODES, id = IDENTITY_NODES;
	GNeuralNet nn;
	nn.add(new GBlockLinear(1, sp + id));
	nn.add(new GBlockSoftPlus(sp));
	nn.concat(new GBlockIdentity(id), sp);
	nn.add(new GBlockLinear(sp + id, n + sp + id));
	nn.add(new GBlockSine(n));
	nn.concat(new GBlockSoftPlus(sp), n);
	nn.concat(new GBlockIdentity(id), n + sp);
	nn.add(new GBlockLinear(n + sp + id, 1));
	GRand rand(0);
	GSGDOptimizer optimizer(nn, rand); // allocates nn's weights
	Linear layer1{nn.layer(0).block(0)};
	Linear layer2{nn.layer(2).block(0)};
	Linear layer3{nn.layer(4).block(0)};

	// Use the Fourier transform to initialize the sine units and the output.
	// First as a plain Fourier network, x -> sine units -> output, to check
	// that it fits the training data.
	FourierFit fit = fourierFit(padded, period);
	{
		GNeuralNet plain;
		plain.add(new GBlockLinear(1, n));
		plain.add(new GBlockSine(n));
		plain.add(new GBlockLinear(n, 1));
		GRand plainRand(0);
		plain.init(plainRand);
		Linear a{plain.layer(0).block(0)};
		Linear b{plain.layer(2).block(0)};
		for(size_t j = 0; j < n; j++)
		{
			a.w(0, j) = fit.freq[j];
			a.bias(j) = fit.phase[j];
			b.w(j, 0) = fit.outWeight[j];
		}
		b.bias(0) = fit.outBias;
		plot_it("fourier.svg", plain, trainFeat, trainLab, testFeat, testLab);

		// Test that it fits to the training data
		double fourier_rmse = rmse(plain, trainFeat, trainLab);
		if(fourier_rmse >= 1e-9)
			throw Ex("Fourier transform failed to fit the training data. (This should never happen.)");
	}

	// Put the sine units in layer 2 (reading layer 1's first output, which
	// carries x) and the Fourier output weights in layer 3. Weights from the
	// inputs they didn't have before get small random values, as
	// GLayerClassic::resize gave them.
	double sineDev = resizeDeviation(fit.freq, (sp + id - 1) * n, PERTURBATION);
	for(size_t j = 0; j < n; j++)
	{
		layer2.bias(j) = fit.phase[j];
		layer2.w(0, j) = fit.freq[j];
		for(size_t i = 1; i < sp + id; i++)
			layer2.w(i, j) = sineDev * rand.normal();
	}
	double outDev = resizeDeviation(fit.outWeight, sp + id, PERTURBATION);
	layer3.bias(0) = fit.outBias;
	for(size_t j = 0; j < n; j++)
		layer3.w(j, 0) = fit.outWeight[j];
	for(size_t i = n; i < n + sp + id; i++)
		layer3.w(i, 0) = outDev * rand.normal();

	// Initialize all the softplus nodes to approximate the identity function
	layer1.setIdentity(0, sp);
	for(size_t i = 0; i < sp; i++)
	{
		layer1.bias(i) += SOFTPLUS_SHIFT;
		layer2.renormalizeInput(i, 0.0, 1.0, SOFTPLUS_SHIFT, SOFTPLUS_SHIFT + 1.0);
	}
	layer2.setIdentity(n, sp);
	for(size_t i = 0; i < sp; i++)
	{
		layer2.bias(n + i) += SOFTPLUS_SHIFT;
		layer3.renormalizeInput(n + i, 0.0, 1.0, SOFTPLUS_SHIFT, SOFTPLUS_SHIFT + 1.0);
	}

	// Initialize all the identity nodes to approximate the identity function
	layer1.setIdentity(sp, id);
	layer2.setIdentity(n + sp, id);

	// Perturb the weights a little bit
	layer1.perturb(rand, PERTURBATION, 0, sp + id);
	layer2.perturb(rand, PERTURBATION, n, sp + id);

	// Test that we still fit to the training data well enough
	double labMean = trainLab.columnMean(0);
	double labDev = sqrt(trainLab.columnVariance(0, labMean));
	cout << "dev=" << to_str(labDev) << "\n";
	double initial_rmse = rmse(nn, trainFeat, trainLab);
	cout << "initial rmse/dev=" << to_str(initial_rmse / labDev) << "\n";
	if(initial_rmse >= TIGHTNESS_GOOD * labDev)
		throw Ex("Already above threshold on initialization. This probably means PERTURBATION is too high or SOFTPLUS_SHIFT is too low.");

	cout << "Progress is drawn to " << outPath("progress.svg") << " (open it in a browser and reload).\n";

	// Do some training. nn's weights are one vector; each round trains it
	// at a higher and a lower learning rate from the same start and keeps
	// whichever did better (the old demo kept two networks for this).
	GVec& weights = nn.weights;
	GVec backup(weights.size()), higher(weights.size());
	GRandomIndexIterator ii(trainLab.rows(), rand);
	double learningRate = 1e-8;
	double lambda = 0.001;
	auto trainEpoch = [&](double rate)
	{
		ii.reset();
		size_t i;
		while(ii.next(i))
		{
			optimizer.computeGradient(trainFeat[i], trainLab[i]); // One iteration of stochastic gradient descent
			optimizer.descendGradient(rate);
		}
	};
	for(size_t epoch = 0; epoch < maxEpochs; epoch++)
	{
		// Regularize until it reaches TIGHTNESS_BAD
		double err;
		size_t regularize_count = 0;
		while(true)
		{
			layer1.regularize(0.1 * lambda, 0.1 * lambda, 0, sp);
			layer1.regularize(0.01 * lambda, 0.01 * lambda, sp, id);
			layer2.regularize(lambda, lambda, 0, n);
			layer2.regularize(0.1 * lambda, 0.1 * lambda, n, sp);
			layer2.regularize(0.01 * lambda, 0.01 * lambda, n + sp, id);
			layer3.regularize(lambda, lambda, 0, 1);

			// Test whether we are there yet
			regularize_count++;
			err = rmse(nn, trainFeat, trainLab);
			if(err / labDev >= TIGHTNESS_BAD)
				break;
		}
		if(regularize_count < 5)
			lambda *= 0.1;
		else
			lambda *= 1.2;
		cout << "rmse/dev=" << to_str(err / labDev) << "	lambda=" << to_str(lambda) << "	regularization iters=" << to_str(regularize_count) << "\n";

		// Train until it reaches TIGHTNESS_GOOD
		double timeStart = 0.0;
		while(true)
		{
			backup.copy(weights);
			trainEpoch(learningRate * 1.25);
			double rmse1 = rmse(nn, trainFeat, trainLab);
			higher.copy(weights);
			weights.copy(backup);
			trainEpoch(learningRate * 0.8);
			double rmse2 = rmse(nn, trainFeat, trainLab);

			// Adjust the learning rate
			if(rmse1 <= rmse2 && rmse1 <= err)
			{
				err = rmse1;
				learningRate *= 1.25;
				weights.copy(higher);
			}
			else if(rmse2 <= rmse1 && rmse2 <= err)
			{
				err = rmse2;
				learningRate *= 0.8;
			}
			else
			{
				learningRate *= 0.1;
				weights.copy(backup);
			}

			// Report progress
			double timeNow = GTime::seconds();
			if(timeNow - timeStart >= 0.5)
			{
				timeStart = timeNow;
				double val = rmse(nn, testFeat, testLab);
				cout << "rmse/dev=" << to_str(err / labDev) << "	eta=" << to_str(learningRate) << "	val=" << to_str(val) << "\n";
				plot_it("progress.svg", nn, trainFeat, trainLab, testFeat, testLab);
			}

			if(err / labDev <= TIGHTNESS_GOOD)
				break;
		}
	}
	plot_it("progress.svg", nn, trainFeat, trainLab, testFeat, testLab);
}

} // namespace

// Usage: fourier_net [--epochs N] [--data DIR]
// Runs until stopped unless --epochs is given. Reads train.arff and test.arff
// from DIR, and writes fourier.svg and progress.svg to the directory
// `bazel run` was started from (or the current directory).
int main(int argc, char* argv[])
{
#ifdef _DEBUG
	GApp::enableFloatingPointExceptions();
#endif
	int nRet = 0;
	try
	{
		size_t maxEpochs = (size_t)-1;
		for(int i = 1; i < argc; i++)
		{
			if(strcmp(argv[i], "--epochs") == 0 && i + 1 < argc)
				maxEpochs = (size_t)strtoul(argv[++i], nullptr, 10);
			else if(strcmp(argv[i], "--data") == 0 && i + 1 < argc)
				g_dataDir = argv[++i];
			else
				throw Ex("Usage: fourier_net [--epochs N] [--data DIR]");
		}
		if(const char* dir = getenv("BUILD_WORKING_DIRECTORY"))
			g_outDir = dir;
		doit(maxEpochs);
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}
