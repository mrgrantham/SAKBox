/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    Michael R. Smith,
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

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include "WApp.h"
#include "WMatrix.h"
#include "WCluster.h"
#include "WDecisionTree.h"
#include "WDistance.h"
#include "WDistribution.h"
#include "WEnsemble.h"
#include "WFile.h"
#include "WFunction.h"
#include "WGaussianProcess.h"
#include "WHillClimber.h"
#include "WHolders.h"
#include "WImage.h"
#include "WKernelTrick.h"
#include "WKNN.h"
#include "WLinear.h"
#include "WError.h"
#include "WManifold.h"
#include "WNaiveBayes.h"
#include "WNaiveInstance.h"
#include "WNeuralNet.h"
#include "WOptimizer.h"
#include "WRand.h"
#include "WSparseMatrix.h"
#include "WTime.h"
#include "WTransform.h"
#include "WDom.h"
#include "WVec.h"
#include "usage.h"
#include <cassert>
#include <time.h>
#include <iostream>
#ifdef WINDOWS
#include <direct.h>
#include <process.h>
#endif
#include <exception>
#include <string>
#include <vector>
#include <set>
#include <memory>

namespace WClasses
{

using std::cout;
using std::cerr;
using std::string;
using std::vector;
using std::set;
using std::ostringstream;

///Provides some useful functions for instantiating learning algorithms from the command line
class WLearnerLib
{
public:
	static WTransducer* InstantiateAlgorithm(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static size_t getAttrVal(const char* szString, size_t attrCount);

	static void parseAttributeList(vector<size_t>& list, WArgReader& args, size_t attrCount);

	static void loadData(WArgReader& args, std::unique_ptr<WMatrix>& hFeaturesOut, std::unique_ptr<WMatrix>& hLabelsOut, bool requireMetadata = false);

	static WAgglomerativeTransducer* InstantiateAgglomerativeTransducer(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBaselineLearner* InstantiateBaseline(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBayesianModelAveraging* InstantiateBMA(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBayesianModelCombination* InstantiateBMC(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBag* InstantiateBag(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WGradBoost* InstantiateGradBoost(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WResamplingAdaBoost* InstantiateBoost(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBucket* InstantiateBucket(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBucket* InstantiateCvdt(WArgReader& args);

	static WDecisionTree* InstantiateDecisionTree(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WGaussianProcess* InstantiateGaussianProcess(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WGraphCutTransducer* InstantiateGraphCutTransducer(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBayesianModelCombination* InstantiateHodgePodge(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WKNN* InstantiateKNN(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WLinearRegressor* InstantiateLinearRegressor(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WMeanMarginsTree* InstantiateMeanMarginsTree(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WNaiveBayes* InstantiateNaiveBayes(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WNaiveInstance* InstantiateNaiveInstance(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WBlock* instantiateBlock(WArgReader& args);

	static WNeuralNetLearner* InstantiateNeuralNet(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WNeighborTransducer* InstantiateNeighborTransducer(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels);

	static WRandomForest* InstantiateRandomForest(WArgReader& args);

	static void showInstantiateAlgorithmError(const char* szMessage, WArgReader& args);

	static void autoTuneDecisionTree(WMatrix& features, WMatrix& labels);

	static void autoTuneKNN(WMatrix& features, WMatrix& labels);

	static void autoTuneNeuralNet(WMatrix& features, WMatrix& labels);

	static void autoTuneNaiveBayes(WMatrix& features, WMatrix& labels);

	static void autoTuneNaiveInstance(WMatrix& features, WMatrix& labels);

	static void autoTuneGraphCutTransducer(WMatrix& features, WMatrix& labels);

	static void autoTune(WArgReader& args);

	static void Train(WArgReader& args);

	static void predict(WArgReader& args);

	static void predictDistribution(WArgReader& args);

	static void leftJustifiedString(const char* pIn, char* pOut, size_t outLen);

	static void rightJustifiedString(const char* pIn, char* pOut, size_t outLen);

	///\brief Returns the header for the machine readable confusion matrix
	///for variable \a variable_idx as printed by
	///printMachineReadableConfusionMatrices
	///
	///The header is comma-separated values. The first two entries in the
	///header are "Variable Name","Variable Index". The rest of the
	///entries fit the format "Expected:xxx/Got:yyy" where xxx and yyy are
	///two values that the variable can take on.
	///
	///\param variable_idx the index of the variable in the relation
	///
	///\param pRelation a pointer to the relation from which the
	///                 variable_idx-'th variable is taken. Cannot be null
	static std::string machineReadableConfusionHeader(std::size_t variable_idx, const WRelation* pRelation);

	//\brief Returns the data for the machine readable confusion matrix
	///for variable \a variable_idx as printed by
	///printMachineReadableConfusionMatrices
	///
	///The first entry is the name of the variable. The second entry is
	///the value of variable_idx, The entry (r*numCols+c)+2 where r and c are both in 0..nv-1, nv being the number of values that the variable takes on, is the entry at row r and column c of *pMatrix
	///
	///\param variable_idx the index of the variable in the relation
	///
	///\param pRelation a pointer to the relation from which the
	///                 variable_idx-'th variable is taken. Cannot be NULL.
	///
	///\param pMatrix a pointer to the confusion matrix. (*pMatrix)[r][c]
	///               is the number of times that r was expected and c was
	///               received. Cannot be NULL.
	static std::string machineReadableConfusionData(std::size_t variable_idx, const WRelation* pRelation, WMatrix const* const pMatrix);

	///\brief Prints the confusion matrices as machine-readable csv-like lines.
	///
	///The first line is a header giving the names of the columns for the
	///next line.  The first column is the name of the label variable for
	///which the matrix is being printed.  The rest of the columns are the
	///names of the expected/got values (row/column in the input matrices)
	///
	///\param pRelation the relation for which the confusion matrices are
	///                 given.  Cannot be NULL.
	///
	///\param matrixArray matrixArray[i] is null if there is no matrix to
	///                   be printed. Otherwise matrixArray[i] is the
	///                   confusion matrix for the i'th attribute of
	///                   pRelation. Row r, column c of matrixArray[i] is the
	///                   number of times the value r of the attribute was expected
	///                   and c was encountered.
	static void printMachineReadableConfusionMatrices(const WRelation* pRelation, vector<WMatrix*>& matrixArray);

	static void printConfusionMatrices(const WRelation* pRelation, vector<WMatrix*>& matrixArray);

	static void Test(WArgReader& args);

	static void Transduce(WArgReader& args);

	static void TransductiveAccuracy(WArgReader& args);

	static void SplitTest(WArgReader& args);

	static void CrossValidateCallback(void* pSupLearner, size_t nRep, size_t nFold, double foldSSE, size_t rows);

	static void CrossValidate(WArgReader& args);

	static void vette(string& s);

	static void PrecisionRecall(WArgReader& args);

	static void sterilize(WArgReader& args);

	static void regress(WArgReader& args);

	static void metaData(WArgReader& args);

	static void ShowUsage(const char* appName);

	static void showError(WArgReader& args, const char* szAppName, const char* szMessage);
};


class OptimizerTargetFunc : public WTargetFunction
{
public:
	WMatrix* m_pIn;
	WMatrix* m_pOut;
	WFunction* m_pFunc;
	WFunctionParser* m_pParser;

	OptimizerTargetFunc(WMatrix* pIn, WMatrix* pOut, WFunction* pFunc, WFunctionParser* pParser) : WTargetFunction(pFunc->m_expectedParams - pIn->cols()), m_pIn(pIn), m_pOut(pOut), m_pFunc(pFunc), m_pParser(pParser)
	{
	}

	~OptimizerTargetFunc() override
	{
	}

	bool isStable() override { return true; }
	virtual bool isConstrained() { return false; }

	void initVector(WVec& pVector) override
	{
		pVector.fill(0.1);
	}

	double computeError(const WVec& pVector) override
	{
		double sse = 0.0;
		vector<double> params;
		params.resize(m_pFunc->m_expectedParams);
		size_t inDims = m_pIn->cols();
		for(size_t j = 0; j < m_pRelation->size(); j++)
			params[inDims + j] = pVector[j];
		for(size_t i = 0; i < m_pIn->rows(); i++)
		{
			WVec& pIn = m_pIn->row(i);
			for(size_t j = 0; j < inDims; j++)
				params[j] = pIn[j];
			double pred = m_pFunc->call(params, *m_pParser);
			WVec& pOut = m_pOut->row(i);
			double d = pOut[0] - pred;
			sse += d * d;
		}
		return sse;
	}
};

} // namespace WClasses
