/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
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
#include <map>
#ifdef WIN32
#include <direct.h>
#include <process.h>
#endif
#include <exception>
#include "WClasses/usage.h"
#include <memory>

using namespace WClasses;
using std::cout;
using std::cerr;
using std::vector;
using std::string;
using std::set;
using std::map;

void loadData(WMatrix& m, const char* szFilename)
{
	// Load the dataset by extension
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
}

void agglomerativeclusterer(WArgReader& args)
{
	// Load the file and params
	WMatrix data;
	loadData(data, args.pop_string());
	int clusters = args.pop_uint();

	// Do the clustering
	WAgglomerativeClusterer clusterer(clusters);
	WMatrix* pOut = clusterer.reduce(data);
	std::unique_ptr<WMatrix> hOut(pOut);
	pOut->print(cout);
}

void fuzzykmeans(WArgReader& args)
{
	// Load the file and params
	WMatrix data;
	loadData(data, args.pop_string());
	int clusters = args.pop_uint();

	// Parse Options
	unsigned int nSeed = getpid() * (unsigned int)time(nullptr);
	double fuzzifier = 1.3;
	size_t reps = 1;
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-fuzzifier"))
			fuzzifier = args.pop_double();
		else if(args.if_pop("-reps"))
			reps = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Do the clustering
	WRand prng(nSeed);
	WFuzzyKMeans clusterer(clusters, &prng);
	clusterer.setFuzzifier(fuzzifier);
	clusterer.setReps(reps);
	WMatrix* pOut = clusterer.reduce(data);
	std::unique_ptr<WMatrix> hOut(pOut);
	pOut->print(cout);
}

void kmeans(WArgReader& args)
{
	// Load the file and params
	WMatrix data;
	loadData(data, args.pop_string());
	int clusters = args.pop_uint();

	// Parse Options
	unsigned int nSeed = getpid() * (unsigned int)time(nullptr);
	size_t reps = 1;
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-reps"))
			reps = args.pop_uint();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Do the clustering
	WRand prng(nSeed);
	WKMeans clusterer(clusters, &prng);
	clusterer.setReps(reps);
	WMatrix* pOut = clusterer.reduce(data);
	std::unique_ptr<WMatrix> hOut(pOut);
	pOut->print(cout);
}

void kmedoids(WArgReader& args)
{
	// Load the file and params
	WMatrix data;
	loadData(data, args.pop_string());
	int clusters = args.pop_uint();

	// Do the clustering
	WKMedoids clusterer(clusters);
	WMatrix* pOut = clusterer.reduce(data);
	std::unique_ptr<WMatrix> hOut(pOut);
	pOut->print(cout);
}

void ShowUsage(const char* appName)
{
	cout << "Full Usage Information\n";
	cout << "[Square brackets] are used to indicate required arguments.\n";
	cout << "<Angled brackets> are used to indicate optional arguments.\n";
	cout << "\n";
	UsageNode* pUsageTree = makeClusterUsageTree();
	std::unique_ptr<UsageNode> hUsageTree(pUsageTree);
	pUsageTree->print(cout, 0, 3, 76, 1000, true);
	cout.flush();
}

void showError(WArgReader& args, const char* szAppName, const char* szMessage)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	args.set_pos(1);
	const char* szCommand = args.peek();
	UsageNode* pUsageTree = makeClusterUsageTree();
	std::unique_ptr<UsageNode> hUsageTree(pUsageTree);
	if(szCommand)
	{
		UsageNode* pUsageCommand = pUsageTree->choice(szCommand);
		if(pUsageCommand)
		{
			cerr << "Brief Usage Information:\n\n";
			cerr << szAppName << " ";
			pUsageCommand->print(cerr, 0, 3, 76, 1000, true);
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
		else if(args.if_pop("agglomerative")) agglomerativeclusterer(args);
		else if(args.if_pop("fuzzykmeans")) fuzzykmeans(args);
		else if(args.if_pop("kmeans")) kmeans(args);
		else if(args.if_pop("kmedoids")) kmedoids(args);
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

