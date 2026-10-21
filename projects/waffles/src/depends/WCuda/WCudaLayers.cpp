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

#include "WCudaLayers.h"
#include "WCudaMatrixKernels.h"
#include "../../WClasses/WNeuralNet.h"
#include "../../WClasses/WVec.h"

namespace WClasses
{

WCudaLayer::WCudaLayer(WCudaEngine& engine)
: WNeuralNetLayer(), m_engine(engine)
{
}

WCudaLayer::WCudaLayer(WDomNode* pNode, WCudaEngine& engine)
: WNeuralNetLayer(), m_engine(engine)
{
}

// virtual
WCudaLayer::~WCudaLayer()
{
}










WLayerClassicCuda::WLayerClassicCuda(WCudaEngine& engine, size_t inputs, size_t outputs)
: WCudaLayer(engine)
{
	resize(inputs, outputs, NULL);
}

WLayerClassicCuda::WLayerClassicCuda(WDomNode* pNode, WCudaEngine& engine)
: WCudaLayer(pNode, engine)
{
	WLayerClassic tmp(pNode);
	upload(tmp);
}

WLayerClassicCuda::~WLayerClassicCuda()
{
}

WDomNode* WLayerClassicCuda::serialize(WDom* pDoc)
{
	WLayerClassic tmp(inputs(), outputs());
	download(tmp);
	return tmp.serialize(pDoc);
}

// virtual
std::string WLayerClassicCuda::to_str()
{
	throw Ex("Sorry, to_str not implemented");
}

void WLayerClassicCuda::resize(size_t inputCount, size_t outputCount)
{
	if(inputCount == inputs() && outputCount == outputs())
		return;

	m_weights.resize(inputCount, outputCount);
	m_delta.resize(inputCount, outputCount);
	m_bias.resize(outputCount);
	m_biasDelta.resize(outputCount);
	m_activation.resize(outputCount);
	m_error.resize(outputCount);
	m_outgoing.resize(0);
}

// virtual
void WLayerClassicCuda::resetWeights(WRand& rand)
{
	size_t inputCount = inputs();
	size_t outputCount = outputs();
	WMatrix mTmp(inputCount, outputCount);
	double mag = 1.0 / inputCount;
	for(size_t i = 0; i < inputCount; i++)
	{
		WVec& pW = mTmp[i];
		for(size_t j = 0; j < outputCount; j++)
			pW[j] = rand.normal() * mag;
	}
	m_weights.upload(mTmp);
	WVec vTmp(outputCount);
	vTmp.fillNormal(rand, mag);
	m_bias.upload(vTmp);
	m_delta.scale(m_engine, 0.0);
	m_biasDelta.scale(m_engine, 0.0);
}

// virtual
void WLayerClassicCuda::perturbWeights(WRand& rand, double deviation, size_t start, size_t count)
{
	// Perturb weights
	WMatrix m;
	m_weights.download(m);
	size_t n = std::min(outputs() - start, count);
	for(size_t j = 0; j < m_weights.rows(); j++)
		WVec::perturb(m[j].data() + start, deviation, n, rand);
	m_weights.upload(m);

	// Perturb biases
	WVec v(outputs());
	m_bias.download(v);
	WVec::perturb(v.data() + start, deviation, n, rand);
	m_bias.upload(v);
}

// virtual
WVec& WLayerClassicCuda::activation()
{
	m_activation.download(m_outgoing);
	return m_outgoing;
}

// virtual
WVec& WLayerClassicCuda::error()
{
	m_error.download(m_outgoing);
	return m_outgoing;
}

// virtual
void WLayerClassicCuda::copyBiasToNet()
{
	m_activation.copy(m_engine, m_bias);
	m_engine.sync();
}

// virtual
void WLayerClassicCuda::feedIn(const WVec& in)
{
	m_incoming.upload(in);
	m_weights.feedIn(m_engine, m_incoming, m_activation, 0);
	m_engine.sync();
}

// virtual
void WLayerClassicCuda::feedIn(WNeuralNetLayer* pUpStreamLayer)
{
	if(pUpStreamLayer->usesGPU())
	{
		m_weights.feedIn(m_engine, ((WCudaLayer*)pUpStreamLayer)->deviceActivation(), m_activation, 0);
		m_engine.sync();
	}
	else
		feedIn(pUpStreamLayer->activation());
}

// virtual
void WLayerClassicCuda::activate()
{
	m_activation.activateTanh(m_engine);
	m_engine.sync();
}

// virtual
void WLayerClassicCuda::feedForward(const WVec& in)
{
	copyBiasToNet();
	m_incoming.upload(in);
	m_weights.feedIn(m_engine, m_incoming, m_activation, 0);
	m_engine.sync();
	activate();
}

// virtual
void WLayerClassicCuda::feedForward(WNeuralNetLayer* pUpStreamLayer)
{
	copyBiasToNet();
	feedIn(pUpStreamLayer);
	activate();
}

// virtual
void WLayerClassicCuda::dropOut(WRand& rand, double probOfDrop)
{
	throw Ex("sorry, not implemented yet");
}

void WLayerClassicCuda::computeError(const WVec& target)
{
	m_error.upload(target);
	m_error.add(m_engine, m_activation, -1.0);
	m_engine.sync();
}

void WLayerClassicCuda::deactivateError()
{
	m_error.deactivateTanh(m_engine, m_activation);
	m_engine.sync();
}

void WLayerClassicCuda::backPropError(WNeuralNetLayer* pUpStreamLayer)
{
	if(pUpStreamLayer->usesGPU())
	{
		m_weights.backPropError(m_engine, m_error, ((WCudaLayer*)pUpStreamLayer)->deviceError(), 0);
		m_engine.sync();
	}
	else
	{
		if(m_incoming.size() != inputs())
			m_incoming.resize(inputs());
		m_weights.backPropError(m_engine, m_error, m_incoming, 0);
		m_engine.sync();
		m_incoming.download(pUpStreamLayer->error());
	}
}

// virtual
void WLayerClassicCuda::updateBias(double learningRate, double momentum)
{
	m_bias.add(m_engine, m_error, learningRate);
	m_engine.sync();
}

// virtual
void WLayerClassicCuda::updateDeltas(const WVec& upStreamActivation, double momentum)
{
	// Assume that the input was already uploaded into m_incoming when feedForward was called
	if(momentum != 1.0)
	{
		m_delta.scale(m_engine, momentum);
		m_engine.sync();
	}
	m_delta.addOuterProduct(m_engine, m_incoming, m_error, 1.0);
	m_biasDelta.add(m_engine, m_error, 1.0);
	m_engine.sync();
}

// virtual
void WLayerClassicCuda::updateDeltas(WNeuralNetLayer* pUpStreamLayer, double momentum)
{
	if(momentum != 1.0)
	{
		m_delta.scale(m_engine, momentum);
		m_engine.sync();
	}
	if(pUpStreamLayer->usesGPU())
	{
		m_delta.addOuterProduct(m_engine, ((WCudaLayer*)pUpStreamLayer)->deviceActivation(), m_error, 1.0);
	}
	else
	{
		// Assume that the input was already uploaded into m_incoming when feedForward was called
		m_delta.addOuterProduct(m_engine, m_incoming, m_error, 1.0);
	}
	m_biasDelta.add(m_engine, m_error, 1.0);
	m_engine.sync();
}

void WLayerClassicCuda::applyDeltas(double learningRate)
{
	m_weights.add(m_engine, m_delta, learningRate);
	m_engine.sync();
}

// virtual
void WLayerClassicCuda::updateWeightsAndRestoreDroppedOnes(const WVec& upStreamActivation, size_t inputStart, size_t inputCount, double learningRate, double momentum)
{
	throw Ex("Sorry, not implemented yet");
}

// virtual
void WLayerClassicCuda::updateWeightsAndRestoreDroppedOnes(WNeuralNetLayer* pUpStreamLayer, size_t inputStart, double learningRate, double momentum)
{
	throw Ex("Sorry, not implemented yet");
}

void WLayerClassicCuda::scaleWeights(double factor, bool scaleBiases)
{
	m_weights.scale(m_engine, factor);
	if(scaleBiases)
		m_bias.scale(m_engine, factor);
	m_engine.sync();
}

void WLayerClassicCuda::diminishWeights(double amount, bool diminishBiases)
{
	throw Ex("Sorry, WLayerClassicCuda::diminishWeights is not yet implemented");
}

// virtual
void WLayerClassicCuda::maxNorm(double min, double max)
{
	throw Ex("Sorry, WLayerClassicCuda::maxNorm is not yet implemented");
}

// virtual
size_t WLayerClassicCuda::countWeights()
{
	return (inputs() + 1) * outputs();
}

// virtual
size_t WLayerClassicCuda::weightsToVector(double* outVector)
{
	WLayerClassic tmp(inputs(), outputs());
	download(tmp);
	return tmp.weightsToVector(outVector);
}

// virtual
size_t WLayerClassicCuda::vectorToWeights(const double* vector)
{
	WLayerClassic tmp(inputs(), outputs());
	size_t ret = tmp.vectorToWeights(vector);
	upload(tmp);
	return ret;
}

// virtual
void WLayerClassicCuda::copyWeights(const WNeuralNetLayer* pSource)
{
	WLayerClassicCuda* pThat = (WLayerClassicCuda*)pSource;
	m_weights.copy(m_engine, pThat->m_weights);
	m_bias.copy(m_engine, pThat->m_bias);
}

// virtual
void WLayerClassicCuda::renormalizeInput(size_t input, double oldMin, double oldMax, double newMin, double newMax)
{
	throw Ex("Sorry, WLayerClassicCuda::renormalizeInput is not yet implemented");
}

// virtual
void WLayerClassicCuda::refineActivationFunction(double learningRate)
{
	throw Ex("Sorry, WLayerClassicCuda::refineActivationFunction does not yet support this method");
}

// virtual
void WLayerClassicCuda::regularizeActivationFunction(double lambda)
{
	throw Ex("Sorry, WLayerClassicCuda::regularizeActivationFunction does not yet support this method");
}


void WLayerClassicCuda::upload(const WLayerClassic& source)
{
	resize(source.inputs(), source.outputs(), NULL);
	m_weights.upload(source.weights());
	m_bias.upload(source.bias());
}

void WLayerClassicCuda::download(WLayerClassic& dest) const
{
	dest.resize(m_weights.rows(), m_weights.cols(), NULL);
	m_weights.download(dest.weights());
	m_bias.download(dest.bias());
}









WLayerConvolutional2DCuda::WLayerConvolutional2DCuda(WCudaEngine& engine, size_t inputCols, size_t inputRows, size_t inputChannels, size_t kernelRows, size_t kernelCols, size_t kernelCount, size_t stride, size_t padding)
: WCudaLayer(engine),
  m_inputRows(inputRows),
  m_inputCols(inputCols),
  m_inputChannels(inputChannels),
  m_kernelRows(kernelRows),
  m_kernelCols(kernelCols),
  m_kernelCount(kernelCount),
  m_stride(stride),
  m_padding(padding),
  m_outputRows((inputRows - kernelRows + 2 * padding) / stride + 1),
  m_outputCols((inputCols - kernelCols + 2 * padding) / stride + 1)
{
	m_bias.resize(kernelCount);
	m_biasDelta.resize(kernelCount);
	m_kernels.resize(kernelCount, inputChannels * kernelRows * kernelCols);
	m_delta.resize(kernelCount, inputChannels * kernelRows * kernelCols);
	size_t n = kernelCount * m_outputRows * m_outputCols;
	m_net.resize(n);
	m_activation.resize(n);
	m_error.resize(n);
}

WLayerConvolutional2DCuda::WLayerConvolutional2DCuda(WCudaEngine& engine, const WLayerConvolutional2DCuda& upstream, size_t kernelRows, size_t kernelCols, size_t kernelCount, size_t stride, size_t padding)
: WCudaLayer(engine),
  m_inputRows(upstream.outputRows()),
  m_inputCols(upstream.outputCols()),
  m_inputChannels(upstream.outputChannels()),
  m_kernelRows(kernelRows),
  m_kernelCols(kernelCols),
  m_kernelCount(kernelCount),
  m_stride(stride),
  m_padding(padding),
  m_outputRows((m_inputRows - kernelRows + 2 * padding) / stride + 1),
  m_outputCols((m_inputCols - kernelCols + 2 * padding) / stride + 1)
{
	m_bias.resize(kernelCount);
	m_biasDelta.resize(kernelCount);
	m_kernels.resize(kernelCount, m_inputChannels * kernelRows * kernelCols);
	m_delta.resize(kernelCount, m_inputChannels * kernelRows * kernelCols);
	size_t n = kernelCount * m_outputRows * m_outputCols;
	m_net.resize(n);
	m_activation.resize(n);
	m_error.resize(n);
}

WLayerConvolutional2DCuda::~WLayerConvolutional2DCuda()
{
}

WDomNode* WLayerConvolutional2DCuda::serialize(WDom* pDoc)
{
	WLayerConvolutional2D tmp(m_inputCols, m_inputRows, m_inputChannels, m_kernelRows, m_kernelCols, m_kernelCount, m_stride, m_padding);
	download(tmp);
	return tmp.serialize(pDoc);
}

std::string WLayerConvolutional2DCuda::to_str()
{
	std::stringstream ss;
	ss << "[WLayerConvolutional2DCuda:" << m_inputCols << "x" << m_inputRows << "x" << m_inputChannels << "]";
	return ss.str();
}

void WLayerConvolutional2DCuda::resize(size_t inputSize, size_t outputSize)
{
	if(inputSize != inputs() || outputSize != outputs())
		throw Ex("WLayerConvolutional2DCuda cannot be resized");
}

void WLayerConvolutional2DCuda::resizeInputs(WNeuralNetLayer* pUpStreamLayer)
{
	throw Ex("Sorry, this method is not yet implemented.");
}

void WLayerConvolutional2DCuda::dropOut(WRand& rand, double probOfDrop)
{
	throw Ex("dropOut not implemented");
}

void WLayerConvolutional2DCuda::dropConnect(WRand& rand, double probOfDrop)
{
	throw Ex("dropConnect not implemented");
}

void WLayerConvolutional2DCuda::computeError(const WVec& target)
{
	m_error.upload(target);
	m_error.add(m_engine, m_activation, -1.0);
	m_engine.sync();
}

void WLayerConvolutional2DCuda::feedForward(const WVec& in)
{
	m_incoming.upload(in);
	Conv2D_feedForward(m_engine, m_activation, m_net, m_incoming, m_kernels, m_bias, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputRows, m_inputCols, m_inputChannels, m_padding, m_stride);
	m_engine.sync();
}

void WLayerConvolutional2DCuda::feedForward(WNeuralNetLayer* pUpStreamLayer)
{
	if(pUpStreamLayer->usesGPU())
	{
		Conv2D_feedForward(m_engine, m_activation, m_net, ((WCudaLayer*)pUpStreamLayer)->deviceActivation(), m_kernels, m_bias, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputRows, m_inputCols, m_inputChannels, m_padding, m_stride);
	}
	else
		feedForward(pUpStreamLayer->activation());
}

void WLayerConvolutional2DCuda::deactivateError()
{
	Conv2D_deactivate(m_engine, m_error, m_net, m_activation, outputs());
}

void WLayerConvolutional2DCuda::backPropError(WNeuralNetLayer* pUpStreamLayer)
{
	if(pUpStreamLayer->usesGPU())
	{
		Conv2D_backPropError(m_engine, ((WCudaLayer*)pUpStreamLayer)->deviceError(), m_error, m_kernels, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputChannels, m_inputRows, m_inputCols, m_padding, m_stride);
		m_engine.sync();
	}
	else
	{
		if(m_incoming.size() != inputs())
			m_incoming.resize(inputs());
		Conv2D_backPropError(m_engine, m_incoming, m_error, m_kernels, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputChannels, m_inputRows, m_inputCols, m_padding, m_stride);
		m_engine.sync();
		m_incoming.download(pUpStreamLayer->error());
	}
}

void WLayerConvolutional2DCuda::updateDeltas(const WVec& upStreamActivation, double momentum)
{
	// Ignore upStreamActivation and assume that the input was already uploaded into m_incoming when feedForward was called
	Conv2D_updateDeltas(m_engine, m_delta, m_biasDelta, m_incoming, m_error, momentum, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputChannels, m_inputRows, m_inputCols, m_padding, m_stride);
}

// virtual
void WLayerConvolutional2DCuda::updateDeltas(WNeuralNetLayer* pUpStreamLayer, double momentum)
{
	if(pUpStreamLayer->usesGPU())
	{
		Conv2D_updateDeltas(m_engine, m_delta, m_biasDelta, ((WCudaLayer*)pUpStreamLayer)->deviceActivation(), m_error, momentum, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputChannels, m_inputRows, m_inputCols, m_padding, m_stride);
	}
	else
	{
		Conv2D_updateDeltas(m_engine, m_delta, m_biasDelta, m_incoming, m_error, momentum, m_kernelCount, m_kernelRows, m_kernelCols, m_outputRows, m_outputCols, m_inputChannels, m_inputRows, m_inputCols, m_padding, m_stride);
	}
	m_engine.sync();
}

void WLayerConvolutional2DCuda::applyDeltas(double learningRate)
{
	m_bias.add(m_engine, m_biasDelta, learningRate);
	m_kernels.add(m_engine, m_delta, learningRate);
}

void WLayerConvolutional2DCuda::scaleWeights(double factor, bool scaleBiases)
{
	throw Ex("scaleWeights not implemented");
}

void WLayerConvolutional2DCuda::diminishWeights(double amount, bool regularizeBiases)
{
	throw Ex("diminishWeights not implemented");
}

size_t WLayerConvolutional2DCuda::countWeights()
{
	return m_inputChannels * m_kernelRows * m_kernelCols + m_kernelCount;
}

size_t WLayerConvolutional2DCuda::weightsToVector(double* pOutVector)
{
	throw Ex("weightsToVector not implemented");
}

size_t WLayerConvolutional2DCuda::vectorToWeights(const double* pVector)
{
	throw Ex("vectorToWeights not implemented");
}

void WLayerConvolutional2DCuda::copyWeights(const WNeuralNetLayer* pSource)
{
	throw Ex("copyWeights not implemented");
}

void WLayerConvolutional2DCuda::resetWeights(WRand& rand)
{
	double mag = std::max(0.03, 1.0 / (m_outputRows * m_outputCols));
	m_kernels.fillNormal(m_engine, 0.0, mag);
	//	m_bias.randomNormal(m_engine, 0.0, mag);
	m_bias.fill(m_engine, 0.0);
	m_delta.fill(m_engine, 0.0);
	m_biasDelta.fill(m_engine, 0.0);
}

void WLayerConvolutional2DCuda::perturbWeights(WRand& rand, double deviation, size_t start, size_t count)
{
	throw Ex("perturbWeights not implemented");
	/*	WAssert(start + count < m_kernelCount);
	size_t n = std::min(m_kernelCount - start, count);
	for(size_t j = start; j < n; j++)
		WVec::perturb(m_kernels[j].data(), deviation, m_kernels.cols(), rand);
	WVec::perturb(m_bias.data(), deviation, m_bias.size(), rand);*/
}

void WLayerConvolutional2DCuda::maxNorm(double min, double max)
{
	throw Ex("maxNorm not implemented");
}

void WLayerConvolutional2DCuda::regularizeActivationFunction(double lambda)
{
	throw Ex("regularizeActivationFunction not implemented");
}

void WLayerConvolutional2DCuda::renormalizeInput(size_t input, double oldMin, double oldMax, double newMin, double newMax)
{
	throw Ex("renormalizeInput not implemented");
}

void WLayerConvolutional2DCuda::upload(const WLayerConvolutional2D& source)
{
	m_kernels.upload(source.kernels());
	m_bias.upload(source.bias());
}

void WLayerConvolutional2DCuda::download(WLayerConvolutional2D& dest) const
{
	m_kernels.download(dest.kernels());
	m_bias.download(dest.bias());
}

// virtual
WVec& WLayerConvolutional2DCuda::activation()
{
	m_activation.download(m_outgoing);
	return m_outgoing;
}

// virtual
WVec& WLayerConvolutional2DCuda::error()
{
	m_error.download(m_outgoing);
	return m_outgoing;
}



} // namespace WClasses

