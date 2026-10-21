/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Luke B. Godfrey,
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

#include "WOptimizer.h"
#include "WNeuralNet.h"
#include "WVec.h"
#include "WRand.h"
#include <string.h>
#include <math.h>

namespace WClasses
{

using std::vector;


WNeuralNetOptimizer::WNeuralNetOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures, const WMatrix* pTrainingLabels)
: m_model(model),
  m_pTrainingFeatures(pTrainingFeatures),
  m_pTrainingLabels(pTrainingLabels),
#ifdef WCUDA
  m_pTrainingFeaturesCuda(nullptr),
  m_pTrainingLabelsCuda(nullptr),
  m_useGPU(true),
#endif // WCUDA
  m_rand(rand),
  m_batchSize(1), m_batchesPerEpoch(INVALID_INDEX), m_epochs(100), m_windowSize(100), m_minImprovement(0.002), m_learningRate(0.05),
  m_pII(nullptr)
{
	if(m_pTrainingFeatures && m_pTrainingLabels && m_pTrainingFeatures->rows() != m_pTrainingLabels->rows())
		throw Ex("Mismatching numbers of training features and labels");
	if(m_model.layerCount() == 0)
		throw Ex("Layers must be added to the neural net before the optimizer is constructed");
}

WNeuralNetOptimizer::~WNeuralNetOptimizer()
{
	delete(m_pII);
#ifdef WCUDA
	delete(m_pTrainingFeaturesCuda);
	delete(m_pTrainingLabelsCuda);
#endif // WCUDA
}

void WNeuralNetOptimizer::resetState()
{
	m_model.resetState();
}

void WNeuralNetOptimizer::optimizeIncremental(const WVec& feat, const WVec& lab)
{
	WAssert(feat.size() == m_model.layer(0).inputs() && lab.size() == m_model.outputLayer().outputs(), "Features/labels size mismatch!");
	WAssert(feat.size() != 0 && lab.size() != 0, "Features/labels are empty!");
	computeGradient(feat, lab);
	descendGradient(m_learningRate);
}

#ifdef WCUDA
void WNeuralNetOptimizer::optimizeIncrementalCuda(const WCudaVector& feat, const WCudaVector& lab)
{
	WAssert(feat.size() == m_model.layer(0).inputs() && lab.size() == m_model.outputLayer().outputs(), "Features/labels size mismatch!");
	WAssert(feat.size() != 0 && lab.size() != 0, "Features/labels are empty!");
	computeGradientCuda(feat, lab);
	descendGradientCuda(m_learningRate);
}
#endif // WCUDA

void WNeuralNetOptimizer::optimizeEpoch()
{
	if(!m_pII || m_pII->length() != m_pTrainingFeatures->rows())
	{
		delete(m_pII);
		m_pII = new WRandomIndexIterator(m_pTrainingFeatures->rows(), m_rand);
	}
	m_pII->reset();
	size_t index;
#ifdef WCUDA
	if(m_useGPU)
	{
		if(!m_pTrainingFeaturesCuda)
		{
			WAssert(!m_pTrainingLabelsCuda);
			m_pTrainingFeaturesCuda = new WCudaMatrix();
			m_pTrainingFeaturesCuda->upload(*m_pTrainingFeatures);
			m_pTrainingLabelsCuda = new WCudaMatrix();
			m_pTrainingLabelsCuda->upload(*m_pTrainingLabels);
		}
		m_model.uploadCuda();
		while(m_pII->next(index))
			optimizeIncrementalCuda((*m_pTrainingFeaturesCuda)[index], (*m_pTrainingLabelsCuda)[index]);
		m_model.downloadCuda();
	}
	else
	{
#endif // WCUDA
		while(m_pII->next(index))
			optimizeIncremental((*m_pTrainingFeatures)[index], (*m_pTrainingLabels)[index]);
#ifdef WCUDA
	}
#endif // WCUDA
}

void WNeuralNetOptimizer::optimizeBatch(const WMatrix& features, const WMatrix& labels, size_t start, size_t batchSize)
{
	WAssert(features.cols() == m_model.layer(0).inputs() && labels.cols() == m_model.outputLayer().outputs(), "Features/labels size mismatch!");
	for(size_t i = 0; i < batchSize; ++i)
		computeGradient(features[start + i], labels[start + i]);
	descendGradient(m_learningRate / batchSize);
}

void WNeuralNetOptimizer::optimizeBatch(const WMatrix& features, const WMatrix& labels, size_t start)
{
	optimizeBatch(features, labels, start, m_batchSize);
}

void WNeuralNetOptimizer::optimizeBatch(const WMatrix& features, const WMatrix& labels, WRandomIndexIterator& ii, size_t batchSize)
{
	WAssert(features.cols() == m_model.layer(0).inputs() && labels.cols() == m_model.outputLayer().outputs(), "Features/labels size mismatch!");
	size_t j;
	for(size_t i = 0; i < batchSize; ++i)
	{
		if(!ii.next(j)) ii.reset(), ii.next(j);
		computeGradient(features[j], labels[j]);
	}
	descendGradient(m_learningRate / batchSize);
}

void WNeuralNetOptimizer::optimizeBatch(const WMatrix& features, const WMatrix& labels, WRandomIndexIterator& ii)
{
	optimizeBatch(features, labels, ii, m_batchSize);
}

void WNeuralNetOptimizer::optimize(const WMatrix& features, const WMatrix& labels)
{
	WAssert(features.cols() == m_model.layer(0).inputs() && labels.cols() == m_model.outputLayer().outputs(), "Features/labels size mismatch!");

	size_t batchesPerEpoch = m_batchesPerEpoch;
	if(m_batchesPerEpoch > features.rows())
		batchesPerEpoch = features.rows();

	WRandomIndexIterator ii(features.rows(), m_rand);
	for(size_t i = 0; i < m_epochs; ++i)
		for(size_t j = 0; j < batchesPerEpoch; ++j)
			optimizeBatch(features, labels, ii, m_batchSize);
}

void WNeuralNetOptimizer::optimizeWithValidation(const WMatrix& features, const WMatrix& labels, const WMatrix& validationFeat, const WMatrix& validationLab)
{
	size_t batchesPerEpoch = m_batchesPerEpoch;
	if(m_batchesPerEpoch > features.rows())
		batchesPerEpoch = features.rows();

	double bestError = 1e308, currentError;
	size_t k = 0;
	WRandomIndexIterator ii(features.rows(), m_rand);
	for([[maybe_unused]] size_t i = 0;; ++i, ++k)
	{
		for(size_t j = 0; j < batchesPerEpoch; ++j)
			optimizeBatch(features, labels, ii, m_batchSize);
		if(k >= m_windowSize)
		{
			k = 0;
			currentError = m_model.measureLoss(validationFeat, validationLab);
			if(1.0 - currentError / bestError >= m_minImprovement)
			{
				if(currentError < bestError)
				{
					if(currentError == 0.0)
						break;
					bestError = currentError;
				}
			}
			else
				break;
		}
	}
}

void WNeuralNetOptimizer::optimizeWithValidation(const WMatrix& features, const WMatrix& labels, double validationPortion)
{
	size_t validationRows = (size_t)(validationPortion * features.rows());
	size_t trainRows = features.rows() - validationRows;
	if(validationRows > 0)
	{
		WDataRowSplitter splitter(features, labels, m_rand, trainRows);
		optimizeWithValidation(splitter.features1(), splitter.labels1(), splitter.features2(), splitter.labels2());
	}
	else
		optimizeWithValidation(features, labels, features, labels);
}










WSGDOptimizer::WSGDOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures, const WMatrix* pTrainingLabels)
: WNeuralNetOptimizer(model, rand, pTrainingFeatures, pTrainingLabels), m_momentum(0)
{
	init();
}

void WSGDOptimizer::init()
{
	m_model.init(m_rand);
#ifdef WCUDA
	if(m_useGPU)
	{
		m_gradientCuda.resize(m_model.gradCount());
		WContextNeuralNet& ctx = context();
		if(&ctx.cudaEngine() == nullptr)
			throw Ex("This context has no cuda engine");
		m_gradientCuda.fill(ctx.cudaEngine(), 0.0);
	}
#endif
}

void WSGDOptimizer::computeGradient(const WVec& feat, const WVec& lab)
{
	m_model.forwardProp(feat);
	m_model.computeBlame(lab);
	m_model.backpropagate();
	m_model.updateGradient();
}

void WSGDOptimizer::descendGradient(double learningRate)
{
	m_model.step(learningRate, m_momentum);
}

#ifdef WCUDA
void WSGDOptimizer::computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab)
{
	WContextNeuralNet& ctx = context();
	WCudaVector& pred = ctx.forwardPropCuda(feat);
	m_objective->calculateOutputLayerBlameCuda(ctx.cudaEngine(), pred, lab, ctx.blameCuda());
	ctx.backPropCuda();
	m_gradientCuda.scale(ctx.cudaEngine(), m_momentum);
	ctx.updateGradientCuda(feat, m_gradientCuda);
}

void WSGDOptimizer::descendGradientCuda(double learningRate)
{
	m_model.stepCuda(context(), learningRate, m_gradientCuda);
}
#endif // WCUDA











WAdamOptimizer::WAdamOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures, const WMatrix* pTrainingLabels)
: WNeuralNetOptimizer(model, rand, pTrainingFeatures, pTrainingLabels), m_correct1(1.0), m_correct2(1.0), m_beta1(0.9), m_beta2(0.999), m_epsilon(1e-8)
{
	m_learningRate = 0.001;
	init();
}

void WAdamOptimizer::init()
{
	m_model.init(m_rand);
	m_deltas.resize(m_gradient.size());
	m_sqdeltas.resize(m_gradient.size());
	m_deltas.fill(0.0);
	m_sqdeltas.fill(0.0);
}

void WAdamOptimizer::computeGradient(const WVec& feat, const WVec& lab)
{
	m_model.forwardProp(feat);
	m_model.computeBlame(lab);
	m_model.backpropagate();
	m_gradient.fill(0.0);
	m_model.updateGradient();
	m_correct1 *= m_beta1;
	m_correct2 *= m_beta2;
	for(size_t i = 0; i < m_gradient.size(); i++)
	{
		m_deltas[i] *= m_beta1;
		m_deltas[i] += (1.0 - m_beta1) * m_gradient[i];
		m_sqdeltas[i] *= m_beta2;
		m_sqdeltas[i] += (1.0 - m_beta2) * (m_gradient[i] * m_gradient[i]);
	}
}

void WAdamOptimizer::descendGradient(double learningRate)
{
	double alpha1 = 1.0 / (1.0 - m_correct1);
	double alpha2 = 1.0 / (1.0 - m_correct2);
	for(size_t i = 0; i < m_gradient.size(); i++)
		m_gradient[i] = alpha1 * m_deltas[i] / (std::sqrt(alpha2 * m_sqdeltas[i]) + m_epsilon);
	m_model.step(learningRate, 0.0);
}

#ifdef WCUDA
void WAdamOptimizer::computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab)
{
	throw Ex("Sorry, not implemented yet");
}

void WAdamOptimizer::descendGradientCuda(double learningRate)
{
	throw Ex("Sorry, not implemented yet");
}
#endif // WCUDA











WRMSPropOptimizer::WRMSPropOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures, const WMatrix* pTrainingLabels)
: WNeuralNetOptimizer(model, rand, pTrainingFeatures, pTrainingLabels), m_momentum(0), m_gamma(0.9), m_epsilon(1e-6)
{
	init();
}

void WRMSPropOptimizer::init()
{
	m_model.init(m_rand);
	m_meanSquare.resize(m_gradient.size());
	m_meanSquare.fill(0.0);
}

void WRMSPropOptimizer::computeGradient(const WVec& feat, const WVec& lab)
{
	m_model.forwardProp(feat);
	m_model.computeBlame(lab);
	m_model.backpropagate();
	m_model.updateGradient();
}

void WRMSPropOptimizer::descendGradient(double learningRate)
{
	for(size_t i = 0; i < m_meanSquare.size(); ++i)
	{
		m_meanSquare[i] *= m_gamma;
		m_meanSquare[i] += (1.0 - m_gamma) * m_gradient[i] * m_gradient[i];
		m_gradient[i] /= sqrt(m_meanSquare[i]) + m_epsilon;
	}
	m_model.step(learningRate, 0.0);
}

#ifdef WCUDA
void WRMSPropOptimizer::computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab)
{
	throw Ex("Sorry, not implemented yet");
}

void WRMSPropOptimizer::descendGradientCuda(double learningRate)
{
	throw Ex("Sorry, not implemented yet");
}
#endif // WCUDA














WTargetFunction::WTargetFunction(size_t dims)
{
	m_pRelation = new WUniformRelation(dims, 0);
}

// virtual
WTargetFunction::~WTargetFunction()
{
	delete(m_pRelation);
}

// virtual
void WTargetFunction::initVector(WVec& pVector)
{
	pVector.fill(0.0);
}










// virtual
double WOptimizerBasicTestTargetFunction::computeError(const WVec& pVector)
{
	double a = pVector[0] - 0.123456789;
	double b = pVector[1] + 9.876543210;
	double c = pVector[2] - 3.333333333;
	return sqrt(a * a + b * b + c * c);
}










WOptimizer::WOptimizer(WTargetFunction* pCritic)
: m_pCritic(pCritic)
{
}

// virtual
WOptimizer::~WOptimizer()
{
}

double WOptimizer::searchUntil(size_t nBurnInIterations, size_t nIterations, double dImprovement)
{
	for(size_t i = 0; i < nBurnInIterations; i++)
		iterate();
	double dPrevError;
	double dError = iterate();
	while(true)
	{
		dPrevError = dError;
		for(size_t i = 0; i < nIterations; i++)
			iterate();
		dError = iterate();
		if((dPrevError - dError) / dPrevError >= dImprovement && dError > 0.0)
		{
		}
		else
			break;
	}
	return dError;
}

void WOptimizer::basicTest(double minAccuracy, double warnRange)
{
	double d = searchUntil(5, 100, 0.001);
	if(d > minAccuracy)
		throw Ex("Optimizer accuracy has regressed. Expected ", to_str(minAccuracy), ". Got ", to_str(d));
	if(d < minAccuracy - warnRange)
		std::cout << "Accuracy is much better than expected. Expected " << to_str(minAccuracy) << ". Got " << to_str(d) << ". Please tighten the expected accuracy for this test.\n";
}








WParallelOptimizers::WParallelOptimizers(size_t dims)
{
	if(dims > 0)
		m_pRelation = new WUniformRelation(dims, 0);
}

WParallelOptimizers::~WParallelOptimizers()
{
	for(vector<WOptimizer*>::iterator it = m_optimizers.begin(); it != m_optimizers.end(); it++)
		delete(*it);
	for(vector<WTargetFunction*>::iterator it = m_targetFunctions.begin(); it != m_targetFunctions.end(); it++)
		delete(*it);
}

void WParallelOptimizers::add(WTargetFunction* pTargetFunction, WOptimizer* pOptimizer)
{
	m_targetFunctions.push_back(pTargetFunction);
	m_optimizers.push_back(pOptimizer);
}

double WParallelOptimizers::iterateAll()
{
	double err = 0;
	for(vector<WOptimizer*>::iterator it = m_optimizers.begin(); it != m_optimizers.end(); it++)
		err += (*it)->iterate();
	return err;
}

double WParallelOptimizers::searchUntil(size_t nBurnInIterations, size_t nIterations, double dImprovement)
{
	for(size_t i = 1; i < nBurnInIterations; i++)
		iterateAll();
	double dPrevError;
	double dError = iterateAll();
	while(true)
	{
		dPrevError = dError;
		for(size_t i = 1; i < nIterations; i++)
			iterateAll();
		dError = iterateAll();
		if((dPrevError - dError) / dPrevError < dImprovement)
			break;
	}
	return dError;
}









class WAction
{
protected:
	int m_nAction;
	WAction* m_pPrev;
	unsigned int m_nRefs;

	~WAction()
	{
		if(m_pPrev)
			m_pPrev->Release(); // todo: this could overflow the stack with recursion
	}
public:
	WAction(int nAction, WAction* pPrev)
	: m_nAction(nAction), m_nRefs(0)
	{
		m_pPrev = pPrev;
		if(pPrev)
			pPrev->AddRef();
	}

	void AddRef()
	{
		m_nRefs++;
	}

	void Release()
	{
		if(--m_nRefs == 0)
			delete(this);
	}

	WAction* GetPrev() { return m_pPrev; }
	int GetAction() { return m_nAction; }
};












WActionPath::WActionPath(WActionPathState* pState) : m_pLastAction(nullptr), m_nPathLen(0)
{
	m_pHeadState = pState;
}

WActionPath::~WActionPath()
{
	if(m_pLastAction)
		m_pLastAction->Release();
	delete(m_pHeadState);
}

void WActionPath::doAction(size_t nAction)
{
	WAction* pPrevAction = m_pLastAction;
	m_pLastAction = new WAction((int)nAction, pPrevAction);
	m_pLastAction->AddRef(); // referenced by m_pLastAction
	if(pPrevAction)
		pPrevAction->Release(); // no longer referenced by m_pLastAction
	m_nPathLen++;
	m_pHeadState->performAction(nAction);
}

WActionPath* WActionPath::fork()
{
	WActionPath* pNewPath = new WActionPath(m_pHeadState->copy());
	pNewPath->m_nPathLen = m_nPathLen;
	pNewPath->m_pLastAction = m_pLastAction;
	if(m_pLastAction)
		m_pLastAction->AddRef();
	return pNewPath;
}

void WActionPath::path(size_t nCount, size_t* pOutBuf)
{
	while(nCount > m_nPathLen)
		pOutBuf[--nCount] = -1;
	size_t i = m_nPathLen;
	WAction* pAction = m_pLastAction;
	while(i > nCount)
	{
		i--;
		pAction = pAction->GetPrev();
	}
	while(i > 0)
	{
		pOutBuf[--i] = pAction->GetAction();
		pAction = pAction->GetPrev();
	}
}

double WActionPath::critique()
{
	return m_pHeadState->critiquePath(m_nPathLen, m_pLastAction);
}

} // namespace WClasses
