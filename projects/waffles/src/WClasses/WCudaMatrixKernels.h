#ifndef WCUDAMATRIXKERNELS_H
#define WCUDAMATRIXKERNELS_H


namespace WClasses
{

class WCudaEngine;
class WCudaVector;

void Conv2D_feedForward(WCudaEngine& engine, WCudaVector& activation, WCudaVector& net, WCudaVector& incoming, const WCudaMatrix& kernels, const WCudaVector& bias, size_t kernelCount, size_t kernelRows, size_t kernelCols, size_t outputRows, size_t outputCols, size_t inputRows, size_t inputCols, size_t inputChannels, size_t padding, size_t stride);

void Conv2D_deactivate(WCudaEngine& engine, WCudaVector& error, const WCudaVector& net, const WCudaVector& activation, size_t outputs);

void Conv2D_backPropError(WCudaEngine& engine, WCudaVector& upStreamError, const WCudaVector& err, const WCudaMatrix& kernels, size_t kernelCount, size_t kernelRows, size_t kernelCols, size_t outputRows, size_t outputCols, size_t inputChannels, size_t inputRows, size_t inputCols, size_t padding, size_t stride);

void Conv2D_updateDeltas(WCudaEngine& engine, WCudaMatrix& delta, WCudaVector& biasDelta, const WCudaVector& upStreamActivation, const WCudaVector& err, double momentum, size_t kernelCount, size_t kernelRows, size_t kernelCols, size_t outputRows, size_t outputCols, size_t inputChannels, size_t inputRows, size_t inputCols, size_t padding, size_t stride);

}

#endif // WCUDAMATRIXKERNELS_H

