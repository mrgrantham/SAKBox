/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#ifndef __GNEURALNET_H__
#define __GNEURALNET_H__

#include "WLearner.h"
#include "WOptimizer.h"
#include "WVec.h"
#include <vector>
#include "WDom.h"
#include <cmath>

namespace WClasses
{

class WContextLayer;
class WContextNeuralNet;
class WContextRecurrent;
class WLayer;










/// Represents a block of network units (artificial neurons) in a neural network.
class WBlock
{
protected:
	size_t inputCount;
	size_t outputCount;
	size_t m_inPos;

public:
	WVec outputBuf;
	WVec outBlameBuf;
	WVec weightsBuf;
	WVec gradientBuf;
	WConstVecWrapper input;
	WVecWrapper output;
	WVecWrapper outBlame;
	WVecWrapper inBlame;
	WVecWrapper weights;
	WVecWrapper gradient;

	enum BlockType
	{
		block_neuralnet,

		// activation functions
		block_identity,
		block_tanh,
		block_scaledtanh,
		block_logistic,
		block_el,
		block_elpos,
		block_bentidentity,
		block_sigexp,
		block_gaussian,
		block_sine,
		block_rectifier,
		block_leakyrectifier,
		block_softplus,
		block_softroot,
		block_softmax,

		// weights transfer
		block_linear,
		block_fanout,
		block_conv,
		block_temperedlinear,
		block_pal,
		block_hinge,
		block_elbow,
		block_softexp,
		block_hypercube,
		block_catin,
		block_catout,
		block_weight_digester,
		block_optional,

		// weightless transfer
		block_scalarsum,
		block_scalarproduct,
		block_biasproduct,
		block_switch,
		block_spectral,
		block_spreader,
		block_repeater,

		// recurrent
		block_lstm,

		// still needed
		// block_maxout,
		// block_batch_normalization,
		// block_drop_out,
		// block_drop_connect,
		// block_softmax,
		// block_gru,
	};

	WBlock(size_t inputs, size_t outputs);
	WBlock(const WBlock& that);
	WBlock(const WDomNode* pNode);
	virtual ~WBlock();

	/// Returns the type of this block
	virtual BlockType type() const = 0;

	/// Returns the name of this block in the form of a string
	virtual std::string name() const = 0;

	/// Returns a string representation of this block
	virtual std::string to_str(bool includeWeights = false, bool includeActivations = false) const;

	/// Returns true iff this block is recurrent
	virtual bool isRecurrent() const { return false; }

	/// Marshall this block into a DOM.
	virtual WDomNode* serialize(WDom* pDoc) const;

	/// Makes a new block of the type the DOM node names, from that node.
	/// SAKBox: was deserialize, the name WNeuralNet uses for loading a net
	/// in place, so the two meant different things under one name.
	static WBlock* deserializeBlock(WDomNode* pNode, WRand& rand);

	/// Returns a copy of this block
	virtual WBlock* clone() const = 0;

	/// Attaches this block to the buffers it will operate on.
	/// If any of pOutput, pOutBlame, pWeights, or pGradient are nullptr, it allocates its own buffers for these.
	/// If pInput or pInBlame are nullptr, it just leaves them unbound.
	virtual void bind(const WVec* pInput, WVec* pOutput, WVec* pOutBlame, WVec* pInBlame, WVec* pWeights, WVec* pGradient);

	/// Returns the offset in the previous layer's output where values are fed as input to this block.
	size_t inPos() const { return m_inPos; }

	/// Sets the starting offset in the previous layer's output where values will be fed as input to this block.
	void setInPos(size_t n);

	/// Returns the number of inputs this block consumes
	virtual size_t inputs() const { return inputCount; }

	/// Returns the number of outputs this block produces
	virtual size_t outputs() const { return outputCount; }

	/// Evaluate the input, compute the output. (Assumes bind has already been called.)
	virtual void forwardProp() = 0;

	/// Computes the blame on the output of this block.
	/// Returns the SSE. Some blocks, such as SoftMax, may override it for comparable behavior.
	/// (Assumes forwardProp has already been called.)
	virtual double computeBlame(const WVec& target);

	/// Resets the state in any recurrent connections.
	virtual void resetState() {}

	/// Resets the state in any recurrent connections.
	virtual WBlock* advanceState(size_t unfoldedInstances) { return this; }

	/// Evaluate outBlame, update inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	/// (Assumes computeBlame has already been called.)
	virtual void backProp() = 0;

	/// Evaluate the input and outBlame, update the gradient of the weights.
	/// (Assumes backProp has already been called.)
	virtual void updateGradient() = 0;

	/// By default, just calls updateGradient. Blocks that implement this method
	/// update the gradient using only the sign of the input, ignoring the magnitude of the input.
	virtual void updateGradientNormalized() { updateGradient(); }

	/// Adds the gradient scaled by the learning rate to the weights.
	/// (Assumes updateGradient has already been called.)
	virtual void step(double learningRate, double momentum);

	/// Same as step, but also adds random noise proportional by jitter to the gradient magnitude to the step.
	virtual void step_jitter(double learningRate, double momentum, double jitter, WRand& rand);

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	virtual size_t weightCount() const = 0;

	/// Returns the number of double-precision elements necessary to represent the gradient of this block.
	virtual size_t gradCount() const
	{
		return weightCount();
	}

	/// Initialize the weights, usually with small random values.
	virtual void initWeights(WRand& rand) = 0;

	/// Blocks that support this method will initialize the weights in a manner that causes the
	/// block to implement the identity function. For all other blocks with weights, this will throw an exception.
	virtual void init_identity(WVec& weights);

	/// Return true iff the weights for this block make it implement identity.
	virtual bool is_identity(WVec& weights);

	/// Returns the number of weights this layer would have if its input and output sizes were adjusted by the given values.
	/// Throws an exception for blocks that do not implement this method.
	virtual size_t adjustedWeightCount(int inAdjustment, int outAdjustment);

	/// Add newInputs units to the input and newOutput units to the output of this block.
	/// The weights in weightsAft will be set to reflect the adjustments.
	virtual void addUnits(size_t newInputs, size_t newOutputs, const WVec& weightsBef, WVec& weightsAft, WRand& rand);

	/// If input is not INVALID_INDEX, then the specified input will be dropped.
	/// If output is not INVALID_INDEX, then the specified output will be dropped.
	/// The weights in weightsAft will be set to reflect the adjustments.
	virtual void dropUnit(size_t input, size_t output, const WVec& weightsBef, WVec& weightsAft);

	/// Puts a 1 in elements that correspond with bias weights and a 0 in all other elements.
	virtual void biasMask(WVec& mask);

	/// Returns the index of the first input that this layer completely ignores, or INVALID_INDEX if there are none.
	virtual size_t firstIgnoredInput(const WVec& weights);

	/// Uses central differencing to test that backProp and updateGradient are implemented correctly.
	/// Throws an exception if a problem is found.
	void finiteDifferencingTest(double tolerance = 0.0005);

protected:
	WDomNode* baseDomNode(WDom* pDoc) const;

	/// Exercises some basic functionality that all blocks have in common
	void basicTest();
};




/// The base class of blocks that have no weights
class WBlockWeightless : public WBlock
{
public:
	WBlockWeightless(size_t inputs, size_t outputs) : WBlock(inputs, outputs) {}
	WBlockWeightless(const WBlockWeightless& that) : WBlock(that) {}
	WBlockWeightless(const WDomNode* pNode) : WBlock(pNode) {}
	~WBlockWeightless() override {}

	size_t weightCount() const override { return 0; }
	void initWeights(WRand& rand) override {}
	void updateGradient() override {}
	void step(double learningRate, double momentum) override {}
};



/// The base class of blocks that apply an activation function, such as tanh, in an element-wise manner.
class WBlockActivation : public WBlockWeightless
{
public:
	WBlockActivation(size_t size);
	WBlockActivation(const WBlockActivation& that) : WBlockWeightless(that) {}
	WBlockActivation(const WDomNode* pNode);
	~WBlockActivation() override {}

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Computes the input that would produce the specified output.
	/// (May throw an exception if this activation function is not invertible.)
	void inverseProp(const WVec& output, WVec& input);

	/// Evaluates the activation function
	virtual double eval(double x) const = 0;

	/// Evaluates the derivative of the activation function.
	/// x is the net input, and f_x is the output activation--the value obtained by calling eval(x).
	virtual double derivative(double x, double f_x) const = 0;

	virtual double inverse(double y) const
	{
		throw Ex("Sorry, this activation function is not invertible");
	}
};




/// Applies the [Identity function](https://en.wikipedia.org/wiki/Identity_function) element-wise to the input.
/// Serves as a pass-through block of units in a neural network.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = x \f]   | ![](Activation_identity.png)
///
class WBlockIdentity : public WBlockActivation
{
public:
	WBlockIdentity(size_t size) : WBlockActivation(size) {}
	WBlockIdentity(const WBlockIdentity& that) : WBlockActivation(that) {}
	WBlockIdentity(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockIdentity() override {}
	BlockType type() const override { return block_identity; }
	std::string name() const override { return "WBlockIdentity"; }
	WBlockIdentity* clone() const override { return new WBlockIdentity(*this); }
	double eval(double x) const override { return x; }
	double derivative(double x, double f_x) const override { return 1.0; }
	double inverse(double y) const override { return y; }
};






/// Applies the [TanH function](https://en.wikipedia.org/wiki/Hyperbolic_function#Hyperbolic_tangent) element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = tanh(x) \f]   | ![](Activation_tanh.png)
///
// (Note, the following code is a bit faster:
// 		if(std::abs(x) >= 700.0)
// 			return (x >= 0 ? 1.0 : -1.0);
// 		double a = exp(x);
// 		double b = 1.0 / a;
// 		return (a - b) / (a + b);
// and here is a fast version of the derivative
// 		if(std::abs(x) >= 700.0)
// 			return (x >= 0 ? 1.0 : 0.0);
// 		double a = exp(x);
// 		double b = 1.0 / a;
// 		double d = 2.0 / (a + b); // sech(x)
// 		return d * d;
class WBlockTanh : public WBlockActivation
{
public:
	WBlockTanh(size_t size) : WBlockActivation(size) {}
	WBlockTanh(const WBlockTanh& that) : WBlockActivation(that) {}
	WBlockTanh(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockTanh() override {}
	BlockType type() const override { return block_tanh; }
	std::string name() const override { return "WBlockTanh"; }
	WBlockTanh* clone() const override { return new WBlockTanh(*this); }
	double eval(double x) const override { return std::tanh(x); }
	double derivative(double x, double f_x) const override { return 1.0 - (f_x * f_x); }
	double inverse(double y) const override { return 0.5 * std::log(-(y + 1) / (y - 1)); }
};




/// Applies a scaled TanH function element-wise to the input.
/// | Equation  |
/// | --------- |
/// | \f[ f(x) = tanh(x \times 0.66666667) \times  1.7159\f] |
/// LeCun et al. suggest scale_in=2/3 and scale_out=1.7159. By carefully matching
/// scale_in and scale_out, the nonlinearity can also be tuned to preserve the mean and variance of its input:
/// - scale_in=0.5, scale_out=2.4: If the input is a random normal variable, the output will have zero mean and unit variance.
/// - scale_in=1, scale_out=1.6: Same property, but with a smaller linear regime in input space.
/// - scale_in=0.5, scale_out=2.27: If the input is a uniform normal variable, the output will have zero mean and unit variance.
/// - scale_in=1, scale_out=1.48: Same property, but with a smaller linear regime in input space.
///
class WBlockScaledTanh : public WBlockActivation
{
	const double SCALE_IN = 0.66666667;
	const double SCALE_OUT = 1.7159;
public:
	WBlockScaledTanh(size_t size) : WBlockActivation(size) {}
	WBlockScaledTanh(const WBlockScaledTanh& that) : WBlockActivation(that) {}
	WBlockScaledTanh(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockScaledTanh() override {}
	BlockType type() const override { return block_scaledtanh; }
	std::string name() const override { return "WBlockScaledTanh"; }
	WBlockScaledTanh* clone() const override { return new WBlockScaledTanh(*this); }
	double eval(double x) const override { return std::tanh(x * SCALE_IN) * SCALE_OUT; }
	double derivative(double x, double f_x) const override { return SCALE_IN / SCALE_OUT * (SCALE_OUT - f_x) * (SCALE_OUT + f_x); }
	double inverse(double y) const override { return 0.5 / SCALE_IN * std::log(-(y / SCALE_OUT + 1) / (y / SCALE_OUT - 1)); }
};



/// Applies the [Logistic function](https://en.wikipedia.org/wiki/Logistic_function) element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = \frac{1}{1 + e^{-x}} \f]   | ![](Activation_logistic.png)
///
class WBlockLogistic : public WBlockActivation
{
public:
	WBlockLogistic(size_t size) : WBlockActivation(size) {}
	WBlockLogistic(const WBlockLogistic& that) : WBlockActivation(that) {}
	WBlockLogistic(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockLogistic() override {}
	BlockType type() const override { return block_logistic; }
	std::string name() const override { return "WBlockLogistic"; }
	WBlockLogistic* clone() const override { return new WBlockLogistic(*this); }
	double eval(double x) const override
	{
		if(x >= 700.0) // Don't trigger a floating point exception
			return 1.0;
		else if(x < -700.0) // Don't trigger a floating point exception
			return 0.0;
		else return 1.0 / (std::exp(-x) + 1.0);
	}
	double derivative(double x, double f_x) const override { return f_x * (1.0 - f_x); }
	double inverse(double y) const override { return std::log(y / (1.0 - y)); }
};




/// An exponential-linear unit
class WBlockEl : public WBlockActivation
{
public:
	WBlockEl(size_t size) : WBlockActivation(size) {}
	WBlockEl(const WBlockEl& that) : WBlockActivation(that) {}
	WBlockEl(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockEl() override {}
	BlockType type() const override { return block_el; }
	std::string name() const override { return "WBlockEl"; }
	WBlockEl* clone() const override { return new WBlockEl(*this); }
	double eval(double x) const override { return x < 0 ? std::exp(x) - 1.0 : x; }
	double derivative(double x, double f_x) const override { return x < 0 ? std::exp(x) : 1.0; }
	double inverse(double y) const override { return y < 0 ? std::log(y + 1) : y; }
};




/// An exponential-linear unit.
/// As a special case, this block passes UNKNOWN_REAL_VALUE straight through.
class WBlockElPos : public WBlockActivation
{
public:
	WBlockElPos(size_t size) : WBlockActivation(size) {}
	WBlockElPos(const WBlockElPos& that) : WBlockActivation(that) {}
	WBlockElPos(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockElPos() override {}
	BlockType type() const override { return block_elpos; }
	std::string name() const override { return "WBlockElPos"; }
	WBlockElPos* clone() const override { return new WBlockElPos(*this); }
	double eval(double x) const override { return x == UNKNOWN_REAL_VALUE ? UNKNOWN_REAL_VALUE : (x < 0 ? std::exp(x) : x + 1); }
	double derivative(double x, double f_x) const override { return x == UNKNOWN_REAL_VALUE ? 0.0 : (x < 0 ? std::exp(x) : 1.0); }
	// SAKBox: was "y = UNKNOWN_REAL_VALUE ? ..." (an assignment, so always unknown),
	// and inverted the wrong pieces; eval is exp(x) below 0 (values below 1) and
	// x + 1 above.
	double inverse(double y) const override { return y == UNKNOWN_REAL_VALUE ? UNKNOWN_REAL_VALUE : (y < 1 ? std::log(y) : y - 1); }
};




#define BEND_AMOUNT 0.5
#define BEND_SIZE 0.1
/// Applies the Bent identity element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = \frac{\sqrt{x^2+0.01}-0.1}{2}+x \f]   | ![](Activation_bent_identity.png)
///
class WBlockBentIdentity : public WBlockActivation
{
public:
	WBlockBentIdentity(size_t size) : WBlockActivation(size) {}
	WBlockBentIdentity(const WBlockBentIdentity& that) : WBlockActivation(that) {}
	WBlockBentIdentity(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockBentIdentity() override {}
	BlockType type() const override { return block_bentidentity; }
	std::string name() const override { return "WBlockBentIdentity"; }
	WBlockBentIdentity* clone() const override { return new WBlockBentIdentity(*this); }
	double eval(double x) const override { return BEND_AMOUNT * (std::sqrt(x * x + BEND_SIZE * BEND_SIZE) - BEND_SIZE) + x; }
	double derivative(double x, double f_x) const override { return BEND_AMOUNT * x / std::sqrt(x * x + BEND_SIZE * BEND_SIZE) + 1.0; }
	double inverse(double y) const override { return (std::sqrt(2.0 * BEND_AMOUNT * BEND_AMOUNT * BEND_AMOUNT * BEND_SIZE * y + BEND_AMOUNT * BEND_AMOUNT * BEND_SIZE * BEND_SIZE + BEND_AMOUNT * BEND_AMOUNT * y * y) - BEND_AMOUNT * BEND_SIZE - y) / (BEND_AMOUNT * BEND_AMOUNT - 1.0); }
};



/// An element-wise nonlinearity block
/// This activation function forms a sigmoid shape by splicing exponential and logarithmic functions together.
class WBlockSigExp : public WBlockActivation
{
public:
	WBlockSigExp(size_t size) : WBlockActivation(size) {}
	WBlockSigExp(const WBlockSigExp& that) : WBlockActivation(that) {}
	WBlockSigExp(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockSigExp() override {}
	BlockType type() const override { return block_sigexp; }
	std::string name() const override { return "WBlockSigExp"; }
	WBlockSigExp* clone() const override { return new WBlockSigExp(*this); }
	double eval(double x) const override { return (x <= 0.0 ? exp(x) - 1.0 : std::log(x + 1.0)); }
	double derivative(double x, double f_x) const override { return (x <= 0.0 ? std::exp(x) : 1.0 / (x + 1.0)); }
	double inverse(double y) const override { return (y > 0.0 ? exp(y) - 1.0 : std::log(y + 1.0)); }
};



/// Applies the [Gaussian function](https://en.wikipedia.org/wiki/Gaussian_function) element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = e^{-x^2} \f]   | ![](Activation_gaussian.png)
///
class WBlockGaussian : public WBlockActivation
{
public:
	WBlockGaussian(size_t size) : WBlockActivation(size) {}
	WBlockGaussian(const WBlockGaussian& that) : WBlockActivation(that) {}
	WBlockGaussian(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockGaussian() override {}
	BlockType type() const override { return block_gaussian; }
	std::string name() const override { return "WBlockGaussian"; }
	WBlockGaussian* clone() const override { return new WBlockGaussian(*this); }
	double eval(double x) const override { return std::exp(-(x * x)); }
	double derivative(double x, double f_x) const override { return -2.0 * x * f_x; }
};



/// Applies the [Sinusoid](https://en.wikipedia.org/wiki/Sine_wave) element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = \sin(x) \f]   | ![](Activation_sinusoid.png)
///
class WBlockSine : public WBlockActivation
{
public:
	WBlockSine(size_t size) : WBlockActivation(size) {}
	WBlockSine(const WBlockSine& that) : WBlockActivation(that) {}
	WBlockSine(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockSine() override {}
	BlockType type() const override { return block_sine; }
	std::string name() const override { return "WBlockSine"; }
	WBlockSine* clone() const override { return new WBlockSine(*this); }
	double eval(double x) const override { return std::sin(x); }
	double derivative(double x, double f_x) const override { return std::cos(x); }
};




/// Applies the [Rectified linear unit](https://en.wikipedia.org/wiki/Rectifier_(neural_networks)) (ReLU) element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = \left \{ \begin{array}{rcl} 0 & \mbox{for} & x < 0 \\ x & \mbox{for} & x \ge 0\end{array} \right. \f]   | ![](Activation_rectified_linear.png)
///
class WBlockRectifier : public WBlockActivation
{
public:
	WBlockRectifier(size_t size) : WBlockActivation(size) {}
	WBlockRectifier(const WBlockSine& that) : WBlockActivation(that) {}
	WBlockRectifier(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockRectifier() override {}
	BlockType type() const override { return block_rectifier; }
	std::string name() const override { return "WBlockRectifier"; }
	WBlockRectifier* clone() const override { return new WBlockRectifier(*this); }
	double eval(double x) const override { return std::max(0.0, x); }
	double derivative(double x, double f_x) const override { return (x >= 0.0 ? 1.0 : 0.0); }
};




/// Applies the Leaky rectified linear unit (Leaky ReLU) element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x) = \left \{ \begin{array}{rcl} 0.01x & \mbox{for} & x < 0\\ x & \mbox{for} & x \ge 0\end{array} \right. \f]   | ![](Activation_prelu.png)
///
class WBlockLeakyRectifier : public WBlockActivation
{
public:
	WBlockLeakyRectifier(size_t size) : WBlockActivation(size) {}
	WBlockLeakyRectifier(const WBlockLeakyRectifier& that) : WBlockActivation(that) {}
	WBlockLeakyRectifier(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockLeakyRectifier() override {}
	BlockType type() const override { return block_leakyrectifier; }
	std::string name() const override { return "WBlockLeakyRectifier"; }
	WBlockLeakyRectifier* clone() const override { return new WBlockLeakyRectifier(*this); }
	double eval(double x) const override { return x >= 0.0 ? x : 0.01 * x; }
	double derivative(double x, double f_x) const override { return x >= 0.0 ? 1.0 : 0.01; }
	double inverse(double y) const override { return (y > 0.0 ? y : 100.0 * y); }
};




/// Applies the SoftPlus function element-wise to the input.
/// | Equation  | Plot
/// | --------- | -------
/// | \f[ f(x)=\ln(1+e^x) \f]   | ![](Activation_softplus.png)
///
// (Note: A similar, but less well-known function is the integral of the logistic function. I think it is slightly faster to compute.)
class WBlockSoftPlus : public WBlockActivation
{
public:
	WBlockSoftPlus(size_t size) : WBlockActivation(size) {}
	WBlockSoftPlus(const WBlockSoftPlus& that) : WBlockActivation(that) {}
	WBlockSoftPlus(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockSoftPlus() override {}
	BlockType type() const override { return block_softplus; }
	std::string name() const override { return "WBlockSoftPlus"; }
	WBlockSoftPlus* clone() const override { return new WBlockSoftPlus(*this); }
	double eval(double x) const override { return x > 500 ? x : log(1.0 + exp(x)); }
	double derivative(double x, double f_x) const override { return 1.0 / (1.0 + exp(-x)); }
	double inverse(double y) const override { return log(exp(y) - 1.0); }
};




/// An element-wise nonlinearity block.
/// This function is shaped like a sigmoid, but with both a co-domain and domain
/// that spans the continuous values. At very negative values,
/// it is shaped like y=-sqrt(-2x). Near zero, it is shaped
/// like y=x. At very positive values, it is shaped like y=sqrt(2x).
class WBlockSoftRoot : public WBlockActivation
{
public:
	WBlockSoftRoot(size_t size) : WBlockActivation(size) {}
	WBlockSoftRoot(const WBlockSoftRoot& that) : WBlockActivation(that) {}
	WBlockSoftRoot(const WDomNode* pNode) : WBlockActivation(pNode) {}
	~WBlockSoftRoot() override {}
	BlockType type() const override { return block_softroot; }
	std::string name() const override { return "WBlockSoftRoot"; }
	WBlockSoftRoot* clone() const override { return new WBlockSoftRoot(*this); }
	double eval(double x) const override
	{
		double d = std::sqrt(x * x + 1.0);
		return std::sqrt(d + x) - std::sqrt(d - x);
	}
	double derivative(double x, double f_x) const override
	{
		if(std::abs(x) > 1e7)
			return 0.0;
		double d = std::sqrt(x * x + 1.0);
		double t = x / d;
		return (t + 1.0) / (2.0 * std::sqrt(d + x)) - (t - 1.0) / (2.0 * std::sqrt(d - x));
	}
	double inverse(double y) const override { return 0.5 * y * std::sqrt(y * y + 4.0); }
};




/// A softmax block.
class WBlockSoftMax : public WBlockWeightless
{
public:
	WBlockSoftMax(size_t size) : WBlockWeightless(size, size) {}
	WBlockSoftMax(const WBlockSoftMax& that) : WBlockWeightless(that) {}
	WBlockSoftMax(const WDomNode* pNode) : WBlockWeightless(pNode) {}
	~WBlockSoftMax() override {}
	BlockType type() const override { return block_softmax; }
	std::string name() const override { return "WBlockSoftMax"; }
	WBlockSoftMax* clone() const override { return new WBlockSoftMax(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Computes the blame with cross-entropy
	double computeBlame(const WVec& target) override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};





/// A block of sinusoid units arranged with exponentially growing wavelengths
/// Assumes there is exactly 1 input.
class WBlockSpectral : public WBlockWeightless
{
protected:
	double m_freq_start;
	double m_freq_scale;
	double m_freq_shift;

public:
	WBlockSpectral(double min_wavelength, double max_wavelength, size_t units, bool linear_spacing = false);
	WBlockSpectral(const WBlockSpectral& that) : WBlockWeightless(that) {}
	WBlockSpectral(const WDomNode* pNode);
	~WBlockSpectral() override {}
	BlockType type() const override { return block_spectral; }
	std::string name() const override { return "WBlockSpectral"; }
	WBlockSpectral* clone() const override { return new WBlockSpectral(*this); }

	/// Marshall this block into a DOM.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};





/// Expands a layer by repeating it as needed to acheive the desired number of units.
class WBlockRepeater : public WBlockWeightless
{
public:
	WBlockRepeater(size_t in, size_t out) : WBlockWeightless(in, out)
	{
		if(in > out) throw Ex("Expected more outputs than inputs");
	}
	WBlockRepeater(const WBlockRepeater& that) : WBlockWeightless(that) {}
	WBlockRepeater(const WDomNode* pNode) : WBlockWeightless(pNode) {}
	~WBlockRepeater() override {}
	BlockType type() const override { return block_repeater; }
	std::string name() const override { return "WBlockRepeater"; }
	WBlockRepeater* clone() const override { return new WBlockRepeater(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};





/// Fans out and shuffles in the values to break spatial locality.
class WBlockSpreader : public WBlockWeightless
{
protected:
	size_t m_spread;
	WIndexVec m_forw;
	WIndexVec m_back;

public:
	WBlockSpreader(size_t units, size_t spread);
	WBlockSpreader(const WBlockSpreader& that);
	WBlockSpreader(const WDomNode* pNode);
	~WBlockSpreader() override {}
	BlockType type() const override { return block_spreader; }
	std::string name() const override { return "WBlockSpreader"; }
	WBlockSpreader* clone() const override { return new WBlockSpreader(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};





/// Standard fully-connected block of weights. Often followed by a WBlockActivation.
class WBlockLinear : public WBlock
{
public:
	/// General-purpose constructor
	WBlockLinear(size_t inputs, size_t outputs);

	/// Copy constructor
	WBlockLinear(const WBlockLinear& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockLinear(WDomNode* pNode);

	/// Destructor
	~WBlockLinear() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_linear; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockLinear"; }

	/// Returns a copy of this block
	WBlockLinear* clone() const override { return new WBlockLinear(*this); }

	/// Returns a string representation of this block
	std::string to_str(bool includeWeights = false, bool includeActivations = false) const override;

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	/// (Assumes backProp has already been called.)
	void updateGradient() override;

	/// Updates the gradient using only the sign of the input, ignoring the magnitude of the input.
	void updateGradientNormalized() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;

	/// Computes weights using Ordinary Least Squares
	void ordinaryLeastSquares(const WMatrix& features, const WMatrix& labels, WVec& outWeights);

	/// Adjusts the bias and weights to compensate for an adjustment in the input range
	/// Warning: the gradient is still scaled by the input values, so this is NOT equivalent for training.
	void adjustInputRange(WVec& weights, size_t inputIndex, double oldMin, double oldMax, double newMin, double newMax);

	/// Initializes weights to implement the identity function
	void init_identity(WVec& weights) override;

	/// Puts a 1 in elements that correspond with bias weights and a 0 in all other elements.
	void biasMask(WVec& mask) override;

	/// Returns the number of weights this layer would have if its input and output sizes were adjusted by the given values.
	size_t adjustedWeightCount(int inAdjustment, int outAdjustment) override;

	/// Returns the index of the first input that this layer completely ignores, or INVALID_INDEX if there are none.
	size_t firstIgnoredInput(const WVec& weights) override;

	/// Add newInputs units to the input and newOutput units to the output of this block.
	/// The weights in weightsAft will be set to reflect the adjustments.
	void addUnits(size_t newInputs, size_t newOutputs, const WVec& weightsBef, WVec& weightsAft, WRand& rand) override;

	/// If input is not INVALID_INDEX, then the specified input will be dropped.
	/// If output is not INVALID_INDEX, then the specified output will be dropped.
	/// The weights in weightsAft will be set to reflect the adjustments.
	void dropUnit(size_t input, size_t output, const WVec& weightsBef, WVec& weightsAft) override;

	/// Pass in two linear layers with their corresponding weights.
	/// The weights will be combined into a single layer that performs the function of both layers.
	/// If targetA is true, the fused weights will be placed in a. Otherwise they will be placed in b.
	/// The non-target layer must have the same number of inputs and outputs
	/// (to ensure that it doesn't change the weights size).
	static void fuseLayers(WBlockLinear& aa, WVec& a, WBlockLinear& bb, WVec& b, bool targetA);

	/// An experimental regularizer that promotes anti-symmetric weights and layers that mirror each other.
	void regularize_square(double lambda, const WVec* pWeightsNext);
};





/// For each input, produces n outputs, where each output is a linear function of one input.
/// As a special case, inputs with a value of UNKNOWN_REAL_VALUE are replicated in the output.
class WBlockFanOut : public WBlock
{
public:
	/// General-purpose constructor
	WBlockFanOut(size_t inputs, size_t outputs);

	/// Copy constructor
	WBlockFanOut(const WBlockFanOut& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockFanOut(WDomNode* pNode);

	/// Destructor
	~WBlockFanOut() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_fanout; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockFanOut"; }

	/// Returns a copy of this block
	WBlockFanOut* clone() const override { return new WBlockFanOut(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	/// (Assumes backProp has already been called.)
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};





/// This is a running version of batch normalization. That is, it uses running means and running variances
/// instead of batch means and batch variances to normalize inputs.
class WBlockRunningNormalizer : public WBlock
{
protected:
	double batch_size;
	double inv_bs;
	double decay_scalar;
	double epsilon;

public:
	/// General-purpose constructor
	WBlockRunningNormalizer(size_t units, double effective_batch_size);

	/// Copy constructor
	WBlockRunningNormalizer(const WBlockRunningNormalizer& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockRunningNormalizer(WDomNode* pNode);

	/// Destructor
	~WBlockRunningNormalizer() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_linear; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockRunningNormalizer"; }

	/// Returns a copy of this block
	WBlockRunningNormalizer* clone() const override { return new WBlockRunningNormalizer(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	/// (Assumes backProp has already been called.)
	void updateGradient() override;

	/// Updates the weights
	void step(double learningRate, double momentum) override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Returns the number of gradient elements
	size_t gradCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};





class WBlockWeightDigester : public WBlock
{
protected:
	WBlock* m_pDigestMyWeights;

public:
	/// General-purpose constructor
	WBlockWeightDigester(WBlock* pDigestMyWeights, size_t extraInputs, size_t outputs);

	/// Copy constructor
	WBlockWeightDigester(const WBlockWeightDigester& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockWeightDigester(WDomNode* pNode);

	/// Destructor
	~WBlockWeightDigester() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_weight_digester; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockWeightDigester"; }

	/// Returns a copy of this block
	WBlockWeightDigester* clone() const override { return new WBlockWeightDigester(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	/// (Assumes backProp has already been called.)
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};





/// A linear layer with protections against saturation.
class WBlockTemperedLinear : public WBlock
{
protected:
	double deviationCap;
	double forgetRate;
	WVec moment1;
	WVec moment2;

public:
	/// General-purpose constructor
	WBlockTemperedLinear(size_t inputs, size_t outputs, double deviation_cap = 3.0, double forget_rate = 0.001);

	/// Copy constructor
	WBlockTemperedLinear(const WBlockTemperedLinear& that);

	/// Unmarshalling constructor
	WBlockTemperedLinear(WDomNode* pNode);

	/// Destructor
	~WBlockTemperedLinear() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_temperedlinear; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockTemperedLinear"; }

	/// Returns a copy of this block
	WBlockTemperedLinear* clone() const override { return new WBlockTemperedLinear(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	/// (Assumes backProp has already been called.)
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};





/// A convolutional layer.
class WBlockConv : public WBlock
{
protected:
	size_t filterSize;
	WTensor tensorInput;
	WTensor tensorFilter;
	WTensor tensorOutput;
	size_t filterCount;
	size_t outputsPerFilter;

public:
	/// General-purpose constructor. Example:
	///  nn.add(new WBlockConv( {28, 28}, {5, 5, 8}, {28, 28, 8} ));
	WBlockConv(const std::initializer_list<size_t>& inputDims, const std::initializer_list<size_t>& filterDims, const std::initializer_list<size_t>& outputDims);

	/// Copy constructor
	WBlockConv(const WBlockConv& that);

	/// Unmarshalling constructor
	WBlockConv(WDomNode* pNode);

	/// Destructor
	~WBlockConv() override {}

	/// Marshall this block into a DOM.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Returns the type of this block
	BlockType type() const override { return block_conv; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockConv"; }

	/// Returns a copy of this block
	WBlockConv* clone() const override { return new WBlockConv(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	/// (Assumes backProp has already been called.)
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;

	static void test();
};





/// Treats the input as two concatenated vectors.
/// Adds each corresponding pair of values together to produce the output.
class WBlockMaxPooling2D : public WBlockWeightless
{
protected:
	size_t width, height, channels;

public:
	/// General-purpose constructor.
	/// width is the width of the input. height is the height of the input. channels is the number of channels in the input.
	WBlockMaxPooling2D(size_t width, size_t height, size_t channels);

	/// Copy constructor
	WBlockMaxPooling2D(const WBlockMaxPooling2D& that) : WBlockWeightless(that) {}

	/// Deserializing constructor
	WBlockMaxPooling2D(WDomNode* pNode);

	/// Destructor
	~WBlockMaxPooling2D() override;

	/// Returns the type of this block
	BlockType type() const override { return block_scalarsum; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockMaxPooling2D"; }

	/// Returns a copy of this block
	WBlockMaxPooling2D* clone() const override { return new WBlockMaxPooling2D(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};




/// Treats the input as two concatenated vectors.
/// Adds each corresponding pair of values together to produce the output.
class WBlockScalarSum : public WBlockWeightless
{
public:
	/// General-purpose constructor.
	WBlockScalarSum(size_t outputs);

	/// Copy constructor
	WBlockScalarSum(const WBlockScalarSum& that) : WBlockWeightless(that) {}

	/// Deserializing constructor
	WBlockScalarSum(WDomNode* pNode);

	/// Destructor
	~WBlockScalarSum() override;

	/// Returns the type of this block
	BlockType type() const override { return block_scalarsum; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockScalarSum"; }

	/// Returns a copy of this block
	WBlockScalarSum* clone() const override { return new WBlockScalarSum(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};





/// Treats the input as two concatenated vectors.
/// Multiplies each corresponding pair of values together to produce the output.
class WBlockScalarProduct : public WBlockWeightless
{
public:
	/// General-purpose constructor.
	WBlockScalarProduct(size_t outputs);

	/// Copy constructor
	WBlockScalarProduct(const WBlockScalarProduct& that) : WBlockWeightless(that) {}

	/// Deserializing constructor
	WBlockScalarProduct(WDomNode* pNode);

	/// Destructor
	~WBlockScalarProduct() override;

	/// Returns the type of this block
	BlockType type() const override { return block_scalarproduct; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockScalarProduct"; }

	/// Returns a copy of this block
	WBlockScalarProduct* clone() const override { return new WBlockScalarProduct(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};






/// Like WBlockScalarProduct, except it adds the first two elements instead of multiplying them.
/// (This is intended to be used as a substitute for matrix factorization.)
class WBlockBiasProduct : public WBlockWeightless
{
public:
	/// General-purpose constructor.
	WBlockBiasProduct(size_t outputs);

	/// Copy constructor
	WBlockBiasProduct(const WBlockBiasProduct& that) : WBlockWeightless(that) {}

	/// Deserializing constructor
	WBlockBiasProduct(WDomNode* pNode);

	/// Destructor
	~WBlockBiasProduct() override;

	/// Returns the type of this block
	BlockType type() const override { return block_biasproduct; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockBiasProduct"; }

	/// Returns a copy of this block
	WBlockBiasProduct* clone() const override { return new WBlockBiasProduct(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};






/// Treats the input as three concatenated vectors: a, b, and c.
/// (The values in 'a' typically fall in the range [0,1].)
/// The output is computed element-wise as a*b + (1-a)*c.
class WBlockSwitch : public WBlockWeightless
{
public:
	/// General-purpose constructor.
	WBlockSwitch(size_t outputs);

	/// Copy constructor
	WBlockSwitch(const WBlockScalarProduct& that) : WBlockWeightless(that) {}

	/// Deserializing constructor
	WBlockSwitch(WDomNode* pNode);

	/// Destructor
	~WBlockSwitch() override;

	/// Returns the type of this block
	BlockType type() const override { return block_switch; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockSwitch"; }

	/// Returns a copy of this block
	WBlockSwitch* clone() const override { return new WBlockSwitch(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};








/// A parameterized activation function (a.k.a. adaptive transfer function).
/// When alpha is 0, this activation function implements identity.
/// When alpha is positive, it bends upward. When alpha is negative, it bends downward.
/// Beta specifies approximately how big the bending curve is. When beta is 0, it bends on a point.
class WBlockHinge : public WBlock
{
public:
	/// General-purpose constructor
	/// Size specifies the number of units in this layer.
	WBlockHinge(size_t size);

	/// Copy constructor
	WBlockHinge(const WBlockHinge& that) : WBlock(that) {}

	WBlockHinge(WDomNode* pNode);

	/// Destructor
	~WBlockHinge() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_hinge; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockHinge"; }

	/// Returns a copy of this block
	WBlockHinge* clone() const override { return new WBlockHinge(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Updates the weights
	void step(double learningRate, double momentum) override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;

	/// Initialize the weights to implement the identity function.
	void init_identity(WVec& weights) override;

	/// Return true iff the weights for this block make it implement identity.
	bool is_identity(WVec& weights) override;

	/// Returns the number of weights this layer would have if its input and output sizes were adjusted by the given values.
	size_t adjustedWeightCount(int inAdjustment, int outAdjustment) override;

	/// Add newInputs units to the input and newOutput units to the output of this block.
	/// The weights in weightsAft will be set to reflect the adjustments.
	void addUnits(size_t newInputs, size_t newOutputs, const WVec& weightsBef, WVec& weightsAft, WRand& rand) override;

	/// If input is not INVALID_INDEX, then the specified input will be dropped.
	/// If output is not INVALID_INDEX, then the specified output will be dropped.
	/// The weights in weightsAft will be set to reflect the adjustments.
	void dropUnit(size_t input, size_t output, const WVec& weightsBef, WVec& weightsAft) override;
};





/// A parameterized activation function (a.k.a. adaptive transfer function).
/// When alpha is 0, this activation function implements identity.
/// When alpha is 1, it implements eponential-linear.
/// When alpha is -1, it implements the anty-symmetric exponetial-linear.
class WBlockElbow : public WBlock
{
public:
	/// General-purpose constructor
	/// Size specifies the number of units in this layer.
	WBlockElbow(size_t size);

	/// Copy constructor
	WBlockElbow(const WBlockElbow& that) : WBlock(that) {}

	WBlockElbow(WDomNode* pNode);

	/// Destructor
	~WBlockElbow() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_elbow; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockElbow"; }

	/// Returns a copy of this block
	WBlockElbow* clone() const override { return new WBlockElbow(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Updates the weights
	void step(double learningRate, double momentum) override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;

	/// Initialize the weights to implement the identity function.
	void init_identity(WVec& weights) override;

	/// Return true iff the weights for this block make it implement identity.
	bool is_identity(WVec& weights) override;

	/// Returns the number of weights this layer would have if its input and output sizes were adjusted by the given values.
	size_t adjustedWeightCount(int inAdjustment, int outAdjustment) override;

	/// Add newInputs units to the input and newOutput units to the output of this block.
	/// The weights in weightsAft will be set to reflect the adjustments.
	void addUnits(size_t newInputs, size_t newOutputs, const WVec& weightsBef, WVec& weightsAft, WRand& rand) override;

	/// If input is not INVALID_INDEX, then the specified input will be dropped.
	/// If output is not INVALID_INDEX, then the specified output will be dropped.
	/// The weights in weightsAft will be set to reflect the adjustments.
	void dropUnit(size_t input, size_t output, const WVec& weightsBef, WVec& weightsAft) override;
};





/// A parameterized activation function (a.k.a. adaptive transfer function).
/// When alpha=0, it is the identity function.
/// As alpha approaches infinity, it approaches tanh.
class WBlockLeakyTanh : public WBlock
{
public:
	/// General-purpose constructor
	/// Size specifies the number of units in this layer.
	WBlockLeakyTanh(size_t size);

	/// Copy constructor
	WBlockLeakyTanh(const WBlockLeakyTanh& that) : WBlock(that) {}

	WBlockLeakyTanh(WDomNode* pNode);

	/// Destructor
	~WBlockLeakyTanh() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_hinge; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockLeakyTanh"; }

	/// Returns a copy of this block
	WBlockLeakyTanh* clone() const override { return new WBlockLeakyTanh(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Updates the weights
	void step(double learningRate, double momentum) override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};





/// A parameterized activation function (a.k.a. adaptive transfer function).
class WBlockSoftExp : public WBlock
{
protected:
	double m_beta;

public:
	/// General-purpose constructor
	/// When beta is 0, this activation function always approximates identity near the origin, but approximates e^x-1 when alpha is 1.
	/// When beta is 1, this activation function approximates e^x when alpha is 1.
	WBlockSoftExp(size_t size, double beta = 0.0);

	/// Copy constructor
	WBlockSoftExp(const WBlockSoftExp& that) : WBlock(that), m_beta(that.m_beta) {}

	WBlockSoftExp(WDomNode* pNode);

	/// Destructor
	~WBlockSoftExp() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_softexp; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockSoftExp"; }

	/// Marshall this block into a DOM.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Returns a copy of this block
	WBlockSoftExp* clone() const override { return new WBlockSoftExp(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};











/// A Probabilistically Activating Linear block.
/// This is an experimental block type.
class WBlockPAL : public WBlock
{
protected:
	WVec m_probs; // stores the probability for activating of each unit
	WRand m_rand;

public:
	/// General-purpose constructor
	WBlockPAL(size_t inputs, size_t outputs, WRand& rand);

	/// Copy constructor
	WBlockPAL(const WBlockPAL& that) : WBlock(that), m_probs(that.m_probs.size()), m_rand(that.m_rand) {}

	WBlockPAL(WDomNode* pNode, WRand& rand);

	/// Destructor
	~WBlockPAL() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_pal; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockPAL"; }

	/// Returns a copy of this block
	WBlockPAL* clone() const override { return new WBlockPAL(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};








/// Number of inputs: 2^dims * vertexSizeIn.
/// Number of outputs: 2^dims * vertexSizeOut.
/// Each vertex is fully connected to all the vertices indicated by the edges of a hypercube.
class WBlockHypercube : public WBlock
{
protected:
	size_t m_dims;
	size_t m_vertexSizeIn;
	size_t m_vertexSizeOut;

public:
	/// General-purpose constructor
	WBlockHypercube(size_t dims, size_t vertexSizeIn, size_t vertexSizeOut);

	/// Copy constructor
	WBlockHypercube(const WBlockHypercube& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockHypercube(WDomNode* pNode);

	/// Destructor
	~WBlockHypercube() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_hypercube; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockHypercube"; }

	/// Returns a copy of this block
	WBlockHypercube* clone() const override { return new WBlockHypercube(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};







class WBlockOptional : public WBlock
{
public:
	/// General-purpose constructor
	WBlockOptional(size_t inputs, size_t outputs);

	/// Copy constructor
	WBlockOptional(const WBlockOptional& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockOptional(WDomNode* pNode);

	/// Destructor
	~WBlockOptional() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_optional; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockOptional"; }

	/// Returns a copy of this block
	WBlockOptional* clone() const override { return new WBlockOptional(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};






/// A special block that expects one categorical input value (encoded as a zero-indexed int casted as a double).
/// It learns the best encoding for each input value using each of its units.
/// It dynamically adjusts to accomodate input values not previously seen.
/// It does not backpropagate any error, so it is only suitable for inputs.
class WBlockCatIn : public WBlock
{
protected:
	size_t m_valueCount;

public:
	/// General-purpose constructor
	WBlockCatIn(size_t valueCount, size_t units);

	/// Copy constructor
	WBlockCatIn(const WBlockCatIn& that) : WBlock(that) {}

	/// Unmarshalling constructor
	WBlockCatIn(WDomNode* pNode);

	/// Destructor
	~WBlockCatIn() override {}

	/// Returns the type of this block
	BlockType type() const override { return block_catin; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockCatIn"; }

	/// Returns a copy of this block
	WBlockCatIn* clone() const override { return new WBlockCatIn(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Updates the weights
	void step(double learningRate, double momentum) override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Returns the number of elements for the gradient vector.
	size_t gradCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;
};




/// A special block that uses a one-hot encoding to predict categorical outputs.
/// It trains to predict the most probably category by at least a specified margin.
class WBlockCatOut : public WBlockWeightless
{
protected:
	double m_margin;

public:
	WBlockCatOut(size_t categories, double margin = 0.3);
	WBlockCatOut(const WBlockCatOut& that) : WBlockWeightless(that), m_margin(that.m_margin) {}
	WBlockCatOut(const WDomNode* pNode);
	~WBlockCatOut() override {}

	BlockType type() const override { return block_catout; }
	std::string name() const override { return "WBlockCatOut"; }
	WBlockCatOut* clone() const override { return new WBlockCatOut(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Computes blame for this layer. (Assumes target is a one-dimensional index value casted as a double)
	double computeBlame(const WVec& target) override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;
};





//  Here is a diagram of this LSTM unit.
//  C_t and h-t feed into this same block in the next time step.
//  x_t comes from the previous layer.
//  y_t feeds into the next layer.
//
//  C_t-1 ----(*)--------(+)----------------------> C_t
//             ^          ^                 |
//             |          |               [tanh]
//             +-->(1-)->(*)                |
//             |          |                 v
//             f          t        o------>(*)
//             ^          ^        ^        |   y_t (output)
//             |          |        |        |    ^
//        [logistic]   [tanh]  [logistic]   |    |
//             |          |        |        |    |
//  h_t-1 --------------------------        -------> h_t
//          ^
//          |
//         x_t (input)
//
/// A Long-short-term-memory block
class WBlockLSTM : public WBlock
{
protected:
	WVec n; // net input consisting of x, h, and c
	WVec f; // controls what stays in memory
	WVec t; // computed value that may (or may not) be written to memory
	WVec o; // controls when to output the memory
	WVec c; // incoming memory values
	WVec h; // incoming previous output
	WVec blame_h; // blame on the output of h
	WVec blame_c; // blame on the output of c
	WBlockLSTM* pPrevInstance; // backward around the ring
	WBlockLSTM* pNextInstance; // forward around the ring
	WBlockLSTM* pSpare; // chain (not ring) of unused instances

public:
	/// General-purpose constructor
	WBlockLSTM(size_t inputs, size_t outputs);

	/// Copy constructor
	WBlockLSTM(const WBlockLSTM& that);

	/// Unmarshalling constructor
	WBlockLSTM(WDomNode* pNode);

	/// Destructor
	~WBlockLSTM() override;

	/// Returns the type of this block
	BlockType type() const override { return block_lstm; }

	/// Returns the name of this block
	std::string name() const override { return "WBlockLSTM"; }

	/// Returns a copy of this block
	WBlockLSTM* clone() const override { return new WBlockLSTM(*this); }

	/// Evaluate the input, set the output.
	void forwardProp() override;

	/// Advances through time
	void resetState() override;

	/// Advances through time
	WBlock* advanceState(size_t unfoldedInstances) override;

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp() override;

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient() override;

	/// Returns the number of double-precision elements necessary to serialize the weights of this block into a vector.
	size_t weightCount() const override;

	/// Initialize the weights with small random values.
	void initWeights(WRand& rand) override;

protected:
	void stepInTime();
	void forwardProp_instance(const WVec& weights);
	void backProp_instance(const WVec& weights, bool current);
	void updateGradient_instance(WVec& weights, WVec& gradient);
};











//
//
//
//
//
//
//
//              Layer
//
//
//
//
//
//
//

/// WNeuralNet contains WLayers stacked upon each other.
/// WLayer contains WBlocks concatenated beside each other. (WNeuralNet is a type of WBlock.)
/// Each WBlock is an array of differentiable network units (artificial neurons).
/// The user must add at least one WBlock to each WLayer.
class WLayer
{
protected:
	size_t input_count;
	size_t output_count;
	size_t weight_count;
	size_t grad_count;
	std::vector<WBlock*> m_blocks;

public:
	WVec outputBuf;
	WVec outBlameBuf;
	WVecWrapper output;
	WVecWrapper outBlame;

	WLayer();
	WLayer(const WLayer& that, WLayer* pPrevLayer);
	WLayer(WDomNode* pNode, WRand& rand);
	virtual ~WLayer();

	/// Marshal this object into a dom node.
	WDomNode* serialize(WDom* pDoc) const;

	/// Returns the number of blocks in this layer.
	size_t blockCount() const { return m_blocks.size(); }

	/// Returns a reference to the specified block.
	WBlock& block(size_t i) { return *m_blocks[i]; }
	const WBlock& block(size_t i) const { return *m_blocks[i]; }

	/// Adds a block of network units (artificial neurons) to this layer.
	/// inPos specifies the index of the first output from the previous layer that will feed into this block of units.
	void add(WBlock* pBlock, size_t inPos);

	/// Recounts the number of inputs, outputs, and weights in this layer.
	void recount();

	/// Returns the number of inputs that this layer consumes.
	size_t inputs() const;

	/// Returns the number of outputs that this layer produces.
	size_t outputs() const;

	/// Allocates output and outBlame buffers and binds each block to the appropriate parts of all these buffers.
	void bind(const WVec* pInput, WVec* pOutput, WVec* pOutBlame, WVec* pInBlame, WVec& _weights, WVec& _gradient);

	/// Binds to the specified input buffer
	void bindInput(const WVec& _input);

	/// Binds to the speicified inBlame buffer
	void bindInBlame(WVec& _inBlame);

	/// Resets the weights in all of the blocks in this layer
	void initWeights(WRand& rand);

	/// Returns the total number of weights in this layer
	size_t weightCount() const;

	/// Returns the total number of gradient elements necessary for updating this layer
	size_t gradCount() const;

	/// Evaluate the input, set the output.
	void forwardProp();

	/// Computes the out blame for all of the blocks in this layer
	double computeBlame(const WVec& target);

	/// Resets the state in any recurrent connections
	void resetState();

	/// Resets the state in any recurrent connections
	void advanceState(size_t unfoldedInstances);

	/// Evaluates outBlame, and adds to inBlame.
	/// (Note that it "adds to" the inBlame because multiple blocks may fork from a common source.)
	void backProp();

	/// Updates the gradient for updating the weights by gradient descent.
	void updateGradient();

	/// Updates the gradient using only the sign of the inputs.
	void updateGradientNormalized();

	/// Adds the gradient scaled by the learningRate to the weights.
	void step(double learningRate, double momentum);

	/// Adds the gradient scaled by the learningRate to the weights and also jitters it.
	void step_jitter(double learningRate, double momentum, double jitter, WRand& rand);

	/// Puts a 1 in elements that correspond with bias weights and a 0 in all other elements.
	void biasMask(WVec& mask);
};






//
//
//
//
//
//
//
//             NeuralNet
//
//
//
//
//
//
//

/// WNeuralNet contains WLayers stacked upon each other.
/// WLayer contains WBlocks concatenated beside each other.
/// (WNeuralNet is a type of WBlock, so you can nest.)
/// Each WBlock is an array of differentiable network units (artificial neurons).
/// The user must add at least one WBlock to each WLayer.
class WNeuralNet : public WBlock
{
	friend class WNeuralNetOptimizer;
protected:
	size_t m_weightCount;
	size_t m_gradCount;
	std::vector<WLayer*> m_layers;

public:
	/// General-purpose constructor
	WNeuralNet();

	/// Copy constructor
	WNeuralNet(const WNeuralNet& that);

	/// Deserializing constructor
	WNeuralNet(WDomNode* pNode, WRand& rand);

	/// Destructor
	~WNeuralNet() override;

	/// Deletes all the layers
	void deleteAllLayers();

	/// Returns the type of this layer
	BlockType type() const override { return block_neuralnet; }

	/// Returns the name of this block
	std::string name() const override { return "WNeuralNet"; }

	/// Retuns a copy of this neural network
	WNeuralNet* clone() const override { return new WNeuralNet(*this); }

	/// Marshal this object into a dom node.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Unmarshalls this object in place from a dom node.
	void deserialize(WDomNode* pNode, WRand& rand);

	/// Adds a block as a new layer to this neural network.
	WBlock* add(WBlock* pBlock);
	void add(WBlock* a, WBlock* b)
	{
		add(a);
		add(b);
	}
	void add(WBlock* a, WBlock* b, WBlock* c)
	{
		add(a);
		add(b, c);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d)
	{
		add(a);
		add(b, c, d);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e)
	{
		add(a);
		add(b, c, d, e);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e, WBlock* f)
	{
		add(a);
		add(b, c, d, e, f);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e, WBlock* f, WBlock* g)
	{
		add(a);
		add(b, c, d, e, f, g);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e, WBlock* f, WBlock* g, WBlock* h)
	{
		add(a);
		add(b, c, d, e, f, g, h);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e, WBlock* f, WBlock* g, WBlock* h, WBlock* i)
	{
		add(a);
		add(b, c, d, e, f, g, h, i);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e, WBlock* f, WBlock* g, WBlock* h, WBlock* i, WBlock* j)
	{
		add(a);
		add(b, c, d, e, f, g, h, i, j);
	}
	void add(WBlock* a, WBlock* b, WBlock* c, WBlock* d, WBlock* e, WBlock* f, WBlock* g, WBlock* h, WBlock* i, WBlock* j, WBlock* k)
	{
		add(a);
		add(b, c, d, e, f, g, h, i, j, k);
	}

	/// Attaches this neural network to the relevant buffers for it to operate on.
	void bind(const WVec* pInput, WVec* pOutput, WVec* pOutBlame, WVec* pInBlame, WVec* pWeights, WVec* pGradient) override;

	/// Concatenates a block to the last (output-most) layer in this neural network.
	/// (inPos specifies the starting position of the inputs into this block.)
	WBlock* concat(WBlock* pBlock, size_t inPos);
	/*
	/// Inserts a block into a new layer at the specified position.
	/// Requires the current weights and returns a new weight vector (which may be the
	/// same vector if pBlock has no weights). New weights will be initialized to identity for a linear layer.
	/// The caller is still responsible to delete pOldWeights (if it is not equal to the returned value)
	/// and initialize a new gradient vector.
	WVec* insert(size_t position, WBlock* pBlock, WVec* pOldWeights, WRand& rand);

	/// Drops a layer from this neural network. The layer must have the same number of inputs and outputs,
	/// and it cannot be the output layer.
	/// Requires the current weights and returns a new weight vector (which may be the
	/// same vector if pBlock has no weights).
	/// The caller is still responsible to delete pOldWeights (if it is not equal to the returned value)
	/// and initialize a new gradient vector.
	WVec* drop(size_t layer, WVec* pOldWeights);

	/// Adds a unit to each hidden layer.
	/// startLayer and layerCount specify which layers will be adjusted.
	/// Assumes all specified layers have the same size.
	WVec* increaseWidth(size_t newUnitsPerLayer, WVec* pWeights, size_t startLayer, size_t layerCount, WRand& rand);

	/// If each linear layer ignores at least one unit that feeds into it,
	/// then this removes an ignored unit from every layer to make the whole network
	/// thinner by one unit. If this condition is not met, then it returns nullptr.
	WVec* decrementWidth(WVec* pWeights, size_t startLayer, size_t layerCount);
*/
	/// An experimental regularization method that promotes anti-symmetry and mirroring in square linear layers
	void regularize_square(double lambda);

	/// Returns the number of layers in this neural net.
	/// (Layers within neural networks embedded within this one are not counted.)
	size_t layerCount() const { return m_layers.size(); }

	/// Returns the specified layer.
	WLayer& layer(size_t i) { return *m_layers[i]; }
	const WLayer& layer(size_t i) const { return *m_layers[i]; }

	/// Returns a reference to the last layer.
	WLayer& outputLayer() { return *m_layers[m_layers.size() - 1]; }
	const WLayer& outputLayer() const { return *m_layers[m_layers.size() - 1]; }

	/// Returns a string representation of this object
	std::string to_str(bool includeWeights = false, bool includeActivations = false) const override;

	/// Same as to_str, but it lets the use specify a string to prepend to each line, and optionally includes the weights
	std::string to_str(const std::string& line_prefix, bool includeWeights = false, bool includeActivations = false) const;

	/// Returns the number of inputs this layer consumes
	size_t inputs() const override { return m_layers[0]->inputs(); }

	/// Returns the number of outputs this layer produces
	size_t outputs() const override { return outputLayer().outputs(); }

	/// Recounts the number of weights.
	void recount();

	/// Returns the number of weights.
	size_t weightCount() const override;

	/// Returns the number of elements in the gradient.
	size_t gradCount() const override;

	/// Copies all the layers from other.
	void copyTopology(const WNeuralNet& other);

	/// Initialize the weights. (This is called by init.)
	void initWeights(WRand& rand) override;

	/// Allocates buffers needed for training, binds all the layers to these buffers,
	/// and to each other, initializes the weights with small random values, and
	/// initializes the gradient vector with zeros.
	void init(WRand& rand, WNeuralNet* pBase = nullptr);

	/// Initializes with pre-trained weights
	void init(WVec& weights);

	/// Measures the loss with respect to some data. Returns sum-squared error.
	/// if pOutSAE is not nullptr, then sum-absolute error will be storead where it points.
	/// As a special case, if labels have exactly one categorical column, then it will be assumed
	/// that the maximum output unit of this neural network represents a categorical prediction,
	/// and sum hamming loss will be returned.
	double measureLoss(const WMatrix& features, const WMatrix& labels, double* pOutSAE = nullptr);

	/// Evaluates the input vector. Returns an output vector.
	WVec& forwardProp(const WVec& input);

	/// Computes blame on the output of this neural network.
	double computeBlame(const WVec& target) override;

	/// Resets the state in any recurrent connections
	void resetState() override;

	/// Resets the state in any recurrent connections
	WBlock* advanceState(size_t unfoldedInstances) override;

	/// Backpropagates the error. If inputBlame is non-null, the blame for the inputs will also be computed.
	void backpropagate(WVec* inputBlame = nullptr);

	/// Updates the gradient vector
	void updateGradient() override;

	/// Update the gradient using only the sign of the input, ignoring the magnitude of the input.
	void updateGradientNormalized() override;

	/// Adds the gradient scaled by the learning rate to the weights
	void step(double learningRate, double momentum) override;

	/// Adds the gradient scaled by the learningRate to the weights and also jitters it.
	void step_jitter(double learningRate, double momentum, double jitter, WRand& rand) override;

	/// Returns a mathematical expression of this neural network.
	/// (Currently only supports linear, tanh, and scalarProduct blocks in one-block layers.)
	std::string toEquation();

	/// Puts a 1 in elements that correspond with bias weights and a 0 in all other elements.
	void biasMask(WVec& mask) override;

	/// Returns the index of the first weight associated with the specified layer.
	size_t layerStart(size_t layer);

	/// Run unit tests for this class
	static void test();

protected:

	/// Internal method to forward propagate. Assumes the input and output have already been hooked up.
	void forwardProp() override;

	/// Internal method to backpropate error. Assumes inBlame and outBlame have already been hooked up.
	void backProp() override;

	/// Like backProp, but it doesn't compute blame on the inputs.
	void backPropFast();
};







class WBlockResidual : public WNeuralNet
{
	void forwardProp() override;

	void backProp() override;
};








/// A thin wrapper around a WNeuralNet that implements the WIncrementalLearner interface.
class WNeuralNetLearner : public WIncrementalLearner
{
protected:
	WNeuralNet m_nn;
	WNeuralNetOptimizer* m_pOptimizer;

public:
	WNeuralNetLearner();
	//WNeuralNetLearner(const WDomNode* pNode);
	~WNeuralNetLearner() override;

	/// Returns a reference to the neural net that this class wraps
	WNeuralNet& nn() { return m_nn; }

	/// Lazily creates an optimizer for the neural net that this class wraps, and returns a reference to it.
	WNeuralNetOptimizer& optimizer();

	void trainIncremental(const WVec& in, const WVec& out) override;
	void trainSparse(WSparseMatrix& features, WMatrix& labels) override;

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Saves the model to a text file.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WTransducer::supportedFeatureRange
	bool supportedFeatureRange(double* pOutMin, double* pOutMax) override;

	/// See the comment for WTransducer::canImplicitlyHandleMissingFeatures
	bool canImplicitlyHandleMissingFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }

	/// See the comment for WTransducer::supportedFeatureRange
	bool supportedLabelRange(double* pOutMin, double* pOutMax) override;

protected:
	/// See the comment for WIncrementalLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;
};



/*
/// A class that facilitates training a neural network with an arbitrary optimization algorithm
class WNeuralNetTargetFunction : public WTargetFunction
{
protected:
	WNeuralNet& m_nn;
	const WMatrix& m_features;
	const WMatrix& m_labels;
	WRand& m_rand;

public:
	/// features and labels should be pre-filtered to contain only continuous values for the neural network.
	WNeuralNetTargetFunction(WNeuralNet& nn, const WMatrix& features, const WMatrix& labels, WRand& rand)
	: WTargetFunction(nn.weightCount()), m_nn(nn), m_features(features), m_labels(labels), m_rand(rand)
	{
	}

	virtual ~WNeuralNetTargetFunction() {}

	/// Copies the neural network weights into the vector.
	virtual void initVector(WVec& vector)
	{
		WRand rand(0);
		vector.copy(m_nn.weights);
	}

	/// Copies the vector into the neural network and measures sum-squared error.
	virtual double computeError(const WVec& vec)
	{
		return m_nn.measureLoss(vec, m_features, m_labels);
	}
};
*/



} // namespace WClasses

#endif // __GNEURALNET_H__
