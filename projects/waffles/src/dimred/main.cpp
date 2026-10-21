/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer
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

#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include "WClasses/WApp.h"
#include "WClasses/WBits.h"
#include "WClasses/WCluster.h"
#include "WClasses/WDistance.h"
#include "WClasses/WError.h"
#include "WClasses/WMatrix.h"
#include "WClasses/WImage.h"
#include "WClasses/WRand.h"
#include "WClasses/WFile.h"
#include "WClasses/WTransform.h"
#include "WClasses/WVec.h"
#include "WClasses/WHashTable.h"
#include "WClasses/WHillClimber.h"
#include "WClasses/WManifold.h"
#include "WClasses/WNeighborFinder.h"
#include "WClasses/WNeuralNet.h"
#include "WClasses/WHeap.h"
#include "WClasses/WRect.h"
#include "WClasses/WSparseMatrix.h"
#include "WClasses/WMath.h"
#include "WClasses/WDom.h"
#include "WClasses/WSelfOrganizingMap.h"
#include <time.h>
#include <iostream>
#include <string>
#include <set>
#include <memory>
#include <map>
#include <algorithm>
#ifdef WIN32
#include <direct.h>
#include <process.h>
#endif
#include <exception>
#include "WClasses/usage.h"

using namespace WClasses;
using std::cout;
using std::cerr;
using std::vector;
using std::string;
using std::set;
using std::map;
using std::shared_ptr;

size_t getAttrVal(const char* szString, size_t attrCount)
{
	bool fromRight = false;
	if(*szString == '*')
	{
		fromRight = true;
		szString++;
	}
	if(*szString < '0' || *szString > '9')
		throw Ex("Expected a digit while parsing attribute list");
#ifdef WIN32
	size_t val = (size_t)_strtoui64(szString, (char**)NULL, 10);
#else
	size_t val = strtoull(szString, (char**)nullptr, 10);
#endif
	if(fromRight)
		val = attrCount - 1 - val;
	return val;
}

void parseAttributeList(vector<size_t>& list, WArgReader& args, size_t attrCount)
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

///Return a pointer to newly allocated data read from the command line
///represented by args.
///
///The returned matrix is allocated by new and it is the caller's
///responsibility to deallocate it. The suggested manner is to use a
///Holder<WMatrix*>
///
///In the returned matrix, all of the attributes designated as labels
///have been moved to the end and ignored attributes have been
///removed. The original indices of all the attributes are returned in
///originalIndices.
///
///\param args the command-line arguments
///
///\param pLabelDims (out parameter) the index of the first attribute
///which is designated a label.
///
///\param originalIndices the vector in which to place the original
///indices.  originalIndices[i] is the index in the original data file
///of the attribute currently at index i.
void loadDataWithSwitches(WMatrix& data, WArgReader& args, size_t& pLabelDims, std::vector<size_t>& originalIndices)
{
	// Load the dataset by extension
	const char* szFilename = args.pop_string();
	PathData pd;
	WFile::parsePath(szFilename, &pd);
	if(_stricmp(szFilename + pd.extStart, ".arff") == 0)
		data.loadArff(szFilename);
	else if(_stricmp(szFilename + pd.extStart, ".csv") == 0)
	{
		WCSVParser parser;
		parser.parse(data, szFilename);
		cerr << "\nParsing Report:\n";
		for(size_t i = 0; i < data.cols(); i++)
			cerr << to_str(i) << ") " << parser.report(i) << "\n";
	}
	else if(_stricmp(szFilename + pd.extStart, ".dat") == 0)
	{
		WCSVParser parser;
		parser.setSeparator('\0');
		parser.parse(data, szFilename);
		cerr << "\nParsing Report:\n";
		for(size_t i = 0; i < data.cols(); i++)
			cerr << to_str(i) << ") " << parser.report(i) << "\n";
	}
	else
		throw Ex("Unsupported file format: ", szFilename + pd.extStart);

	//Make the initial list of original indices
	originalIndices.resize(data.cols());
	for(std::size_t i = 0; i < originalIndices.size(); ++i)
	{
		originalIndices.at(i) = i;
	}

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
			break;
	}

	// Throw out the ignored attributes
	std::sort(ignore.begin(), ignore.end());
	for(size_t i = ignore.size() - 1; i < ignore.size(); i--)
	{
		data.deleteColumns(ignore[i], 1);
		originalIndices.erase(originalIndices.begin() + ignore[i]);
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
	pLabelDims = std::max((size_t)1, labels.size());
	for(size_t i = 0; i < labels.size(); i++)
	{
		size_t src = labels[i];
		size_t dst = data.cols() - pLabelDims + i;
		if(src != dst)
		{
			data.swapColumns(src, dst);
			std::swap(originalIndices.at(src), originalIndices.at(dst));
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
}

WMatrix* loadData(const char* szFilename)
{
	// Load the dataset by extension
	WMatrix* pM = new WMatrix();
	Holder<WMatrix> hM(pM);
	WMatrix& m = *pM;
	PathData pd;
	WFile::parsePath(szFilename, &pd);
	if(_stricmp(szFilename + pd.extStart, ".arff") == 0)
		m.loadArff(szFilename);
	else if(_stricmp(szFilename + pd.extStart, ".csv") == 0)
	{
		WCSVParser parser;
		parser.parse(m, szFilename);
		cerr << "\nParsing Report:\n";
		for(size_t i = 0; i < m.cols(); i++)
			cerr << to_str(i) << ") " << parser.report(i) << "\n";
	}
	else if(_stricmp(szFilename + pd.extStart, ".dat") == 0)
	{
		WCSVParser parser;
		parser.setSeparator('\0');
		parser.parse(m, szFilename);
		cerr << "\nParsing Report:\n";
		for(size_t i = 0; i < m.cols(); i++)
			cerr << to_str(i) << ") " << parser.report(i) << "\n";
	}
	else
		throw Ex("Unsupported file format: ", szFilename + pd.extStart);

	return hM.release();
}

void showInstantiateNeighborFinderError(const char* szMessage, WArgReader& args)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	const char* szNFName = args.peek();
	UsageNode* pNFTree = makeNeighborUsageTree();
	Holder<UsageNode> hNFTree(pNFTree);
	if(szNFName)
	{
		UsageNode* pUsageAlg = pNFTree->choice(szNFName);
		if(pUsageAlg)
		{
			cerr << "Partial Usage Information:\n\n";
			pUsageAlg->print(cerr, 0, 3, 76, 1000, true);
		}
		else
		{
			cerr << "\"" << szNFName << "\" is not a recognized neighbor-finding techniqie. Try one of these:\n\n";
			pNFTree->print(cerr, 0, 3, 76, 1, false);
		}
	}
	else
	{
		cerr << "Expected a neighbor-finding technique. Here are some choices:\n";
		pNFTree->print(cerr, 0, 3, 76, 1, false);
	}
	cerr << "\nTo see full usage information, run:\n	waffles_transform usage\n\n";
	cerr << "For a graphical tool that will help you to build a command, run:\n	waffles_wizard\n";
	cerr.flush();
}

WNeighborFinder* instantiateNeighborFinder(WMatrix* pData, size_t neighborCount, WRand* pRand, WArgReader& args)
{
	// Get the algorithm name
	int argPos = args.get_pos();
	WNeighborFinder* pNF = nullptr;
	const char* alg = args.pop_string();

	try
	{
		// Parse the options
		int cutCycleLen = 0;
		while(args.next_is_flag())
		{
			if(args.if_pop("-cyclecut"))
				cutCycleLen = args.pop_uint();
			else
				throw Ex("Invalid neighbor finder option: ", args.peek());
		}

		// Parse required algorithms
		if(_stricmp(alg, "bruteforce") == 0)
		{
			pNF = new WBruteForceNeighborFinder(pData, nullptr, true);
		}
		else if(_stricmp(alg, "kdtree") == 0)
		{
			pNF = new WKdTree(pData, nullptr, true);
		}
		else
			throw Ex("Unrecognized neighbor finding algorithm: ", alg);

		// Apply CycleCut
		if(cutCycleLen > 0)
		{
			WNeighborGraph* pNF2 = new WNeighborGraph(pNF, true, neighborCount);
			pNF2->cutShortcuts(cutCycleLen);
			pNF = pNF2;
		}
	}
	catch(const std::exception& e)
	{
		args.set_pos(argPos);
		showInstantiateNeighborFinderError(e.what(), args);
		throw Ex("nevermind"); // this means "don't display another error message"
	}

	return pNF;
}

void attributeSelector(WArgReader& args)
{
	// Load the data
	size_t labelDims;
	std::vector<size_t> originalIndices;
	WMatrix data;
	loadDataWithSwitches(data, args, labelDims, originalIndices);

	// Parse the options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	int targetFeatures = 1;
	string outFilename = "";
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-out"))
		{
			targetFeatures = args.pop_uint();
			outFilename = args.pop_string();
		}
		else
			throw Ex("Invalid neighbor finder option: ", args.peek());
	}

	// Do the attribute selection
	WAttributeSelector as(labelDims, targetFeatures);
	as.setSeed(seed);
	if(outFilename.length() > 0)
	{
		as.train(data);
		WMatrix* pDataOut = as.transformBatch(data);
		Holder<WMatrix> hDataOut(pDataOut);
		cout << "Reduced data saved to " << outFilename.c_str() << ".\n";
		pDataOut->saveArff(outFilename.c_str());
	}
	else
		as.train(data);
	cout << "\nAttribute rankings from most salient to least salient. (Attributes are zero-indexed.)\n";
	const WArffRelation& rel = (WArffRelation&)data.relation();
	for(size_t i = 0; i < as.ranks().size(); i++)
		cout << originalIndices.at(as.ranks()[i]) << " " << rel.attrName(as.ranks()[i]) << "\n";
}

void blendEmbeddings(WArgReader& args)
{
	// Load the files and params
	WMatrix* pDataOrig = loadData(args.pop_string());
	Holder<WMatrix> hDataOrig(pDataOrig);
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	WRand prng(seed);
	size_t neighborCount = args.pop_uint();
	WNeighborFinder* pNF = instantiateNeighborFinder(pDataOrig, neighborCount, &prng, args);
	Holder<WNeighborFinder> hNF(pNF);
	WMatrix* pDataA = loadData(args.pop_string());
	Holder<WMatrix> hDataA(pDataA);
	WMatrix* pDataB = loadData(args.pop_string());
	Holder<WMatrix> hDataB(pDataB);
	if(pDataA->rows() != pDataOrig->rows() || pDataB->rows() != pDataOrig->rows())
		throw Ex("mismatching number of rows");
	if(pDataA->cols() != pDataB->cols())
		throw Ex("mismatching number of cols");

	// Parse Options
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			prng.setSeed(args.pop_uint());
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Get a neighbor table
	if(!pNF->isCached())
	{
		WNeighborGraph* pNF2 = new WNeighborGraph(hNF.release(), true, neighborCount);
		hNF.reset(pNF2);
		pNF = pNF2;
	}

	// Do the blending
	size_t startPoint = (size_t)prng.next(pDataA->rows());
	double* pRatios = new double[pDataA->rows()];
	ArrayHolder<double> hRatios(pRatios);
	WVecWrapper vw(pRatios, pDataA->rows());
	vw.fill(0.5);
	WMatrix* pDataC = WManifold::blendEmbeddings(pDataA, pRatios, pDataB, neighborCount, (WNeighborGraph*)pNF, startPoint);
	Holder<WMatrix> hDataC(pDataC);
	pDataC->print(cout);
}

void breadthFirstUnfolding(WArgReader& args)
{
	// Load the file and params
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	size_t nSeed = getpid() * (unsigned int)time(nullptr);
	WRand prng(nSeed);
	size_t neighborCount = args.pop_uint();
	WNeighborFinder* pNF = instantiateNeighborFinder(pData, neighborCount, &prng, args);
	Holder<WNeighborFinder> hNF(pNF);
	int targetDims = args.pop_uint();

	// Parse Options
	size_t reps = 1;
	Holder<WMatrix> hControlData(nullptr);
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-reps"))
			reps = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Transform the data
	WBreadthFirstUnfolding transform(reps, neighborCount, targetDims);
	transform.rand().setSeed(nSeed);
	transform.setNeighborFinder(pNF);
	WMatrix* pDataAfter = transform.reduce(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);
	pDataAfter->print(cout);
}

void isomap(WArgReader& args)
{
	// Load the file and params
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	unsigned int nSeed = getpid() * (unsigned int)time(nullptr);
	WRand prng(nSeed);
	size_t neighborCount = args.pop_uint();
	WNeighborFinder* pNF = instantiateNeighborFinder(pData, neighborCount, &prng, args);
	Holder<WNeighborFinder> hNF(pNF);
	int targetDims = args.pop_uint();

	// Parse Options
	bool tolerant = false;
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			prng.setSeed(args.pop_uint());
		else if(args.if_pop("-tolerant"))
			tolerant = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Transform the data
	WIsomap transform(neighborCount, targetDims, &prng);
	transform.setNeighborFinder(pNF);
	if(tolerant)
		transform.dropDisconnectedPoints();
	WMatrix* pDataAfter = transform.reduce(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);
	pDataAfter->print(cout);
}

void lle(WArgReader& args)
{
	// Load the file and params
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	unsigned int nSeed = getpid() * (unsigned int)time(nullptr);
	WRand prng(nSeed);
	size_t neighborCount = args.pop_uint();
	WNeighborFinder* pNF = instantiateNeighborFinder(pData, neighborCount, &prng, args);
	Holder<WNeighborFinder> hNF(pNF);
	int targetDims = args.pop_uint();

	// Parse Options
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			prng.setSeed(args.pop_uint());
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Transform the data
	WLLE transform(neighborCount, targetDims, &prng);
	transform.setNeighborFinder(pNF);
	WMatrix* pDataAfter = transform.reduce(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);
	pDataAfter->print(cout);
}

void ManifoldSculpting(WArgReader& args)
{
	// Load the file and params
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	unsigned int nSeed = getpid() * (unsigned int)time(nullptr);
	WRand prng(nSeed);
	size_t neighborCount = args.pop_uint();
	WNeighborFinder* pNF = instantiateNeighborFinder(pData, neighborCount, &prng, args);
	Holder<WNeighborFinder> hNF(pNF);
	size_t targetDims = args.pop_uint();

	// Parse Options
	const char* szPreprocessedData = nullptr;
	double scaleRate = 0.999;
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			prng.setSeed(args.pop_uint());
		else if(args.if_pop("-continue"))
			szPreprocessedData = args.pop_string();
		else if(args.if_pop("-scalerate"))
			scaleRate = args.pop_double();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Load the hint data
	WMatrix* pDataHint = nullptr;
	Holder<WMatrix> hDataHint(nullptr);
	if(szPreprocessedData)
	{
		pDataHint = loadData(szPreprocessedData);
		hDataHint.reset(pDataHint);
		if(pDataHint->relation().size() != targetDims)
			throw Ex("Wrong number of dims in the hint data");
		if(pDataHint->rows() != pData->rows())
			throw Ex("Wrong number of patterns in the hint data");
	}

	// Transform the data
	WManifoldSculpting transform(neighborCount, targetDims, &prng);
	transform.setSquishingRate(scaleRate);
	if(pDataHint)
		transform.setPreprocessedData(hDataHint.release());
	transform.setNeighborFinder((WNeighborFinderGeneralizing*)pNF);
	WMatrix* pDataAfter = transform.reduce(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);
	pDataAfter->print(cout);
}

void multiDimensionalScaling(WArgReader& args)
{
	WRand prng(0);
	WMatrix* pDistances = loadData(args.pop_string());
	int targetDims = args.pop_uint();

	// Parse Options
	bool useSquaredDistances = false;
	while(args.size() > 0)
	{
		if(args.if_pop("-squareddistances"))
			useSquaredDistances = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	WMatrix* pResults = WManifold::multiDimensionalScaling(pDistances, targetDims, &prng, useSquaredDistances);
	Holder<WMatrix> hResults(pResults);
	pResults->print(cout);
}

void principalComponentAnalysis(WArgReader& args)
{
	// Load the file
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	int nTargetDims = args.pop_uint();

	// Parse options
	string roundTrip;
	size_t seed = getpid() * (unsigned int)time(nullptr);
	string eigenvalues;
	string components;
	string modelIn;
	string modelOut;
	bool aboutOrigin = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-roundtrip"))
			roundTrip = args.pop_string();
		else if(args.if_pop("-eigenvalues"))
			eigenvalues = args.pop_string();
		else if(args.if_pop("-components"))
			components = args.pop_string();
		else if(args.if_pop("-aboutorigin"))
			aboutOrigin = true;
		else if(args.if_pop("-modelin"))
			modelIn = args.pop_string();
		else if(args.if_pop("-modelout"))
			modelOut = args.pop_string();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Transform the data
	WPCA* pTransform = nullptr;
	if(modelIn.length() > 0)
	{
		WDom doc;
		doc.loadJson(modelIn.c_str());
		pTransform = new WPCA(doc.root());
	}
	else
	{
		pTransform = new WPCA(nTargetDims);
		if(aboutOrigin)
			pTransform->aboutOrigin();
		if(eigenvalues.length() > 0)
			pTransform->computeEigVals();
		pTransform->train(*pData);
	}
	Holder<WPCA> hTransform(pTransform);
	pTransform->rand().setSeed(seed);

	WMatrix* pDataAfter = pTransform->transformBatch(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);

	// Save the eigenvalues
	if(eigenvalues.length() > 0)
	{
		WArffRelation* pRelation = new WArffRelation();
		pRelation->addAttribute("eigenvalues", 0, nullptr);
		WMatrix dataEigenvalues(pRelation);
		dataEigenvalues.newRows(nTargetDims);
		WVec& pEigVals = pTransform->eigVals();
		for(int i = 0; i < nTargetDims; i++)
			dataEigenvalues[i][0] = pEigVals[i];
		dataEigenvalues.saveArff(eigenvalues.c_str());
	}

	// Save the components
	if(components.length() > 0)
		pTransform->components()->saveArff(components.c_str());

	// Do the round-trip
	if(roundTrip.size() > 0)
	{
		WMatrix roundTripped(pData->rows(), pData->cols());
		for(size_t i = 0; i < pData->rows(); i++)
			pTransform->untransform(pDataAfter->row(i), roundTripped.row(i));
		roundTripped.saveArff(roundTrip.c_str());
	}

	if(modelOut.length() > 0)
	{
		WDom doc;
		doc.setRoot(pTransform->serialize(&doc));
		doc.saveJson(modelOut.c_str());
	}

	pDataAfter->print(cout);
}

void scalingUnfolder(WArgReader& args)
{
	// Load the file and params
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	size_t nSeed = getpid() * (unsigned int)time(nullptr);
	WRand prng(nSeed);
	size_t neighborCount = args.pop_uint();
	WNeighborFinder* pNF = instantiateNeighborFinder(pData, neighborCount, &prng, args);
	Holder<WNeighborFinder> hNF(pNF);
	int targetDims = args.pop_uint();

	// Parse Options
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Transform the data
	WScalingUnfolder transform;
	transform.rand().setSeed(nSeed);
	transform.setNeighborCount(neighborCount);
	transform.setTargetDims(targetDims);
	//transform.setNeighborFinder(pNF);
	WMatrix* pDataAfter = transform.reduce(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);
	pDataAfter->print(cout);
}

void selfOrganizingMap(WArgReader& args)
{
	// Load the file
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);

	// Parse arguments
	std::vector<double> netDims;
	unsigned numNodes = 1;
	while(args.next_is_uint())
	{
		unsigned dim = args.pop_uint();
		netDims.push_back(dim);
		numNodes *= dim;
	}
	if(netDims.size() < 1)
	{
		throw Ex("No dimensions specified for self organizing map.  ", "A map must be at least 1 dimensional.");
	}

	WRand rand(getpid() * (unsigned int)time(nullptr));
	Holder<SOM::ReporterChain> reporters(new SOM::ReporterChain);
	Holder<SOM::TrainingAlgorithm> alg(nullptr);
	Holder<WDistanceMetric> weightDist(new WRowDistance);
	Holder<WDistanceMetric> nodeDist(new WRowDistance);
	Holder<SOM::NodeLocationInitialization> topology(new SOM::GridTopology);
	Holder<SOM::NodeWeightInitialization> weightInit(new SOM::NodeWeightInitializationTrainingSetSample(rand));
	Holder<SOM::NeighborhoodWindowFunction>
	  windowFunc(new SOM::GaussianWindowFunction());

	//Loading and saving
	string loadFrom = "";
	string saveTo = "";

	//Parameters for different training algorithms
	string algoName = "batch";
	double startWidth = -1; //Start width - set later if still negative
	double endWidth = -1; //End width   - set later if still negative
	double startRate = -1; //Start learning rate
	double endRate = -1; //End learning rate
	unsigned numIter = 100; //Total iterations
	unsigned numConverge = 1; //#steps for batch to converge

	while(args.next_is_flag())
	{
		if(args.if_pop("-tofile"))
		{
			saveTo = args.pop_string();
		}
		else if(args.if_pop("-fromfile"))
		{
			loadFrom = args.pop_string();
		}
		else if(args.if_pop("-seed"))
		{
			rand.setSeed(args.pop_uint());
		}
		else if(args.if_pop("-neighborhood"))
		{
			string name = args.pop_string();
			if(name == "gaussian")
			{
				windowFunc.reset(new SOM::GaussianWindowFunction());
			}
			else if(name == "uniform")
			{
				windowFunc.reset(new SOM::UniformWindowFunction());
			}
			else
			{
				throw Ex("Only gaussian and uniform are acceptible ", "neighborhood types");
			}
		}
		else if(args.if_pop("-printMeshEvery"))
		{
			using namespace SOM;
			unsigned interval = args.pop_uint();
			string baseFilename = args.pop_string();
			unsigned xDim = args.pop_uint();
			unsigned yDim = args.pop_uint();
			bool showTrain = false;
			if(args.if_pop("showTrain") || args.if_pop("showtrain"))
			{
				showTrain = true;
			}
			shared_ptr<Reporter> weightReporter(new SVG2DWeightReporter(baseFilename, xDim, yDim, showTrain));
			Holder<IterationIntervalReporter> intervalReporter(new IterationIntervalReporter(weightReporter, interval));
			reporters->add(intervalReporter.release());
		}
		else if(args.if_pop("-batchTrain"))
		{
			algoName = "batch";
			startWidth = args.pop_double();
			endWidth = args.pop_double();
			numIter = args.pop_uint();
			numConverge = args.pop_uint();
		}
		else if(args.if_pop("-stdTrain"))
		{
			algoName = "standard";
			startWidth = args.pop_double();
			endWidth = args.pop_double();
			startRate = args.pop_double();
			endRate = args.pop_double();
			numIter = args.pop_uint();
		}
		else
		{
			throw Ex("Invalid option: ", args.peek());
		}
	}

	//Create the training algorithm
	Holder<SOM::TrainingAlgorithm> algo;
	if(algoName == "batch")
	{
		double netRadius = *std::max_element(netDims.begin(), netDims.end());
		if(startWidth < 0) { startWidth = 2 * netRadius; }
		if(endWidth < 0) { endWidth = 1; }
		algo.reset(new SOM::BatchTraining(startWidth, endWidth, numIter, numConverge, weightInit.release(), windowFunc.release(), reporters.release()));
	}
	else if(algoName == "standard")
	{
		algo.reset(new SOM::TraditionalTraining(startWidth, endWidth, startRate, endRate, numIter, weightInit.release(), windowFunc.release(), reporters.release()));
	}
	else
	{
		throw Ex("Unknown type of training algorithm: \"", algoName, "\"");
	}

	//Create the network & transform the data
	Holder<WSelfOrganizingMap> som;
	Holder<WMatrix> out;

	if(loadFrom == "")
	{
		//Create map from arguments given
		som.reset(new WSelfOrganizingMap(netDims, numNodes, topology.release(), algo.release(), weightDist.release(), nodeDist.release()));
		//Train the network and transform the data in place
		out.reset(som->reduce(*pData));
	}
	else
	{
		//Create map from file
		WDom source;
		source.loadJson(loadFrom.c_str());
		som.reset(new WSelfOrganizingMap(source.root()));
		//Transform using the loaded network
		out.reset(som->transformBatch(*pData));
	}

	//Save the trained network
	if(saveTo != "")
	{
		WDom serialized;
		WDomNode* root = som->serialize(&serialized);
		serialized.setRoot(root);
		serialized.saveJson(saveTo.c_str());
	}

	//Print the result
	out->print(cout);
}

void singularValueDecomposition(WArgReader& args)
{
	// Load
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);

	// Parse options
	string ufilename = "u.arff";
	string sigmafilename;
	string vfilename = "v.arff";
	int maxIters = 100;
	while(args.size() > 0)
	{
		if(args.if_pop("-ufilename"))
			ufilename = args.pop_string();
		else if(args.if_pop("-sigmafilename"))
			sigmafilename = args.pop_string();
		else if(args.if_pop("-vfilename"))
			vfilename = args.pop_string();
		else if(args.if_pop("-maxiters"))
			maxIters = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	WMatrix* pU;
	double* pDiag;
	WMatrix* pV;
	pData->singularValueDecomposition(&pU, &pDiag, &pV, false, maxIters);
	Holder<WMatrix> hU(pU);
	ArrayHolder<double> hDiag(pDiag);
	Holder<WMatrix> hV(pV);
	pU->saveArff(ufilename.c_str());
	pV->saveArff(vfilename.c_str());
	if(sigmafilename.length() > 0)
	{
		WMatrix sigma(pU->rows(), pV->rows());
		sigma.fill(0.0);
		size_t m = std::min(sigma.rows(), (size_t)sigma.cols());
		for(size_t i = 0; i < m; i++)
			sigma.row(i)[i] = pDiag[i];
		sigma.saveArff(sigmafilename.c_str());
	}
	else
	{
		WVecWrapper diag(pDiag, std::min(pU->rows(), pV->rows()));
		diag.print(cout);
		cout << "\n";
	}
}
/*
void unsupervisedBackProp(WArgReader& args)
{
	// Load the file and params
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	int targetDims = args.pop_uint();

	// Parse Options
	unsigned int nSeed = getpid() * (unsigned int)time(NULL);
	WRand prng(nSeed);
	WUnsupervisedBackProp* pUBP = new WUnsupervisedBackProp(targetDims, &prng);
	Holder<WUnsupervisedBackProp> hUBP(pUBP);
	vector<size_t> paramRanges;
	string sModelOut;
	string sProgress;
	bool inputBias = true;
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			prng.setSeed(args.pop_uint());
		else if(args.if_pop("-addlayer"))
			pUBP->neuralNet()->addLayer(new WLayerClassic(FLEXIBLE_SIZE, args.pop_uint()));
		else if(args.if_pop("-params"))
		{
			if(pUBP->jitterer())
				throw Ex("You can't change the params after you add an image jitterer");
			size_t paramDims = args.pop_uint();
			for(size_t i = 0; i < paramDims; i++)
				paramRanges.push_back(args.pop_uint());
		}
		else if(args.if_pop("-modelin"))
		{
			WDom doc;
			doc.loadJson(args.pop_string());
			pUBP = new WUnsupervisedBackProp(doc.root());
			hUBP.reset(pUBP);
		}
		else if(args.if_pop("-modelout"))
			sModelOut = args.pop_string();
		else if(args.if_pop("-intrinsicin"))
		{
			WMatrix* pInt = new WMatrix();
			pInt->loadArff(args.pop_string());
			pUBP->setIntrinsic(pInt);
		}
		else if(args.if_pop("-jitter"))
		{
			if(paramRanges.size() != 2)
				throw Ex("The params must be set to 2 before a tweaker is set");
			size_t channels = args.pop_uint();
			double rot = args.pop_double();
			double trans = args.pop_double();
			double zoom = args.pop_double();
			WImageJitterer* pJitterer = new WImageJitterer(paramRanges[0], paramRanges[1], channels, rot, trans, zoom);
			pUBP->setJitterer(pJitterer);
		}
		else if(args.if_pop("-noinputbias"))
			inputBias = false;
		else if(args.if_pop("-progress"))
		{
			sProgress = args.pop_string();
			pUBP->trackProgress();
		}
		else if(args.if_pop("-onepass"))
			pUBP->onePass();
		else
			throw Ex("Invalid option: ", args.peek());
	}
	pUBP->setParams(paramRanges);
	pUBP->setUseInputBias(inputBias);
	pUBP->neuralNet()->addLayer(new WLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE));

	// Transform the data
	WMatrix* pDataAfter = pUBP->reduce(*pData);
	Holder<WMatrix> hDataAfter(pDataAfter);
	pDataAfter->print(cout);

	// Save the model (if requested)
	if(sModelOut.length() > 0)
	{
		WDom doc;
		doc.setRoot(pUBP->serialize(&doc));
		doc.saveJson(sModelOut.c_str());
	}
	if(sProgress.length() > 0)
		pUBP->progress().saveArff(sProgress.c_str());
}
*/
/*
void autoencoder(WArgReader& args)
{
	// Load the file
	WMatrix* pData = loadData(args.pop_string());
	Holder<WMatrix> hData(pData);
	int targetDims = args.pop_uint();

	// Parse options
	string roundTrip;
	size_t seed = getpid() * (unsigned int)time(NULL);
	string eigenvalues;
	string components;
	string modelIn;
	string modelOut;
	bool aboutOrigin = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else if(args.if_pop("-roundtrip"))
			roundTrip = args.pop_string();
		else if(args.if_pop("-eigenvalues"))
			eigenvalues = args.pop_string();
		else if(args.if_pop("-components"))
			components = args.pop_string();
		else if(args.if_pop("-aboutorigin"))
			aboutOrigin = true;
		else if(args.if_pop("-modelin"))
			modelIn = args.pop_string();
		else if(args.if_pop("-modelout"))
			modelOut = args.pop_string();
		else
			throw Ex("Invalid option: ", args.peek());
	}


	// Make the encoder
	WNeuralNet encoder;
	encoder.addLayer(new WLayerClassic(FLEXIBLE_SIZE, std::max(pData->cols() / 2, targetDims * 2), new WActivationHinge()));
	encoder.addLayer(new WLayerClassic(FLEXIBLE_SIZE, targetDims * 2, new WActivationHinge()));
	encoder.addLayer(new WLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE, new WActivationHinge()));
	encoder.setLearningRate(0.01);
	WUniformRelation relObs(pData->cols());
	WUniformRelation relInt(targetDims);
	encoder.beginIncrementalLearning(relObs, relInt);

	// Make the decoder
	WNeuralNet decoder;
	decoder.addLayer(new WLayerClassic(FLEXIBLE_SIZE, targetDims * 2, new WActivationHinge()));
	decoder.addLayer(new WLayerClassic(FLEXIBLE_SIZE, std::max(pData->cols() / 2, targetDims * 2), new WActivationHinge()));
	decoder.addLayer(new WLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE, new WActivationHinge()));
	decoder.setLearningRate(0.01);
	encoder.beginIncrementalLearning(relInt, relObs);

	// Train
	WRandomIndexIterator ii(pData->rows(), encoder.rand());
	double prevRmse = 1e200;
	while(true)
	{
		double sse = 0.0;
		ii.reset();
		size_t index;
		while(ii.next(index))
		{
			const double* pIn = observations[index];
			encoder.forwardProp(pIn);
			decoder.forwardProp(encoder.outputLayer().activation());
			sse += decoder.sumSquaredPredictionError(pIn);
			decoder.backpropagateAndRefineActivationFunction(pIn, decoder.learningRate());
			encoder.backpropagateFromLayer(&decoder.layer(0), encoder.learningRate());
			encoder.descendGradient(pIn, encoder.learningRate(), 0.0);
			decoder.descendGradient(encoder.outputLayer().activation(), decoder.learningRate(), 0.0);
		}
		double rmse = sqrt(sse / observations.rows());
		cout << "RMSE = " << to_str(rmse) << "\n";
		if(1.0 - (rmse / prevRmse) < 0.001)
			break;
		prevRmse = rmse;
	}
}
*/
void ShowUsage(const char* appName)
{
	cout << "Full Usage Information\n";
	cout << "[Square brackets] are used to indicate required arguments.\n";
	cout << "<Angled brackets> are used to indicate optional arguments.\n";
	cout << "\n";
	UsageNode* pUsageTree = makeDimRedUsageTree();
	Holder<UsageNode> hUsageTree(pUsageTree);
	pUsageTree->print(cout, 0, 3, 76, 1000, true);
	cout.flush();
}

void showError(WArgReader& args, const char* szAppName, const char* szMessage)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	args.set_pos(1);
	const char* szCommand = args.peek();
	UsageNode* pUsageTree = makeDimRedUsageTree();
	Holder<UsageNode> hUsageTree(pUsageTree);
	if(szCommand)
	{
		UsageNode* pUsageCommand = pUsageTree->choice(szCommand);
		if(pUsageCommand)
		{
			cerr << "Brief Usage Information:\n\n";
			cerr << szAppName << " ";
			pUsageCommand->print(cerr, 0, 3, 76, 1000, true);
			if(pUsageCommand->findPart("[neighbor-finder]") >= 0)
			{
				UsageNode* pNFTree = makeNeighborUsageTree();
				Holder<UsageNode> hNFTree(pNFTree);
				pNFTree->print(cerr, 1, 3, 76, 2, false);
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

int main(int argc, char* argv[])
{
#ifdef _DEBUG
	WApp::enableFloatingPointExceptions();
#endif
	int ret = 0;
	PathData pd;
	WFile::parsePath(argv[0], &pd);
	const char* appName = argv[0] + pd.fileStart;
	WArgReader args(argc, argv);
	args.pop_string(); // advance past the app name
	try
	{
		if(args.size() < 1) throw Ex("Expected a command");
		else if(args.if_pop("usage")) ShowUsage(appName);
		else if(args.if_pop("attributeselector")) attributeSelector(args);
		else if(args.if_pop("blendembeddings")) blendEmbeddings(args);
		else if(args.if_pop("breadthfirstunfolding")) breadthFirstUnfolding(args);
		else if(args.if_pop("isomap")) isomap(args);
		else if(args.if_pop("lle")) lle(args);
		else if(args.if_pop("manifoldsculpting")) ManifoldSculpting(args);
		else if(args.if_pop("multidimensionalscaling")) multiDimensionalScaling(args);
		else if(args.if_pop("pca")) principalComponentAnalysis(args);
		else if(args.if_pop("scalingunfolder")) scalingUnfolder(args);
		else if(args.if_pop("svd")) singularValueDecomposition(args);
		else if(args.if_pop("som")) selfOrganizingMap(args);
		else throw Ex("Unrecognized command: ", args.peek());
	}
	catch(const std::exception& e)
	{
		if(strcmp(e.what(), "nevermind") != 0) // if an error message was not already displayed...
			showError(args, appName, e.what());
		ret = 1;
	}

	return ret;
}
