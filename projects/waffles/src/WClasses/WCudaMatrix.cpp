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

#ifdef WCUDA

#include "WCudaMatrix.h"
#include "WError.h"
#include "WMatrix.h"
#include <cuda.h>
#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <curand.h>

namespace WClasses
{

bool g_haveEngine = false;

WCudaEngine::WCudaEngine()
{
	if(g_haveEngine)
		throw Ex("There should only be one WCudaEngine in existence at any time");
	g_haveEngine = true;
	cublasStatus_t status = cublasCreate((cublasHandle_t*)&m_handle);
	if(status != CUBLAS_STATUS_SUCCESS)
	{
		if(status == CUBLAS_STATUS_NOT_INITIALIZED)
			throw Ex("the CUDA™ Runtime initialization failed");
		else if(status == CUBLAS_STATUS_ALLOC_FAILED)
			throw Ex("the resources could not be allocated");
		else
			throw Ex("unrecognized error while calling cublasCreate");
	}
	m_blockSize = 64;
	if(curandCreateGenerator((curandGenerator_t*)&m_prng, CURAND_RNG_PSEUDO_DEFAULT) != CURAND_STATUS_SUCCESS)
		throw Ex("curandCreateGenerator failed");
	if(curandSetPseudoRandomGeneratorSeed((curandGenerator_t)m_prng, 1234ULL) != CURAND_STATUS_SUCCESS)
		throw Ex("curandSetPseudoRandomGeneratorSeed failed");
	m_hogWild = false;
}

WCudaEngine::~WCudaEngine()
{
	if(cublasDestroy((cublasHandle_t)m_handle) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDestroy failed");
	g_haveEngine = false;
}

void WCudaEngine::sync()
{
	if(!m_hogWild)
	{
		if(cudaDeviceSynchronize() != cudaSuccess)
			throw Ex(cudaGetErrorString(cudaGetLastError()));
	}
}








WCudaVector::WCudaVector(size_t size)
: m_size(0), d_vals(NULL), m_own(true)
{
	if(size > 0)
		resize(size);
}

WCudaVector::WCudaVector(WCudaVector& wrapme, size_t start, size_t len)
: m_size(len), d_vals(wrapme.d_vals + start), m_own(false)
{
	if(!wrapme.d_vals)
		throw Ex("Cannot wrap null");
}

WCudaVector::WCudaVector(WCudaMatrix& wrapme, size_t row, size_t len)
: m_size(len), d_vals(wrapme.d_vals + (len * row)), m_own(false)
{
	if(!wrapme.d_vals)
		throw Ex("Cannot wrap null");
}

WCudaVector::~WCudaVector()
{
	if(m_own && d_vals)
		cudaFree(d_vals);
}

void WCudaVector::resize(size_t size)
{
	if(m_size == size)
		return;
	if(!m_own)
		throw Ex("Cannot resize a wrapped vector");
	if(d_vals)
		cudaFree(d_vals);
	if(cudaMalloc((void**)&d_vals, size * sizeof(double)) != cudaSuccess)
		throw Ex(cudaGetErrorString(cudaGetLastError()));
	m_size = size;
}

void WCudaVector::upload(const WVec& hostVector, size_t start, size_t len)
{
	len = std::min(len, hostVector.size() - start);
	resize(len);
	if(cublasSetVector(m_size, sizeof(double), hostVector.data() + start, 1, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasSetVector failed");
}

void WCudaVector::download(WVec& hostVector) const
{
	hostVector.resize(m_size);
	if(cublasGetVector(m_size, sizeof(double), d_vals, 1, hostVector.data(), 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasGetVector failed");
}

void WCudaVector::copy(WCudaEngine& engine, const WCudaVector& that)
{
	resize(that.m_size);
	if(cublasDcopy((cublasHandle_t)engine.m_handle, m_size, that.d_vals, 1, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDcopy failed");
}

void WCudaVector::add(WCudaEngine& engine, const WCudaVector& that, double thatScalar)
{
	WAssert(m_size == that.m_size);
	if(cublasDaxpy((cublasHandle_t)engine.m_handle, m_size, &thatScalar, that.d_vals, 1, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDaxpy failed");
}

void WCudaVector::scale(WCudaEngine& engine, double scalar)
{
	if(cublasDscal((cublasHandle_t)engine.m_handle, m_size, &scalar, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDscal failed");
}

void WCudaVector::fillUniform(WCudaEngine& engine)
{
	if(curandGenerateUniformDouble((curandGenerator_t)engine.m_prng, d_vals, m_size) != CURAND_STATUS_SUCCESS)
		throw Ex("curandGenerateUniformDouble failed");
}

void WCudaVector::fillNormal(WCudaEngine& engine, double mean, double dev)
{
	if(curandGenerateNormalDouble((curandGenerator_t)engine.m_prng, d_vals, m_size, mean, dev) != CURAND_STATUS_SUCCESS)
		throw Ex("curandGenerateNormalDouble failed in WCudaVector::randomNormal");
}

void WCudaVector::addOuterProduct(WCudaEngine& engine, const WCudaVector& upStreamInput, const WCudaVector& downStreamError, double learningRate)
{
	WAssert(m_size == upStreamInput.size() * downStreamError.size());
	if(cublasDger((cublasHandle_t)engine.m_handle, downStreamError.size(), upStreamInput.size(), &learningRate, downStreamError.d_vals, 1, upStreamInput.d_vals, 1, d_vals, downStreamError.size()) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDger failed");
}








WCudaMatrix::WCudaMatrix()
: m_rows(0), m_cols(0), d_vals(NULL)
{
}

WCudaMatrix::~WCudaMatrix()
{
	if(d_vals)
		cudaFree(d_vals);
}

void WCudaMatrix::resize(size_t rows, size_t cols)
{
	if(rows == m_rows && cols == m_cols)
		return;
	if(d_vals)
		cudaFree(d_vals);
	if(cudaMalloc((void**)&d_vals, rows * cols * sizeof(double)) != cudaSuccess)
		throw Ex(cudaGetErrorString(cudaGetLastError()));
	m_rows = rows;
	m_cols = cols;
}

void WCudaMatrix::upload(const WMatrix& m, size_t rowStart, size_t colStart, size_t rowCount, size_t colCount)
{
	rowCount = std::min(rowCount, m.rows() - rowStart);
	colCount = std::min(colCount, m.cols() - colStart);
	resize(rowCount, colCount);
	double* pVals = d_vals;
	for(size_t i = 0; i < m_rows; i++)
	{
		if(cublasSetVector(colCount, sizeof(double), m[i].data() + colStart, 1, pVals, 1) != CUBLAS_STATUS_SUCCESS)
			throw Ex("cublasSetVector failed");
		pVals += colCount;
	}
}

void WCudaMatrix::download(WMatrix& m) const
{
	m.resize(m_rows, m_cols);
	double* pVals = d_vals;
	for(size_t i = 0; i < m_rows; i++)
	{
		if(cublasGetVector(m_cols, sizeof(double), pVals, 1, m[i].data(), 1) != CUBLAS_STATUS_SUCCESS)
			throw Ex("cublasGetVector failed");
		pVals += m_cols;
	}
}

void WCudaMatrix::copy(WCudaEngine& engine, WCudaMatrix& that)
{
	resize(that.rows(), that.cols());
	if(cublasDcopy((cublasHandle_t)engine.m_handle, m_rows * m_cols, that.d_vals, 1, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDcopy failed");
}

void WCudaMatrix::scale(WCudaEngine& engine, double scalar)
{
	if(cublasDscal((cublasHandle_t)engine.m_handle, m_rows * m_cols, &scalar, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDscal failed");
}

void WCudaMatrix::add(WCudaEngine& engine, WCudaMatrix& that, double thatScalar)
{
	WAssert(that.rows() == m_rows && that.cols() == m_cols);
	if(cublasDaxpy((cublasHandle_t)engine.m_handle, m_rows * m_cols, &thatScalar, that.d_vals, 1, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDaxpy failed");
}

void WCudaMatrix::add(WCudaEngine& engine, WCudaVector& that, double thatScalar)
{
	WAssert(that.size() == m_rows * m_cols);
	if(cublasDaxpy((cublasHandle_t)engine.m_handle, m_rows * m_cols, &thatScalar, that.d_vals, 1, d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDaxpy failed");
}

void WCudaMatrix::rowVectorTimesThis(WCudaEngine& engine, const WCudaVector& in, WCudaVector& out)
{
	WAssert(in.m_size == m_rows);
	out.resize(m_cols);
	double alpha = 1.0f;
	double beta = 0.0f;
	if(cublasDgemv((cublasHandle_t)engine.m_handle, CUBLAS_OP_N, m_cols, m_rows, &alpha, d_vals, m_cols, in.d_vals, 1, &beta, out.d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDgemv failed");
}

void WCudaMatrix::thisTimesColumnVector(WCudaEngine& engine, const WCudaVector& in, WCudaVector& out)
{
	WAssert(in.m_size == m_cols);
	out.resize(m_rows);
	double alpha = 1.0f;
	double beta = 0.0f;
	if(cublasDgemv((cublasHandle_t)engine.m_handle, CUBLAS_OP_T, m_cols, m_rows, &alpha, d_vals, m_cols, in.d_vals, 1, &beta, out.d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDgemv failed");
}

void WCudaMatrix::feedIn(WCudaEngine& engine, const WCudaVector& in, WCudaVector& out, size_t inputStart) const
{
	WAssert(inputStart + in.m_size <= m_rows);
	WAssert(out.m_size == m_cols);
	double alpha = 1.0f;
	double beta = 1.0f;
	if(cublasDgemv((cublasHandle_t)engine.m_handle, CUBLAS_OP_N, m_cols, in.m_size, &alpha, d_vals + inputStart * m_cols, m_cols, in.d_vals, 1, &beta, out.d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDgemv failed");
}

void WCudaMatrix::backPropError(WCudaEngine& engine, const WCudaVector& in, WCudaVector& out, size_t inputStart) const
{
	WAssert(in.m_size == m_cols);
	double alpha = 1.0f;
	double beta = 0.0f;
	if(cublasDgemv((cublasHandle_t)engine.m_handle, CUBLAS_OP_T, in.size(), m_rows, &alpha, d_vals, m_cols, in.d_vals + inputStart, 1, &beta, out.d_vals, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDgemv failed");
}

void WCudaMatrix::addOuterProduct(WCudaEngine& engine, WCudaVector& upStreamInput, WCudaVector& downStreamError, double learningRate)
{
	if(cublasDger((cublasHandle_t)engine.m_handle, m_cols, upStreamInput.size(), &learningRate, downStreamError.d_vals, 1, upStreamInput.d_vals, 1, d_vals, m_cols) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDger failed");
}

double WCudaMatrix::rowSumAbs(WCudaEngine& engine, size_t row)
{
	double res;
	if(cublasDasum((cublasHandle_t)engine.m_handle, m_cols, d_vals + row * m_cols, 1, &res) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDasum failed");
	return res;
}

double WCudaMatrix::rowSumSquare(WCudaEngine& engine, size_t row)
{
	double res;
	if(cublasDdot((cublasHandle_t)engine.m_handle, m_cols, d_vals + row * m_cols, 1, d_vals + row * m_cols, 1, &res) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDdot failed");
	return res;
}

double WCudaMatrix::colSumAbs(WCudaEngine& engine, size_t col)
{
	double res;
	if(cublasDasum((cublasHandle_t)engine.m_handle, m_rows, d_vals + col, m_cols, &res) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDasum failed");
	return res;
}

double WCudaMatrix::colSumSquare(WCudaEngine& engine, size_t col)
{
	double res;
	if(cublasDdot((cublasHandle_t)engine.m_handle, m_rows, d_vals + col, m_cols, d_vals + col, m_cols, &res) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDdot failed");
	return res;
}

void WCudaMatrix::scaleRow(WCudaEngine& engine, size_t row, double scalar)
{
	if(cublasDscal((cublasHandle_t)engine.m_handle, m_cols, &scalar, d_vals + row * m_cols, 1) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDscal failed");
}

void WCudaMatrix::scaleCol(WCudaEngine& engine, size_t col, double scalar)
{
	if(cublasDscal((cublasHandle_t)engine.m_handle, m_rows, &scalar, d_vals + col, m_cols) != CUBLAS_STATUS_SUCCESS)
		throw Ex("cublasDscal failed");
}

void WCudaMatrix::fillNormal(WCudaEngine& engine, double mean, double dev)
{
	if(curandGenerateNormalDouble((curandGenerator_t)engine.m_prng, d_vals, m_rows * m_cols, mean, dev) != CURAND_STATUS_SUCCESS)
		throw Ex("curandGenerateNormalDouble failed in WCudaMatrix::fillNormal");
}

} // namespace WClasses

#endif // WCUDA
