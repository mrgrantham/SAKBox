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
#include "WLearnerLib.h"
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

using namespace WClasses;

using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::vector;
using std::set;
using std::ostringstream;

size_t WLearnerLib::getAttrVal(const char* szString, size_t attrCount)
{
	bool fromRight = false;
	if(*szString == '*')
	{
		fromRight = true;
		szString++;
	}
	if(*szString < '0' || *szString > '9')
		throw Ex("Expected a digit while parsing attribute list");
#ifdef WINDOWS
	size_t val = (size_t)_strtoui64(szString, (char**)NULL, 10);
#else
	size_t val = strtoull(szString, (char**)nullptr, 10);
#endif
	if(fromRight)
		val = attrCount - 1 - val;
	return val;
}

void WLearnerLib::parseAttributeList(vector<size_t>& list, WArgReader& args, size_t attrCount)
{
	const char* szList = args.pop_string();
	set<size_t> attrSet;
	while(true)
	{
		// Skip whitespace
		while(*szList <= ' ' && *szList != '\0')
			szList++;

		// Find the next ',' or the end of string, and
		int i;
		int j = -1;
		for(i = 0; szList[i] != '\0' && szList[i] != ','; i++)
		{
			if(j < 0 && szList[i] == '-')
				j = i;
		}
		if(j >= 0)
		{
			while(szList[j + 1] <= ' ' && szList[j + 1] != '\0')
				j++;
		}

		// Add the attributes to the list
		if(i > 0) // If there is more...
		{
			if(j < 0) // If there is no "-" character in the next value...
			{
				size_t val = getAttrVal(szList, attrCount);
				if(val >= attrCount)
					throw Ex("Invalid column index: ", to_str(val), ". Valid values are from 0 to ", to_str(attrCount - 1), ". (Columns are zero-indexed.)");
				if(attrSet.find(val) != attrSet.end())
					throw Ex("Columns ", to_str(val), " is listed multiple times");
				attrSet.insert(val);
				list.push_back(val);
			}
			else
			{
				size_t beg = getAttrVal(szList, attrCount);
				if(beg >= attrCount)
					throw Ex("Invalid column index: ", to_str(beg), ". Valid values are from 0 to ", to_str(attrCount - 1), ". (Columns are zero-indexed.)");
				size_t end = getAttrVal(szList + j + 1, attrCount);
				if(end >= attrCount)
					throw Ex("Invalid column index: ", to_str(end), ". Valid values are from 0 to ", to_str(attrCount - 1), ". (Columns are zero-indexed.)");
				int step = 1;
				if(end < beg)
					step = -1;
				for(size_t val = beg; true; val += step)
				{
					if(attrSet.find(val) != attrSet.end())
						throw Ex("Column ", to_str(val), " is listed multiple times");
					attrSet.insert(val);
					list.push_back(val);
					if(val == end)
						break;
				}
			}
		}

		// Advance
		szList += i;
		if(*szList == '\0')
			break;
		szList++;
	}
}

void WLearnerLib::loadData(WArgReader& args, std::unique_ptr<WMatrix>& hFeaturesOut, std::unique_ptr<WMatrix>& hLabelsOut, bool requireMetadata)
{
	// Load the dataset by extension
	if(args.size() < 1)
		throw Ex("Expected the filename of a datset. (Found end of arguments.)");
	const char* szFilename = args.pop_string();
	PathData pd;
	WFile::parsePath(szFilename, &pd);
	WMatrix data;
	vector<size_t> abortedCols;
	vector<size_t> ambiguousCols;
	const char* input_type;
	if(args.next_is_flag() && args.if_pop("-input_type"))
	{
		input_type = args.pop_string();
	}
	else
	{ /* deduce it from extension (if any) */
		input_type = szFilename + pd.extStart;
		if(*input_type != '.') /* no extension - assume ARFF */
			input_type = "arff";
		else
			input_type++;
	}
	// Now load the data
	if(_stricmp(input_type, "arff") == 0)
	{
		data.loadArff(szFilename);
	}
	else if(_stricmp(input_type, "csv") == 0)
	{
		WCSVParser parser;
		parser.parse(data, szFilename);
		cerr << "\nParsing Report:\n";
		for(size_t i = 0; i < data.cols(); i++)
			cerr << to_str(i) << ") " << parser.report(i) << "\n";
		if(requireMetadata && !data.relation().areContinuous())
			throw Ex("A data format containing meta-data (such as ARFF) is necessary for this operation.");
	}
	else if(_stricmp(input_type, "dat") == 0)
	{
		WCSVParser parser;
		parser.setSeparator('\0');
		parser.parse(data, szFilename);
		cerr << "\nParsing Report:\n";
		for(size_t i = 0; i < data.cols(); i++)
			cerr << to_str(i) << ") " << parser.report(i) << "\n";
		if(requireMetadata && !data.relation().areContinuous())
			throw Ex("A data format containing meta-data (such as ARFF) is necessary for this operation.");
	}
	else
		throw Ex("Unsupported file format: ", szFilename + pd.extStart);


	// Parse params
	vector<size_t> ignore;
	vector<size_t> labels;
	while(args.next_is_flag())
	{
		if(args.if_pop("-labels"))
			parseAttributeList(labels, args, data.cols());
		else if(args.if_pop("-ignore"))
			parseAttributeList(ignore, args, data.cols());
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Throw out the ignored attributes
	std::sort(ignore.begin(), ignore.end());
	for(size_t i = ignore.size() - 1; i < ignore.size(); i--)
	{
		data.deleteColumns(ignore[i], 1);
		for(size_t j = 0; j < labels.size(); j++)
		{
			if(labels[j] >= ignore[i])
			{
				if(labels[j] == ignore[i])
					throw Ex("Attribute ", to_str(labels[j]), " is both ignored and used as a label");
				labels[j]--;
			}
		}
	}

	// Swap label columns to the end
	size_t labelDims = std::max((size_t)1, labels.size());
	for(size_t i = 0; i < labels.size(); i++)
	{
		size_t src = labels[i];
		size_t dst = data.cols() - labelDims + i;
		if(src != dst)
		{
			data.swapColumns(src, dst);
			for(size_t j = i + 1; j < labels.size(); j++)
			{
				if(labels[j] == dst)
				{
					labels[j] = src;
					break;
				}
			}
		}
	}

	// Split data into a feature matrix and a label matrix
	WMatrix* pFeatures = new WMatrix(data, 0, 0, data.rows(), data.cols() - labelDims);
	hFeaturesOut.reset(pFeatures);
	WMatrix* pLabels = new WMatrix(data, 0, data.cols() - labelDims, data.rows(), labelDims);
	hLabelsOut.reset(pLabels);
}

WAgglomerativeTransducer* WLearnerLib::InstantiateAgglomerativeTransducer(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WAgglomerativeTransducer* pTransducer = new WAgglomerativeTransducer();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pTransducer->autoTune(*pFeatures, *pLabels);
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pTransducer;
}

WBag* WLearnerLib::InstantiateBag(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WBag* pEnsemble = new WBag();
	while(args.size() > 0)
	{
		if(args.if_pop("end"))
			break;
		int instance_count = args.pop_uint();
		int arg_pos = args.get_pos();
		for(int i = 0; i < instance_count; i++)
		{
			args.set_pos(arg_pos);
			WTransducer* pLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
			if(!pLearner->canGeneralize())
			{
				delete(pLearner);
				throw Ex("bag does not support algorithms that cannot generalize.");
			}
			pEnsemble->addLearner((WSupervisedLearner*)pLearner);
		}
	}
	return pEnsemble;
}

WGradBoost* WLearnerLib::InstantiateGradBoost(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	double trainingSizeRatio = 1;
	size_t ensembleSize = 30;

	while(args.size() > 0)
	{
		if(args.if_pop("-trainratio"))
		{
			trainingSizeRatio = args.pop_double();
		}
		else if(args.if_pop("-size"))
		{
			ensembleSize = args.pop_uint();
		}
		else
		{
			break;
		}
	}

	WTransducer* pLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
	if(!pLearner->canGeneralize())
	{
		delete(pLearner);
		throw Ex("gradboost does not support algorithms that cannot generalize.");
	}

	WGradBoost* pEnsemble = new WGradBoost((WSupervisedLearner*)pLearner, true, new WLearnerLoader());
	pEnsemble->setTrainSize(trainingSizeRatio);
	pEnsemble->setSize(ensembleSize);

	return pEnsemble;
}

WBaselineLearner* WLearnerLib::InstantiateBaseline(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WBaselineLearner* pModel = new WBaselineLearner();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pModel;
}

WBayesianModelAveraging* WLearnerLib::InstantiateBMA(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WBayesianModelAveraging* pEnsemble = new WBayesianModelAveraging();
	while(args.size() > 0)
	{
		if(args.if_pop("end"))
			break;
		int instance_count = args.pop_uint();
		int arg_pos = args.get_pos();
		for(int i = 0; i < instance_count; i++)
		{
			args.set_pos(arg_pos);
			WTransducer* pLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
			if(!pLearner->canGeneralize())
			{
				delete(pLearner);
				throw Ex("BMA does not support algorithms that cannot generalize.");
			}
			pEnsemble->addLearner((WSupervisedLearner*)pLearner);
		}
	}
	return pEnsemble;
}

WBayesianModelCombination* WLearnerLib::InstantiateBMC(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WBayesianModelCombination* pEnsemble = new WBayesianModelCombination();
	size_t samples = 100;
	while(args.next_is_flag())
	{
		if(args.if_pop("-samples"))
			samples = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}
	pEnsemble->setSamples(samples);
	while(args.size() > 0)
	{
		if(args.if_pop("end"))
			break;
		int instance_count = args.pop_uint();
		int arg_pos = args.get_pos();
		for(int i = 0; i < instance_count; i++)
		{
			args.set_pos(arg_pos);
			WTransducer* pLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
			if(!pLearner->canGeneralize())
			{
				delete(pLearner);
				throw Ex("BMC does not support algorithms that cannot generalize.");
			}
			pEnsemble->addLearner((WSupervisedLearner*)pLearner);
		}
	}
	return pEnsemble;
}

WResamplingAdaBoost* WLearnerLib::InstantiateBoost(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	double trainingSizeRatio = 1;
	size_t ensembleSize = 30;

	while(args.size() > 0)
	{
		if(args.if_pop("-trainratio"))
		{
			trainingSizeRatio = args.pop_double();
		}
		else if(args.if_pop("-size"))
		{
			ensembleSize = args.pop_uint();
		}
		else
		{
			break;
		}
	}

	WTransducer* pLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
	if(!pLearner->canGeneralize())
	{
		delete(pLearner);
		throw Ex("boost does not support algorithms that cannot generalize.");
	}

	WResamplingAdaBoost* pEnsemble = new WResamplingAdaBoost((WSupervisedLearner*)pLearner, true, new WLearnerLoader());
	pEnsemble->setTrainSize(trainingSizeRatio);
	pEnsemble->setSize(ensembleSize);

	return pEnsemble;
}

WBucket* WLearnerLib::InstantiateBucket(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WBucket* pEnsemble = new WBucket();
	while(args.size() > 0)
	{
		if(args.if_pop("end"))
			break;
		WTransducer* pLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
		if(!pLearner->canGeneralize())
		{
			delete(pLearner);
			throw Ex("crossvalidationselector does not support algorithms that cannot generalize.");
		}
		pEnsemble->addLearner((WSupervisedLearner*)pLearner);
	}
	return pEnsemble;
}

WBucket* WLearnerLib::InstantiateCvdt(WArgReader& args)
{
	size_t trees = args.pop_uint();
	WBucket* pBucket = new WBucket();
	WBag* pBag1 = new WBag();
	pBucket->addLearner(pBag1);
	for(size_t i = 0; i < trees; i++)
		pBag1->addLearner(new WDecisionTree());
	WBag* pBag2 = new WBag();
	pBucket->addLearner(pBag2);
	for(size_t i = 0; i < trees; i++)
		pBag2->addLearner(new WMeanMarginsTree());
	return pBucket;
}

WDecisionTree* WLearnerLib::InstantiateDecisionTree(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WDecisionTree* pModel = new WDecisionTree();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else if(args.if_pop("-binary"))
		{
			pModel->useBinaryDivisions();
		}
		else if(args.if_pop("-random"))
		{
			pModel->useRandomDivisions(args.pop_uint());
		}
		else if(args.if_pop("-leafthresh"))
		{
			pModel->setLeafThresh(args.pop_uint());
		}
		else if(args.if_pop("-maxlevels"))
		{
			pModel->setMaxLevels(args.pop_uint());
		}
		else
		{
			throw Ex("Invalid option: ", args.peek());
		}
	}
	return pModel;
}

WGaussianProcess* WLearnerLib::InstantiateGaussianProcess(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WGaussianProcess* pModel = new WGaussianProcess();
	while(args.next_is_flag())
	{
		if(args.if_pop("-noise"))
		{
			pModel->setNoiseVariance(args.pop_double());
		}
		else if(args.if_pop("-prior"))
		{
			pModel->setWeightsPriorVariance(args.pop_double());
		}
		else if(args.if_pop("-maxsamples"))
		{
			pModel->setMaxSamples(args.pop_uint());
		}
		else if(args.if_pop("-kernel"))
		{
			if(args.if_pop("identity"))
				pModel->setKernel(new WKernelIdentity());
			else if(args.if_pop("chisquared"))
				pModel->setKernel(new WKernelChiSquared());
			else if(args.if_pop("rbf"))
				pModel->setKernel(new WKernelGaussianRBF(args.pop_double()));
			else if(args.if_pop("polynomial"))
				pModel->setKernel(new WKernelPolynomial(args.pop_double(), args.pop_uint()));
			else throw Ex("Unrecognized kernel: ", args.pop_string());
		}
		else
		{
			throw Ex("Invalid option: ", args.peek());
		}
	}
	return pModel;
}

WGraphCutTransducer* WLearnerLib::InstantiateGraphCutTransducer(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WGraphCutTransducer* pTransducer = new WGraphCutTransducer();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pTransducer->autoTune(*pFeatures, *pLabels);
		}
		else if(args.if_pop("-neighbors"))
			pTransducer->setNeighbors(args.pop_uint());
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pTransducer;
}

WBayesianModelCombination* WLearnerLib::InstantiateHodgePodge(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WBayesianModelCombination* pEnsemble = new WBayesianModelCombination();

	WNaiveBayes* pNB = new WNaiveBayes();
	pEnsemble->addLearner(pNB);

	WNaiveBayes* pNB2 = new WNaiveBayes();
	pNB2->setEquivalentSampleSize(1.0);
	pEnsemble->addLearner(pNB2);

	WLinearRegressor* pLin = new WLinearRegressor();
	pEnsemble->addLearner(pLin);

	WNaiveInstance* pNI = new WNaiveInstance();
	pEnsemble->addLearner(pNI);

	WNeuralNetLearner* pNN = new WNeuralNetLearner();
	pEnsemble->addLearner(pNN);

	WBaselineLearner* pBL = new WBaselineLearner();
	pEnsemble->addLearner(pBL);

	for(size_t i = 0; i < 6; i++)
	{
		WDecisionTree* pDT = new WDecisionTree();
		pDT->setLeafThresh(6 * i);
		pEnsemble->addLearner(pDT);
	}

	for(size_t i = 0; i < 12; i++)
	{
		WDecisionTree* pRDT = new WDecisionTree();
		pRDT->useRandomDivisions(1);
		pEnsemble->addLearner(pRDT);
	}

	for(size_t i = 0; i < 8; i++)
	{
		WMeanMarginsTree* pMM = new WMeanMarginsTree();
		pEnsemble->addLearner(pMM);
	}

	for(size_t i = 0; i < 5; i++)
	{
		WKNN* pKnn = new WKNN();
		pKnn->setNeighborCount(1);
		pKnn->drawRandom(16);
		pEnsemble->addLearner(pKnn);
	}

	for(size_t i = 0; i < 3; i++)
	{
		WKNN* pKnn = new WKNN();
		pKnn->setNeighborCount(3);
		pKnn->drawRandom(24);
		pEnsemble->addLearner(pKnn);
	}

	return pEnsemble;
}

WKNN* WLearnerLib::InstantiateKNN(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WKNN* pModel = new WKNN();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else if(args.if_pop("-nonormalize"))
			pModel->setNormalizeScaleFactors(false);
		else if(args.if_pop("-neighbors"))
			pModel->setNeighborCount(args.pop_uint());
		else if(args.if_pop("-equalweight"))
			pModel->setInterpolationMethod(WKNN::Mean);
		else if(args.if_pop("-scalefeatures"))
			pModel->setOptimizeScaleFactors(true);
		else if(args.if_pop("-cosine"))
			pModel->setMetric(new WCosineSimilarity(), true);
		else if(args.if_pop("-pearson"))
			pModel->setMetric(new WPearsonCorrelation(), true);
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pModel;
}

WLinearRegressor* WLearnerLib::InstantiateLinearRegressor(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WLinearRegressor* pModel = new WLinearRegressor();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pModel;
}

WMeanMarginsTree* WLearnerLib::InstantiateMeanMarginsTree(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WMeanMarginsTree* pModel = new WMeanMarginsTree();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pModel;
}

WNaiveBayes* WLearnerLib::InstantiateNaiveBayes(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WNaiveBayes* pModel = new WNaiveBayes();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else if(args.if_pop("-ess"))
			pModel->setEquivalentSampleSize(args.pop_double());
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pModel;
}

WNaiveInstance* WLearnerLib::InstantiateNaiveInstance(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WNaiveInstance* pModel = new WNaiveInstance();
	while(args.next_is_flag())
	{
		if(args.if_pop("-"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pModel->autoTune(*pFeatures, *pLabels);
		}
		else if(args.if_pop("-neighbors"))
			pModel->setNeighbors(args.pop_uint());
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pModel;
}

WNeighborTransducer* WLearnerLib::InstantiateNeighborTransducer(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WNeighborTransducer* pTransducer = new WNeighborTransducer();
	while(args.next_is_flag())
	{
		if(args.if_pop("-autotune"))
		{
			if(!pFeatures || !pLabels)
				throw Ex("Insufficient data to support automatic tuning");
			pTransducer->autoTune(*pFeatures, *pLabels);
		}
		else if(args.if_pop("-neighbors"))
			pTransducer->setNeighbors(args.pop_uint());
		else
			throw Ex("Invalid option: ", args.peek());
	}
	return pTransducer;
}

WBlock* WLearnerLib::instantiateBlock(WArgReader& args)
{
	const char* szBlockName = args.pop_string();
	if(strcmp(szBlockName, "linear") == 0)
		return new WBlockLinear(args.pop_uint(), args.pop_uint());
	/*	else if(strcmp(szBlockName, "bentidentity") == 0)
		return new WBlockBentIdentity();
	else if(strcmp(szBlockName, "gaussian") == 0)
		return new WBlockGaussian();
	else if(strcmp(szBlockName, "identity") == 0)
		return new WBlockIdentity();
	else if(strcmp(szBlockName, "logistic") == 0)
		return new WBlockLogistic();
	else if(strcmp(szBlockName, "rectifier") == 0)
		return new WBlockRectifier();
	else if(strcmp(szBlockName, "leakyrectifier") == 0)
		return new WBlockLeakyRectifier();
	else if(strcmp(szBlockName, "sigexp") == 0)
		return new WBlockSigExp();
	else if(strcmp(szBlockName, "sine") == 0)
		return new WBlockSine();
	else if(strcmp(szBlockName, "softplus") == 0)
		return new WBlockSoftPlus();
	else if(strcmp(szBlockName, "softroot") == 0)
		return new WBlockSoftRoot();*/
	else if(strcmp(szBlockName, "tanh") == 0)
		return new WBlockTanh(args.pop_uint());
	throw Ex("Unrecognized block type: ", szBlockName);
}

WNeuralNetLearner* WLearnerLib::InstantiateNeuralNet(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	WNeuralNetLearner* pModel = new WNeuralNetLearner();
	while(args.next_is_flag())
	{
		if(args.if_pop("-add"))
			pModel->nn().add(instantiateBlock(args));
		else if(args.if_pop("-concat"))
		{
			size_t inPos = args.pop_uint();
			pModel->nn().concat(instantiateBlock(args), inPos);
		}
		/*		else if(args.if_pop("-learningrate"))
			pModel->setLearningRate(args.pop_double());
		else if(args.if_pop("-momentum"))
			pModel->setMomentum(args.pop_double());
		else if(args.if_pop("-windowepochs"))
			pModel->setWindowSize(args.pop_uint());
		else if(args.if_pop("-minwindowimprovement"))
			pModel->setImprovementThresh(args.pop_double());
		else if(args.if_pop("-holdout"))
			pModel->setValidationPortion(args.pop_double());
*/
		else
			throw Ex("Invalid option: ", args.peek());
	}
	if(pModel->nn().layerCount() == 0)
		throw Ex("At least one layer is required");
	return pModel;
}

WRandomForest* WLearnerLib::InstantiateRandomForest(WArgReader& args)
{
	size_t trees = args.pop_uint();
	size_t samples = 1;
	while(args.next_is_flag())
	{
		if(args.if_pop("-samples"))
			samples = args.pop_uint();
		else
			throw Ex("Invalid random forest option: ", args.peek());
	}
	return new WRandomForest(trees, samples);
}

void WLearnerLib::showInstantiateAlgorithmError(const char* szMessage, WArgReader& args)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	const char* szAlgName = args.peek();
	UsageNode* pAlgTree = makeAlgorithmUsageTree();
	std::unique_ptr<UsageNode> hAlgTree(pAlgTree);
	if(szAlgName)
	{
		UsageNode* pUsageAlg = pAlgTree->choice(szAlgName);
		if(pUsageAlg)
		{
			cerr << "Partial Usage Information:\n\n";
			pUsageAlg->print(cerr, 0, 3, 76, 1000, true);
		}
		else
		{
			cerr << "\"" << szAlgName << "\" is not a recognized algorithm. Try one of these:\n\n";
			pAlgTree->print(cerr, 0, 3, 76, 1, false);
		}
	}
	else
	{
		cerr << "Expected an algorithm. Here are some choices:\n";
		pAlgTree->print(cerr, 0, 3, 76, 1, false);
	}
	cerr << "\nTo see full usage information, run:\n	waffles_learn usage\n\n";
	cerr << "For a graphical tool that will help you to build a command, run:\n	waffles_wizard\n";
	cerr.flush();
}

WTransducer* WLearnerLib::InstantiateAlgorithm(WArgReader& args, WMatrix* pFeatures, WMatrix* pLabels)
{
	int argPos = args.get_pos();
	if(args.size() < 1)
		throw Ex("No algorithm specified.");
	WTransducer* pAlg = nullptr;
	try
	{
		if(args.if_pop("agglomerativetransducer"))
			pAlg = InstantiateAgglomerativeTransducer(args, pFeatures, pLabels);
		else if(args.if_pop("bag"))
			pAlg = InstantiateBag(args, pFeatures, pLabels);
		else if(args.if_pop("baseline"))
			pAlg = InstantiateBaseline(args, pFeatures, pLabels);
		else if(args.if_pop("bma"))
			pAlg = InstantiateBMA(args, pFeatures, pLabels);
		else if(args.if_pop("bmc"))
			pAlg = InstantiateBMC(args, pFeatures, pLabels);
		else if(args.if_pop("boost"))
			pAlg = InstantiateBoost(args, pFeatures, pLabels);
		else if(args.if_pop("bucket"))
			pAlg = InstantiateBucket(args, pFeatures, pLabels);
		else if(args.if_pop("cvdt"))
			pAlg = InstantiateCvdt(args);
		else if(args.if_pop("decisiontree"))
			pAlg = InstantiateDecisionTree(args, pFeatures, pLabels);
		else if(args.if_pop("gaussianprocess"))
			pAlg = InstantiateGaussianProcess(args, pFeatures, pLabels);
		else if(args.if_pop("gradboost"))
			pAlg = InstantiateGradBoost(args, pFeatures, pLabels);
		else if(args.if_pop("graphcuttransducer"))
			pAlg = InstantiateGraphCutTransducer(args, pFeatures, pLabels);
		else if(args.if_pop("hodgepodge"))
			pAlg = InstantiateHodgePodge(args, pFeatures, pLabels);
		else if(args.if_pop("knn"))
			pAlg = InstantiateKNN(args, pFeatures, pLabels);
		else if(args.if_pop("linear"))
			pAlg = InstantiateLinearRegressor(args, pFeatures, pLabels);
		else if(args.if_pop("meanmarginstree"))
			pAlg = InstantiateMeanMarginsTree(args, pFeatures, pLabels);
		else if(args.if_pop("naivebayes"))
			pAlg = InstantiateNaiveBayes(args, pFeatures, pLabels);
		else if(args.if_pop("naiveinstance"))
			pAlg = InstantiateNaiveInstance(args, pFeatures, pLabels);
		else if(args.if_pop("neighbortransducer"))
			pAlg = InstantiateNeighborTransducer(args, pFeatures, pLabels);
		else if(args.if_pop("neuralnet"))
			pAlg = InstantiateNeuralNet(args, pFeatures, pLabels);
		else if(args.if_pop("randomforest"))
			pAlg = InstantiateRandomForest(args);
		else
			throw Ex("Unrecognized algorithm name: ", args.peek());
	}
	catch(const std::exception& e)
	{
		args.set_pos(argPos);
		if(strcmp(e.what(), "nevermind") != 0) // if an error message was not already displayed...
			showInstantiateAlgorithmError(e.what(), args);
		throw Ex("nevermind"); // this means "don't display another error message"
	}
	if(pAlg->canGeneralize())
	{
		WSupervisedLearner* pLearner = (WSupervisedLearner*)pAlg;
		pAlg = new WAutoFilter(pLearner);
	}
	return pAlg;
}

void WLearnerLib::autoTuneDecisionTree(WMatrix& features, WMatrix& labels)
{
	WDecisionTree dt;
	dt.autoTune(features, labels);
	cout << "decisiontree";
	if(dt.leafThresh() != 1)
		cout << " -leafthresh " << dt.leafThresh();
	if(dt.isBinary())
		cout << " -binary";
	cout << "\n";
}

void WLearnerLib::autoTuneKNN(WMatrix& features, WMatrix& labels)
{
	WKNN model;
	model.autoTune(features, labels);
	cout << "knn";
	if(model.neighborCount() != 1)
		cout << " -neighbors " << model.neighborCount();
	cout << "\n";
}

void WLearnerLib::autoTuneNeuralNet(WMatrix& features, WMatrix& labels)
{
	throw Ex("Cannot autotune neural net at this time. Recent changes to the way optimization works have broken this functionality.");
}

void WLearnerLib::autoTuneNaiveBayes(WMatrix& features, WMatrix& labels)
{
	WNaiveBayes model;
	model.autoTune(features, labels);
	cout << "naivebayes";
	cout << " -ess " << model.equivalentSampleSize();
	cout << "\n";
}

void WLearnerLib::autoTuneNaiveInstance(WMatrix& features, WMatrix& labels)
{
	WNaiveInstance model;
	model.autoTune(features, labels);
	cout << "naiveinstance";
	cout << " -neighbors " << model.neighbors();
	cout << "\n";
}

void WLearnerLib::autoTuneGraphCutTransducer(WMatrix& features, WMatrix& labels)
{
	WGraphCutTransducer transducer;
	transducer.autoTune(features, labels);
	cout << "graphcuttransducer";
	cout << " -neighbors " << transducer.neighbors();
	cout << "\n";
}

void WLearnerLib::autoTune(WArgReader& args)
{
	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Load the model name
	const char* szModel = args.pop_string();
	if(strcmp(szModel, "agglomerativetransducer") == 0)
		cout << "agglomerativetransducer\n"; // no params to tune
	else if(strcmp(szModel, "decisiontree") == 0)
		autoTuneDecisionTree(*pFeatures, *pLabels);
	else if(strcmp(szModel, "graphcuttransducer") == 0)
		autoTuneGraphCutTransducer(*pFeatures, *pLabels);
	else if(strcmp(szModel, "knn") == 0)
		autoTuneKNN(*pFeatures, *pLabels);
	else if(strcmp(szModel, "meanmarginstree") == 0)
		cout << "meanmarginstree\n"; // no params to tune
	else if(strcmp(szModel, "neuralnet") == 0)
		autoTuneNeuralNet(*pFeatures, *pLabels);
	else if(strcmp(szModel, "naivebayes") == 0)
		autoTuneNaiveBayes(*pFeatures, *pLabels);
	else if(strcmp(szModel, "naiveinstance") == 0)
		autoTuneNaiveInstance(*pFeatures, *pLabels);
	else
		throw Ex("Sorry, autotune does not currently support a model named ", szModel, ".");
}

void WLearnerLib::Train(WArgReader& args)
{
	// Parse options
	size_t seed = getpid() * (unsigned int)time(nullptr);
	bool embed = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-embed"))
			embed = true;
		else
			throw Ex("Invalid train option: ", args.peek());
	}

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
	pSupLearner->rand().setSeed(seed);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	if(!pSupLearner->canGeneralize())
		throw Ex("This algorithm cannot be \"trained\". It can only be used to \"transduce\".");
	WSupervisedLearner* pModel = (WSupervisedLearner*)pSupLearner;

	// Train the modeler
	pModel->train(*pFeatures, *pLabels);

	// Output the trained model
	WDom doc;
	WDomNode* pRoot = pModel->serialize(&doc);
	doc.setRoot(pRoot);
	if(embed)
		doc.writeJsonCpp(cout);
	else
		doc.writeJson(cout);
}

void WLearnerLib::predict(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid predict option: ", args.peek());
	}

	// Load the model
	WDom doc;
	if(args.size() < 1)
		throw Ex("Model not specified.");
	doc.loadJson(args.pop_string());
	WLearnerLoader ll(true);
	WSupervisedLearner* pModeler = ll.loadLearner(doc.root());
	std::unique_ptr<WSupervisedLearner> hModeler(pModeler);
	pModeler->rand().setSeed(seed);

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels, true);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();
	if(pLabels->cols() != pModeler->relLabels().size())
		throw Ex("The model was trained with ", to_str(pModeler->relLabels().size()), " label dims, but the specified dataset has ", to_str(pLabels->cols()));
	if(!pFeatures->relation().isCompatible(pModeler->relFeatures()) || !pLabels->relation().isCompatible(pModeler->relLabels()))
		throw Ex("This data is not compatible with the data that was used to train the model. (The column meta-data is different.)");
	pLabels->fill(0.0); // Wipe out the existing labels, just to be absolutely certain that we don't somehow accidentally let them influence the predictions

	// Test
	for(size_t i = 0; i < pFeatures->rows(); i++)
	{
		WVec& featureVec = pFeatures->row(i);
		WVec& labelVec = pLabels->row(i);
		pModeler->predict(featureVec, labelVec);
	}

	// Print results
	pLabels->print(cout);
}

void WLearnerLib::predictDistribution(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid predict option: ", args.peek());
	}

	// Load the model
	WDom doc;
	if(args.size() < 1)
		throw Ex("Model not specified.");
	doc.loadJson(args.pop_string());
	WLearnerLoader ll(true);
	WSupervisedLearner* pModeler = ll.loadLearner(doc.root());
	std::unique_ptr<WSupervisedLearner> hModeler(pModeler);
	pModeler->rand().setSeed(seed);

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels, true);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();
	if(pLabels->cols() != pModeler->relLabels().size())
		throw Ex("The model was trained with ", to_str(pModeler->relLabels().size()), " label dims, but the specified dataset has ", to_str(pLabels->cols()));
	if(!pFeatures->relation().isCompatible(pModeler->relFeatures()) || !pLabels->relation().isCompatible(pModeler->relLabels()))
		throw Ex("This data is not compatible with the data that was used to train the model. (The column meta-data is different.)");

	// Predict

	// If the model doesn't support probability distribution and  we'd like to
	// return result we should wrap it with WLabelFilter like it's done
	// with the WLinearRegressor

	// Set ARFF header

	// TODO: Not nice! Better have all WRelation type implement attrName().
	WArffRelation* pModelRelLabels = (WArffRelation*)&pModeler->relLabels();
	const size_t modelRelLabelsSize = pModelRelLabels->size();

	cout << "@RELATION" << " " << pModelRelLabels->name() << endl;
	cout << endl;
	for(size_t i = 0; i < modelRelLabelsSize; ++i)
	{
		if(pModelRelLabels->valueCount(i) > 0) // categorical column
		{
			for(size_t j = 0; j < pModelRelLabels->valueCount(i); ++j)
			{
				cout << "@ATTRIBUTE" << " ";
				pModelRelLabels->printAttrValue(cout, i, (double)j);
				cout << " " << "numeric" << endl;
			}
		}
		else // value column
		{
			cout << "@ATTRIBUTE" << " " << pModelRelLabels->attrName(i) << "-mean" << " " << "numeric" << endl;
			cout << "@ATTRIBUTE" << " " << pModelRelLabels->attrName(i) << "-variance" << " " << "numeric" << endl;
		}
	}

	cout << "\n"
	     << "@data" << "\n"
	     << endl;

	WPrediction* p = new WPrediction[modelRelLabelsSize];
	std::unique_ptr<WPrediction[]> hp(p);
	for(size_t i = 0; i < pFeatures->rows(); ++i)
	{
		pModeler->predictDistribution(pFeatures->row(i), p);
		for(size_t j = 0; j < modelRelLabelsSize; ++j)
		{
			if(j > 0)
				cout << ",";

			if(p[j].isContinuous())
			{
				WNormalDistribution* pNorm = p[j].asNormal();
				cout << pNorm->mean() << "," << pNorm->variance();
			}
			else
			{
				WCategoricalDistribution* pCat = p[j].asCategorical();
				WVec& values = pCat->values(pCat->valueCount());
				for(size_t k = 0; k < pCat->valueCount(); ++k)
				{
					if(k > 0)
						cout << ",";
					cout << values[k];
				}
			}
		}
		cout << endl;
	}
}

void WLearnerLib::leftJustifiedString(const char* pIn, char* pOut, size_t outLen)
{
	for([[maybe_unused]] size_t i = 0; outLen > 0 && *pIn != '\0'; i++)
	{
		*(pOut++) = *(pIn++);
		outLen--;
	}
	while(outLen > 0)
	{
		*(pOut++) = ' ';
		outLen--;
	}
	*pOut = '\0';
}

void WLearnerLib::rightJustifiedString(const char* pIn, char* pOut, size_t outLen)
{
	size_t inLen = strlen(pIn);
	size_t spaces = std::max(outLen, inLen) - inLen;
	memset(pOut, ' ', spaces);
	memcpy(pOut + spaces, pIn, outLen - spaces);
	pOut[outLen] = '\0';
}

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
std::string WLearnerLib::machineReadableConfusionHeader(std::size_t variable_idx, const WRelation* pRelation)
{
	assert(pRelation != nullptr);
	ostringstream out;
	out << "\"Variable Name\",\"Variable Index\"";
	std::size_t n = pRelation->valueCount(variable_idx);
	for(std::size_t r = 0; r < n; ++r)
	{
		std::ostringstream expected_name;
		pRelation->printAttrValue(expected_name, variable_idx, (double)r);
		std::string e = expected_name.str();
		for(std::size_t c = 0; c < n; ++c)
		{
			std::ostringstream got_name;
			pRelation->printAttrValue(got_name, variable_idx, (double)c);
			std::string g = got_name.str();
			out << ",\"Expected:" << e << " Got:" << g << "\"";
		}
	}
	return out.str();
}

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
std::string WLearnerLib::machineReadableConfusionData(std::size_t variable_idx, const WRelation* pRelation, WMatrix const* const pMatrix)
{
	ostringstream out;
	{
		ostringstream v_name;
		pRelation->printAttrName(v_name, variable_idx);
		out << v_name.str() << "," << variable_idx;
	}
	const WMatrix& m = *pMatrix;
	std::size_t n = pRelation->valueCount(variable_idx);
	for(std::size_t r = 0; r < n; ++r)
	{
		for(std::size_t c = 0; c < n; ++c)
		{
			out << "," << m[r][c];
		}
	}
	return out.str();
}

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
void WLearnerLib::printMachineReadableConfusionMatrices(const WRelation* pRelation, vector<WMatrix*>& matrixArray)
{
	for(size_t i = 0; i < matrixArray.size(); i++)
	{
		if(matrixArray[i] == nullptr)
		{
			continue;
		}
		std::cout << machineReadableConfusionHeader(i, pRelation) << std::endl;
		std::cout << machineReadableConfusionData(i, pRelation, matrixArray[i])
		          << std::endl;
	}
}

void WLearnerLib::printConfusionMatrices(const WRelation* pRelation, vector<WMatrix*>& matrixArray)
{
	cout << "\n(Rows=expected values, Cols=predicted values, Elements=number of occurrences)\n\n";
	char buf[41];
	ostringstream oss;
	for(size_t i = 0; i < matrixArray.size(); i++)
	{
		if(matrixArray[i] == nullptr)
			continue;
		WMatrix& cm = *matrixArray[i];

		// Print attribute name
		oss.precision(9);
		oss.str("");
		oss << "Confusion matrix for ";
		pRelation->printAttrName(oss, i);
		string s = oss.str();
		leftJustifiedString(s.c_str(), buf, 40);
		cout << buf;

		// Print column numbers
		for(size_t j = 0; j < cm.cols(); j++)
		{
			oss.str("");
			pRelation->printAttrValue(oss, i, (double)j);
			s = oss.str();
			rightJustifiedString(s.c_str(), buf, 12);
			cout << buf;
		}
		cout << "\n";

		// Print the confusion matrix values
		for(size_t k = 0; k < cm.rows(); k++)
		{
			oss.str("");
			pRelation->printAttrValue(oss, i, (double)k);
			s = oss.str();
			rightJustifiedString(s.c_str(), buf, 40);
			cout << buf;
			for(size_t j = 0; j < cm.cols(); j++)
			{
				oss.str("");
				oss << cm[k][j];
				s = oss.str();
				rightJustifiedString(s.c_str(), buf, 12);
				cout << buf;
			}
			cout << "\n";
		}
		cout << "\n";
	}
}

void WLearnerLib::Test(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	bool confusion = false;
	bool confusioncsv = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-confusion"))
			confusion = true;
		else if(args.if_pop("-confusioncsv"))
			confusioncsv = true;
		else
			throw Ex("Invalid test option: ", args.peek());
	}

	// Load the model
	WDom doc;
	if(args.size() < 1)
		throw Ex("Model not specified.");
	doc.loadJson(args.pop_string());
	WLearnerLoader ll(true);
	WSupervisedLearner* pModeler = ll.loadLearner(doc.root());
	std::unique_ptr<WSupervisedLearner> hModeler(pModeler);
	pModeler->rand().setSeed(seed);

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels, true);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();
	if(!pLabels->relation().isCompatible(pModeler->relLabels()))
		throw Ex("This dataset is not compatible with the one used to train the model. (The meta-data is different.)");

	// Test
	double mse = pModeler->sumSquaredError(*pFeatures, *pLabels) / pFeatures->rows();
	if(pLabels->cols() == 1 && pLabels->relation().valueCount(0) > 0)
		cout << "Misclassification rate: ";
	else
		cout << "Mean squared error: ";
	cout << to_str(mse) << "\n";

	if(confusion || confusioncsv)
	{
		vector<WMatrix*> confusionMatrices;
		pModeler->confusion(*pFeatures, *pLabels, confusionMatrices);

		// Print the confusion matrix
		if(confusion)
		{
			printConfusionMatrices(&pLabels->relation(), confusionMatrices);
		}

		if(confusioncsv)
		{
			printMachineReadableConfusionMatrices(&pLabels->relation(), confusionMatrices);
		}
	}
}

void WLearnerLib::Transduce(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid transduce option: ", args.peek());
	}

	// Load the data sets
	if(args.size() < 1)
		throw Ex("No labeled set specified.");

	// Load the labeled and unlabeled sets
	std::unique_ptr<WMatrix> hFeatures1, hLabels1, hFeatures2, hLabels2;
	loadData(args, hFeatures1, hLabels1, true);
	loadData(args, hFeatures2, hLabels2, true);
	WMatrix* pFeatures1 = hFeatures1.get();
	WMatrix* pLabels1 = hLabels1.get();
	WMatrix* pFeatures2 = hFeatures2.get();
	WMatrix* pLabels2 = hLabels2.get();
	if(pFeatures1->cols() != pFeatures2->cols() || pLabels1->cols() != pLabels2->cols())
		throw Ex("The labeled and unlabeled datasets must have the same number of columns. (The labels in the unlabeled set are just place-holders, and will be overwritten.)");

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args, pFeatures1, pLabels1);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	pSupLearner->rand().setSeed(seed);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());

	// Transduce
	auto pLabels3 = pSupLearner->transduce(*pFeatures1, *pLabels1, *pFeatures2);

	// Print results
	pLabels3->print(cout);
}

void WLearnerLib::TransductiveAccuracy(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	//	bool confusion = false;
	//	bool confusioncsv = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		/*		else if(args.if_pop("-confusion"))
			confusion = true;
		else if(args.if_pop("-confusioncsv"))
			confusioncsv = true;*/
		else
			throw Ex("Invalid transacc option: ", args.peek());
	}

	// Load the data sets
	std::unique_ptr<WMatrix> hFeatures1, hLabels1, hFeatures2, hLabels2;
	loadData(args, hFeatures1, hLabels1, true);
	loadData(args, hFeatures2, hLabels2, true);
	WMatrix* pFeatures1 = hFeatures1.get();
	WMatrix* pLabels1 = hLabels1.get();
	WMatrix* pFeatures2 = hFeatures2.get();
	WMatrix* pLabels2 = hLabels2.get();
	if(pFeatures1->cols() != pFeatures2->cols() || pLabels1->cols() != pLabels2->cols())
		throw Ex("The training and test datasets must have the same number of columns.");

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args, pFeatures1, pLabels1);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	pSupLearner->rand().setSeed(seed);

	// Transduce and measure accuracy
	vector<WMatrix*> confusionMatrices;
	double mse = pSupLearner->trainAndTest(*pFeatures1, *pLabels1, *pFeatures2, *pLabels2) / pFeatures2->rows();
	if(pLabels2->cols() == 1 && pLabels2->relation().valueCount(0) > 0)
		cout << "Misclassification rate: ";
	else
		cout << "Mean squared error: ";
	cout << to_str(mse) << "\n";
	/*
	// Print the confusion matrix
	if(confusion){
		printConfusionMatrices(pLabels2->relation().get(), confusionMatrices);
	}
	if(confusioncsv){
		printMachineReadableConfusionMatrices(pLabels2->relation().get(), confusionMatrices);
	}
*/
}

void WLearnerLib::SplitTest(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	double trainRatio = 0.5;
	size_t reps = 1;
	string lastModelFile = "";
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
		{
			seed = args.pop_uint();
		}
		else if(args.if_pop("-trainratio"))
		{
			trainRatio = args.pop_double();
		}
		else if(args.if_pop("-reps"))
		{
			reps = args.pop_uint();
		}
		else if(args.if_pop("-writelastmodel"))
		{
			lastModelFile = args.pop_string();
		}
		else
		{
			throw Ex("Invalid splittest option: ", args.peek());
		}
	}
	if(trainRatio < 0 || trainRatio > 1)
		throw Ex("trainratio must be between 0 and 1");

	// Load the data
	WRand prng(seed);
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	pSupLearner->rand().setSeed(seed);

	// Ensure that can write if we are required to
	if(!pSupLearner->canGeneralize() && lastModelFile != "")
	{
		throw Ex("The learner specified does not have an internal model "
		         "and thus cannot be saved to a file.  Remove the "
		         "-lastModelFile argument.");
	}

	// Do the reps
	size_t trainingPatterns = std::max((size_t)1, std::min(pFeatures->rows() - 1, (size_t)floor(pFeatures->rows() * trainRatio + 0.5)));
	size_t testPatterns = pFeatures->rows() - trainingPatterns;
	// Results is the mean results for all columns (plus some extra
	// storage to make allocation and deallocation easier)
	double sumMSE = 0.0;
	for(size_t i = 0; i < reps; i++)
	{
		// Shuffle and split the data
		pFeatures->shuffle(prng, pLabels);
		WMatrix testFeatures(pFeatures->relation().clone());
		WMatrix testLabels(pLabels->relation().clone());
		{
			WMergeDataHolder hFeatures2(*pFeatures, testFeatures);
			WMergeDataHolder hLabels2(*pLabels, testLabels);
			testFeatures.reserve(testPatterns);
			testLabels.reserve(testPatterns);
			pFeatures->splitBySize(testFeatures, testPatterns);
			pLabels->splitBySize(testLabels, testPatterns);

			// Test and print results
			vector<WMatrix*> confusionMatrices;
			double mse = pSupLearner->trainAndTest(*pFeatures, *pLabels, testFeatures, testLabels) / testFeatures.rows();

			// Write trained model file on last repetition
			if(lastModelFile != "" && i + 1 == reps)
			{
				WSupervisedLearner* pSup =
				  dynamic_cast<WSupervisedLearner*>(pSupLearner);
				WDom doc;
				WDomNode* pRoot = pSup->serialize(&doc);
				doc.setRoot(pRoot);
				std::ofstream out(lastModelFile.c_str());
				if(out)
				{
					doc.writeJson(out);
				}
			}
			cout << "rep " << i << ") ";
			if(testLabels.cols() == 1 && testLabels.relation().valueCount(0) > 0)
				cout << "Misclassification rate: ";
			else
				cout << "Mean squared error: ";
			cout << to_str(mse) << "\n";
			sumMSE += mse;
		}
	}
	if(pLabels->cols() == 1 && pLabels->relation().valueCount(0) > 0)
		cout << "Average misclassification rate: ";
	else
		cout << "Average mean squared error: ";
	cout << to_str(sumMSE / reps) << "\n";
}

void WLearnerLib::CrossValidateCallback(void* pSupLearner, size_t nRep, size_t nFold, double foldSSE, size_t rows)
{
	cout << "Rep: " << nRep << ", Fold: " << nFold << ", Mean squared error: " << to_str(foldSSE / rows) << "\n";
}

void WLearnerLib::CrossValidate(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	int reps = 5;
	int folds = 2;
	bool succinct = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-reps"))
			reps = args.pop_uint();
		else if(args.if_pop("-folds"))
			folds = args.pop_uint();
		else if(args.if_pop("-succinct"))
			succinct = true;
		else
			throw Ex("Invalid crossvalidate option: ", args.peek());
	}
	if(reps < 1)
		throw Ex("There must be at least 1 rep.");
	if(folds < 2)
		throw Ex("There must be at least 2 folds.");

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	pSupLearner->rand().setSeed(seed);

	// Test
	cout.precision(8);
	double sae;
	double sse = pSupLearner->repValidate(*pFeatures, *pLabels, reps, folds, &sae, succinct ? nullptr : CrossValidateCallback, pSupLearner);
	if(succinct)
		cout << to_str(sse / pFeatures->rows());
	else
	{
		if(pLabels->cols() == 1 && pLabels->relation().valueCount(0) > 0)
		{
			cout << "Misclassification rate: " << to_str(sse / pFeatures->rows()) << "\n";
			cout << "Predictive accuracy: " << to_str(1.0 - (sse / pFeatures->rows())) << "\n";
		}
		else
		{
			cout << "Sum absolute error: " << to_str(sae) << "\n";
			cout << "Mean absolute error: " << to_str(sae / pFeatures->rows()) << "\n";
			cout << "Sum squared error: " << to_str(sse) << "\n";
			cout << "Mean squared error: " << to_str(sse / pFeatures->rows()) << "\n";
			cout << "Root mean squared error: " << to_str(sqrt(sse / pFeatures->rows())) << "\n";
		}
	}
}

void WLearnerLib::vette(string& s)
{
	for(size_t i = 0; i < s.length(); i++)
	{
		if(s[i] <= ' ' || s[i] == '\'' || s[i] == '"')
			s[i] = '_';
	}
}

void WLearnerLib::PrecisionRecall(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	int reps = 5;
	int samples = 100;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-reps"))
			reps = args.pop_uint();
		else if(args.if_pop("-samples"))
			samples = args.pop_uint();
		else
			throw Ex("Invalid precisionrecall option: ", args.peek());
	}
	if(reps < 1)
		throw Ex("There must be at least 1 rep.");
	if(samples < 2)
		throw Ex("There must be at least 2 samples.");

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args, pFeatures, pLabels);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	pSupLearner->rand().setSeed(seed);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	if(!pSupLearner->canGeneralize())
		throw Ex("This algorithm cannot be \"trained\". It can only be used to \"transduce\".");
	WSupervisedLearner* pModel = (WSupervisedLearner*)pSupLearner;

	// Build the relation for the results
	WArffRelation* pRelation = new WArffRelation();
	pRelation->setName("untitled");
	WArffRelation* pRel = pRelation;
	pRel->addAttribute("recall", 0, nullptr);
	for(size_t i = 0; i < pLabels->cols(); i++)
	{
		size_t valCount = std::max((size_t)1, pLabels->relation().valueCount(i));
		for(int val = 0; val < (int)valCount; val++)
		{
			string s = "precision_";
			if(pLabels->relation().type() == WRelation::ARFF)
				s += ((const WArffRelation&)pLabels->relation()).attrName(i);
			else
			{
				s += "attr";
				s += to_str(i);
			}
			if(valCount > 1)
			{
				s += '_';
				ostringstream oss;
				pLabels->relation().printAttrValue(oss, i, val);
				s += oss.str();
			}
			vette(s);
			pRel->addAttribute(s.c_str(), 0, nullptr);
		}
	}

	// Measure precision/recall
	WMatrix results(pRelation);
	results.newRows(samples);
	for(int i = 0; i < samples; i++)
		results.row(i)[0] = (double)i / samples;
	size_t pos = 1;
	for(size_t i = 0; i < pLabels->cols(); i++)
	{
		size_t valCount = std::max((size_t)1, pLabels->relation().valueCount(i));
		double* precision = new double[valCount * samples];
		std::unique_ptr<double[]> hPrecision(precision);
		pModel->precisionRecall(precision, samples, *pFeatures, *pLabels, i, reps);
		for(size_t j = 0; j < valCount; j++)
			results.setCol(pos++, precision + samples * j);
	}
	WAssert(pos == pRelation->size()); // counting problem
	results.print(cout);
}

void WLearnerLib::sterilize(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	size_t folds = 10;
	double diffThresh = 0.1;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-folds"))
			folds = args.pop_uint();
		else if(args.if_pop("-diffthresh"))
			diffThresh = args.pop_double();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Instantiate the modeler
	WTransducer* pTransducer = InstantiateAlgorithm(args, pFeatures, pLabels);
	std::unique_ptr<WTransducer> hModel(pTransducer);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	pTransducer->rand().setSeed(seed);

	// Sterilize
	WMatrix sterileFeatures(pFeatures->relation().clone());
	WReleaseDataHolder hSterileFeatures(&sterileFeatures);
	WMatrix sterileLabels(pLabels->relation().clone());
	WReleaseDataHolder hSterileLabels(&sterileLabels);
	for(size_t fold = 0; fold < folds; fold++)
	{
		// Split the data
		WMatrix trainFeatures(pFeatures->relation().clone());
		WReleaseDataHolder hTrainFeatures(&trainFeatures);
		WMatrix trainLabels(pLabels->relation().clone());
		WReleaseDataHolder hTrainLabels(&trainLabels);
		WMatrix testFeatures(pFeatures->relation().clone());
		WReleaseDataHolder hTestFeatures(&testFeatures);
		WMatrix testLabels(pLabels->relation().clone());
		WReleaseDataHolder hTestLabels(&testLabels);
		size_t foldBegin = fold * pFeatures->rows() / folds;
		size_t foldEnd = (fold + 1) * pFeatures->rows() / folds;
		for(size_t i = 0; i < foldBegin; i++)
		{
			trainFeatures.takeRow(&pFeatures->row(i));
			trainLabels.takeRow(&pLabels->row(i));
		}
		for(size_t i = foldBegin; i < foldEnd; i++)
		{
			testFeatures.takeRow(&pFeatures->row(i));
			testLabels.takeRow(&pLabels->row(i));
		}
		for(size_t i = foldEnd; i < pFeatures->rows(); i++)
		{
			trainFeatures.takeRow(&pFeatures->row(i));
			trainLabels.takeRow(&pLabels->row(i));
		}

		// Transduce
		auto pPredictedLabels = pTransducer->transduce(trainFeatures, trainLabels, testFeatures);

		// Keep only the correct predictions
		for(size_t j = 0; j < testLabels.rows(); j++)
		{
			WVec& target = testLabels[j];
			WVec& predicted = pPredictedLabels->row(j);
			for(size_t i = 0; i < testLabels.cols(); i++)
			{
				size_t vals = testLabels.relation().valueCount(i);
				bool goodEnough = false;
				if(vals == 0)
				{
					if(std::abs(target[i] - predicted[i]) < diffThresh)
						goodEnough = true;
				}
				else
				{
					if(target[i] == predicted[i])
						goodEnough = true;
				}
				if(goodEnough)
				{
					sterileFeatures.takeRow(&testFeatures[j]);
					sterileLabels.takeRow(&testLabels[j]);
				}
			}
		}
	}

	// Merge the sterile features and labels
	WMatrix* pSterile = WMatrix::mergeHoriz(&sterileFeatures, &sterileLabels);
	std::unique_ptr<WMatrix> hSterile(pSterile);
	pSterile->print(cout);
}
/*
void WLearnerLib::trainRecurrent(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(NULL);
	vector<size_t> paramDims;
	const char* stateFilename = NULL;
	double validationInterval = 0;
	vector<string> validationFilenames;
	const char* outFilename = "model.json";
	double trainTime = 60 * 60; // 1 hour
	bool useIsomap = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-paramdims"))
		{
			unsigned int count = args.pop_uint();
			for(unsigned int i = 0; i < count; i++)
				paramDims.push_back(args.pop_uint());
		}
		else if(args.if_pop("-state"))
			stateFilename = args.pop_string();
		else if(args.if_pop("-validate"))
		{
			validationInterval = args.pop_double();
			int count = args.pop_uint();
			for(int i = 0; i < count; i++)
			{
				validationFilenames.push_back(args.pop_string());
				validationFilenames.push_back(args.pop_string());
			}
		}
		else if(args.if_pop("-out"))
			outFilename = args.pop_string();
		else if(args.if_pop("-traintime"))
			trainTime = args.pop_double();
		else if(args.if_pop("-isomap"))
			useIsomap = true;
		else
			throw Ex("Invalid trainRecurrent option: ", args.peek());
	}

	// Parse the algorithm
	const char* alg = args.pop_string();
//	int bpttDepth = 0;
//	int bpttItersPerGrow = 0;
	double annealDeviation = 0.0;
	double annealDecay = 0.0;
	double annealTimeWindow = 0.0;
	if(strcmp(alg, "moses") == 0)
	{
	}
	else if(strcmp(alg, "evolutionary") == 0)
	{
	}
	else if(strcmp(alg, "hillclimber") == 0)
	{
	}
	else if(strcmp(alg, "annealing") == 0)
	{
		annealDeviation = args.pop_double();
		annealDecay = args.pop_double();
		annealTimeWindow = args.pop_double();
	}
	else
		throw Ex("Unrecognized recurrent model training algorithm: ", alg);

	// Load the data
	WMatrix dataObs;
	dataObs.loadArff(args.pop_string());
	WMatrix dataAction;
	dataAction.loadArff(args.pop_string());

	// Get the number of context dims
	int contextDims = args.pop_uint();

	// Infer remaining values and check that the parts fit together
	size_t pixels = 1;
	for(vector<size_t>::iterator it = paramDims.begin(); it != paramDims.end(); it++)
		pixels *= *it;
	size_t channels = dataObs.cols() / pixels;
	if((channels * pixels) != dataObs.cols())
		throw Ex("The number of columns in the observation data must be a multiple of the product of the param dims");

	// Instantiate the recurrent model
	WTransducer* pTransitionFunc = InstantiateAlgorithm(args, NULL, NULL);
	std::unique_ptr<WTransducer> hTransitionFunc(pTransitionFunc);
	if(!pTransitionFunc->canGeneralize())
		throw Ex("The algorithm specified for the transition function cannot be \"trained\". It can only be used to \"transduce\".");
	pTransitionFunc->rand().setSeed(seed);
	WTransducer* pObservationFunc = InstantiateAlgorithm(args, NULL, NULL);
	std::unique_ptr<WTransducer> hObservationFunc(pObservationFunc);
	if(!pObservationFunc->canGeneralize())
		throw Ex("The algorithm specified for the observation function cannot be \"trained\". It can only be used to \"transduce\".");
	pObservationFunc->rand().setSeed((seed + 13) * 11);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	WRand prng(seed);
	MyRecurrentModel model((WSupervisedLearner*)hTransitionFunc.release(), (WSupervisedLearner*)hObservationFunc.release(), dataAction.cols(), contextDims, dataObs.cols(), &prng, &paramDims, stateFilename, validationInterval);

	// Set it up to do validation during training if specified
	vector<WMatrix*> validationData;
	VectorOfPointersHolder<WMatrix> hValidationData(validationData);
	if(validationInterval > 0)
	{
		for(size_t i = 0; i < validationFilenames.size(); i++)
		{
			WMatrix* pVal = new WMatrix();
			std::unique_ptr<WMatrix> hVal(pVal);
			pVal->loadArff(validationFilenames[i].c_str());
			validationData.push_back(hVal.release());
		}
		model.validateDuringTraining(validationInterval, &validationData);
		cout << "@RELATION validation_scores\n\n@ATTRIBUTE seconds real\n@ATTRIBUTE " << alg << " real\n\n@DATA\n";
	}

	// Set other flags
	model.setTrainingSeconds(trainTime);
	model.setUseIsomap(useIsomap);

	// Do the training
	if(strcmp(alg, "moses") == 0)
		model.trainMoses(&dataAction, &dataObs);
	else if(strcmp(alg, "evolutionary") == 0)
		model.trainEvolutionary(&dataAction, &dataObs);
	else if(strcmp(alg, "hillclimber") == 0)
		model.trainHillClimber(&dataAction, &dataObs, 0.0, 0.0, 0.0, true, false);
	else if(strcmp(alg, "annealing") == 0)
		model.trainHillClimber(&dataAction, &dataObs, annealDeviation, annealDecay, annealTimeWindow, false, true);
	WDom doc;
	doc.setRoot(model.serialize(&doc));
	doc.saveJson(outFilename);
}
*/
void WLearnerLib::regress(WArgReader& args)
{
	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();
	if(pLabels->cols() != 1)
		throw Ex("Sorry, only 1 label dimension currently supported");

	// Load the equation
	string expr;
	while(args.size() > 0)
		expr += args.pop_string();
	WFunctionParser fp;
	fp.add(expr.c_str());
	WFunction* pFunc = fp.getFunctionNoThrow("f");
	if(!pFunc)
		throw Ex("Expected a function named \"f\".");
	if((size_t)pFunc->m_expectedParams <= pFeatures->cols())
		throw Ex("Expected more than", to_str(pFeatures->cols()), " params. Got only ", to_str(pFunc->m_expectedParams));

	// Optimize
	OptimizerTargetFunc tf(pFeatures, pLabels, pFunc, &fp);
	WHillClimber hc(&tf);
	hc.searchUntil(10000, 200, 0.01);
	double err = hc.currentError();
	cout << "SSE = " << to_str(err) << "\n";
	cout << "Params:\n";
	const WVec& pVec = hc.currentVector();
	for(size_t i = 0; i < (size_t)pFunc->m_expectedParams - pFeatures->cols(); i++)
	{
		if(i > 0)
			cout << ", ";
		cout << to_str(pVec[i]);
	}
	cout << "\n";
}

void WLearnerLib::metaData(WArgReader& args)
{
	// Load the data
	std::unique_ptr<WMatrix> hFeatures, hLabels;
	loadData(args, hFeatures, hLabels);
	WMatrix* pFeatures = hFeatures.get();
	WMatrix* pLabels = hLabels.get();

	// Make the relation
	WArffRelation* pRel = new WArffRelation();
	pRel->addAttribute("log_rows", 0, nullptr);
	pRel->addAttribute("log_feature_dims", 0, nullptr);
	pRel->addAttribute("log_label_dims", 0, nullptr);
	pRel->addAttribute("log_feature_elements", 0, nullptr);
	pRel->addAttribute("log_sum_feature_vals", 0, nullptr);
	pRel->addAttribute("mean_feature_vals", 0, nullptr);
	pRel->addAttribute("feature_range_deviation", 0, nullptr);
	pRel->addAttribute("feature_portion_real", 0, nullptr);
	pRel->addAttribute("label_portion_real", 0, nullptr);
	pRel->addAttribute("feature_is_missing_values", 0, nullptr);
	pRel->addAttribute("label_entropy", 0, nullptr);
	pRel->addAttribute("label_skew", 0, nullptr);
	pRel->addAttribute("landmark_baseline", 0, nullptr);
	pRel->addAttribute("landmark_linear", 0, nullptr);
	pRel->addAttribute("landmark_decisiontree", 0, nullptr);
	pRel->addAttribute("landmark_shallowtree", 0, nullptr);
	pRel->addAttribute("landmark_meanmarginstree", 0, nullptr);
	pRel->addAttribute("landmark_naivebayes", 0, nullptr);

	// Make the meta-data
	WMatrix meta(pRel);
	WVec& row = meta.newRow();
	size_t r = 0;

	// log_rows
	row[r++] = log((double)pFeatures->rows());

	// log_feature_dims
	row[r++] = log((double)pFeatures->cols());

	// log_label_dims
	row[r++] = log((double)pLabels->cols());

	// log_feature_elements
	row[r++] = log((double)(pFeatures->rows() * pFeatures->cols()));

	// log_sum_feature_vals
	size_t sum = 0;
	for(size_t i = 0; i < pFeatures->cols(); i++)
		sum += pFeatures->relation().valueCount(i);
	row[r++] = log((double)(sum + 1));

	// mean_feature_vals
	row[r++] = (double)sum / pFeatures->cols();

	// feature_range_deviation
	{
		double s = 0.0;
		double ss = 0.0;
		for(size_t i = 0; i < pFeatures->cols(); i++)
		{
			double range = pFeatures->columnMax(i) - pFeatures->columnMin(i);
			s += range;
			ss += (range * range);
		}
		s /= pFeatures->cols();
		ss /= pFeatures->cols();
		row[r++] = (double)(pFeatures->cols() - 1) / pFeatures->cols() * sqrt(ss - (s * s));
	}

	// feature_portion_real
	size_t realCount = 0;
	for(size_t i = 0; i < pFeatures->cols(); i++)
	{
		if(pFeatures->relation().valueCount(i) == 0)
			realCount++;
	}
	row[r++] = (double)realCount / pFeatures->cols();

	// label_portion_real
	realCount = 0;
	for(size_t i = 0; i < pLabels->cols(); i++)
	{
		if(pLabels->relation().valueCount(i) == 0)
			realCount++;
	}
	row[r++] = (double)realCount / pLabels->cols();

	// features_is_missing_values
	row[r++] = pFeatures->doesHaveAnyMissingValues() ? 1.0 : 0.0;

	// label_entropy
	double dsum = 0.0;
	for(size_t i = 0; i < pLabels->cols(); i++)
	{
		if(pLabels->relation().valueCount(i) == 0)
		{
			double mean = pLabels->columnMean(i);
			dsum += log(1.0 + sqrt(pLabels->columnVariance(i, mean)));
		}
		else
			dsum += pLabels->entropy(i);
	}
	row[r++] = dsum / pLabels->cols();

	// label_skew
	dsum = 0.0;
	for(size_t i = 0; i < pLabels->cols(); i++)
	{
		if(pLabels->relation().valueCount(i) == 0)
		{
			double mean = pLabels->columnMean(i);
			double median = pLabels->columnMedian(i);
			dsum += log(1.0 + std::abs(mean - median));
		}
		else
		{
			double mostCommonValue = pLabels->baselineValue(i);
			size_t count = 0;
			for(size_t j = 0; j < pLabels->rows(); j++)
			{
				if(pLabels->row(j)[i] == mostCommonValue)
					count++;
			}
			dsum += (double)count / pLabels->rows();
		}
	}
	row[r++] = dsum / pLabels->cols();

	// landmark_baseline
	{
		WBaselineLearner model;
		row[r++] = model.repValidate(*pFeatures, *pLabels, 5, 2) / pFeatures->rows();
	}

	// landmark_linear
	{
		WLinearRegressor model;
		row[r++] = model.repValidate(*pFeatures, *pLabels, 5, 2) / pFeatures->rows();
	}

	// landmark_decisiontree
	{
		WDecisionTree model;
		model.useBinaryDivisions();
		row[r++] = model.repValidate(*pFeatures, *pLabels, 5, 2) / pFeatures->rows();
	}

	// landmark_shallowtree
	{
		WDecisionTree model;
		model.useBinaryDivisions();
		model.setLeafThresh(24);
		row[r++] = model.repValidate(*pFeatures, *pLabels, 5, 2) / pFeatures->rows();
	}

	// landmark_meanmarginstree
	{
		WMeanMarginsTree model;
		row[r++] = model.repValidate(*pFeatures, *pLabels, 5, 2) / pFeatures->rows();
	}

	// landmark_naivebayes
	{
		WNaiveBayes model;
		row[r++] = model.repValidate(*pFeatures, *pLabels, 5, 2) / pFeatures->rows();
	}

	// Print the results
	meta.print(cout);
}

void WLearnerLib::ShowUsage(const char* appName)
{
	cout << "Full Usage Information\n";
	cout << "[Square brackets] are used to indicate required arguments.\n";
	cout << "<Angled brackets> are used to indicate optional arguments.\n";
	cout << "\n";
	UsageNode* pUsageTree = makeLearnUsageTree();
	std::unique_ptr<UsageNode> hUsageTree(pUsageTree);
	pUsageTree->print(cout, 0, 3, 76, 1000, true);
	UsageNode* pUsageTree2 = makeAlgorithmUsageTree();
	std::unique_ptr<UsageNode> hUsageTree2(pUsageTree2);
	pUsageTree2->print(cout, 0, 3, 76, 1000, true);
	cout.flush();
}

void WLearnerLib::showError(WArgReader& args, const char* szAppName, const char* szMessage)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	args.set_pos(1);
	const char* szCommand = args.peek();
	UsageNode* pUsageTree = makeLearnUsageTree();
	std::unique_ptr<UsageNode> hUsageTree(pUsageTree);
	if(szCommand)
	{
		UsageNode* pUsageCommand = pUsageTree->choice(szCommand);
		if(pUsageCommand)
		{
			cerr << "Brief Usage Information:\n\n";
			cerr << szAppName << " ";
			pUsageCommand->print(cerr, 0, 3, 76, 1000, true);
			if(pUsageCommand->findPart("[algorithm]") >= 0)
			{
				UsageNode* pAlgTree = makeAlgorithmUsageTree();
				std::unique_ptr<UsageNode> hAlgTree(pAlgTree);
				pAlgTree->print(cerr, 1, 3, 76, 2, false);
			}
		}
		else
		{
			cerr << "Brief Usage Information:\n\n";
			pUsageTree->print(cerr, 0, 3, 76, 1, false);
		}
	}
	else
	{
		pUsageTree->print(cerr, 0, 3, 76, 1, false);
		cerr << "\nFor more specific usage information, enter as much of the command as you know.\n";
	}
	cerr << "\nTo see full usage information, run:\n	" << szAppName << " usage\n\n";
	cerr << "For a graphical tool that will help you to build a command, run:\n	waffles_wizard\n";
	cerr.flush();
}
