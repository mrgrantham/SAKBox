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
#include "WApp.h"
#include "WCluster.h"
#include "WMatrix.h"
#include "WDistance.h"
#include "WFile.h"
#include "WError.h"
#include "WHolders.h"
#include "WNeuralNet.h"
#include "WRand.h"
#include "WRecommender.h"
#include "WSparseMatrix.h"
#include "WDom.h"
#include "usage.h"
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

using std::cout;
using std::cerr;
using std::string;
using std::vector;
using std::set;

namespace WClasses
{

class WRecommenderLib
{
public:
	static size_t getAttrVal(const char* szString, size_t attrCount);

	static void parseAttributeList(vector<size_t>& list, WArgReader& args, size_t attrCount);

	static void loadData(WMatrix& data, const char* szFilename);

	static WSparseMatrix* loadSparseData(const char* szFilename);

	static WBaselineRecommender* InstantiateBaselineRecommender(WArgReader& args);

	static WBagOfRecommenders* InstantiateBagOfRecommenders(WArgReader& args);

	static WInstanceRecommender* InstantiateInstanceRecommender(WArgReader& args);

	static WDenseClusterRecommender* InstantiateDenseClusterRecommender(WArgReader& args);

	static WSparseClusterRecommender* InstantiateSparseClusterRecommender(WArgReader& args);

	static WMatrixFactorization* InstantiateMatrixFactorization(WArgReader& args);

	//	static WNonlinearPCA* InstantiateNonlinearPCA(WArgReader& args);

	//	static WHybridNonlinearPCA* InstantiateHybridNonlinearPCA(WArgReader& args);

	static WContentBasedFilter* InstantiateContentBasedFilter(WArgReader& args);

	static WContentBoostedCF* InstantiateContentBoostedCF(WArgReader& args);

	static void showInstantiateAlgorithmError(const char* szMessage, WArgReader& args);

	static WCollaborativeFilter* InstantiateAlgorithm(WArgReader& args);

	static void crossValidate(WArgReader& args);

	static void precisionRecall(WArgReader& args);

	static void ROC(WArgReader& args);

	static void transacc(WArgReader& args);

	static void fillMissingValues(WArgReader& args);

	static void ShowUsage(const char* appName);

	static void showError(WArgReader& args, const char* szAppName, const char* szMessage);
};

} // namespace WClasses
