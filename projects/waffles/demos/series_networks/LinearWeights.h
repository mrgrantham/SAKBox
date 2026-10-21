#ifndef LINEAR_WEIGHTS_H
#define LINEAR_WEIGHTS_H

// SAKBox: the weight edits Waffles' old WLayerClassic had (setWeightsToIdentity,
// perturbWeights, scaleWeights and diminishWeights, renormalizeInput), for a
// WBlockLinear in the current block API. The fourier_net and time_series demos
// set up their networks with them.

#include <WClasses/WNeuralNet.h>
#include <WClasses/WRand.h>

#include <cstddef>

namespace WClasses
{

// One fully connected linear block's weights: biases first, then one row of
// output weights per input (WBlockLinear's layout).
struct LinearWeights
{
	WBlock& block;
	double& bias(size_t out) { return block.weights[out]; }
	double& w(size_t in, size_t out) { return block.weights[block.outputs() * (1 + in) + out]; }
	size_t inputs() const { return block.inputs(); }
	size_t outputs() const { return block.outputs(); }

	// The old WLayerClassic::setWeightsToIdentity, for outputs [start, start + count)
	void setIdentity(size_t start, size_t count)
	{
		for(size_t o = start; o < start + count; o++)
		{
			bias(o) = 0.0;
			for(size_t i = 0; i < inputs(); i++)
				w(i, o) = (i == o - start ? 1.0 : 0.0);
		}
	}

	// WLayerClassic::perturbWeights, for outputs [start, start + count)
	void perturb(WRand& rand, double deviation, size_t start, size_t count)
	{
		for(size_t o = start; o < start + count; o++)
		{
			bias(o) += deviation * rand.normal();
			for(size_t i = 0; i < inputs(); i++)
				w(i, o) += deviation * rand.normal();
		}
	}

	// WLayerClassic::scaleWeights then diminishWeights (L2 then L1, biases
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

	// WLayerClassic::renormalizeInput: input `in` used to range over
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

} // namespace WClasses

#endif // LINEAR_WEIGHTS_H
