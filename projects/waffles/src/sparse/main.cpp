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

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include "WClasses/WApp.h"
#include "WClasses/WMatrix.h"
#include "WClasses/WCluster.h"
#include "WClasses/WDistance.h"
#include "WClasses/WDistribution.h"
#include "WClasses/WFile.h"
#include "WClasses/WHolders.h"
#include "WClasses/WImage.h"
#include "WClasses/WKNN.h"
#include "WClasses/WLinear.h"
#include "WClasses/WError.h"
#include "WClasses/WManifold.h"
#include "WClasses/WNaiveBayes.h"
#include "WClasses/WNaiveInstance.h"
#include "WClasses/WNeuralNet.h"
#include "WClasses/WRand.h"
#include "WClasses/WSparseMatrix.h"
#include "WClasses/WHtml.h"
#include "WClasses/WText.h"
#include "WClasses/WDirList.h"
#include "WClasses/WTime.h"
#include "WClasses/WTransform.h"
#include "WClasses/WDom.h"
#include "WClasses/WVec.h"
#include "WClasses/usage.h"
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

using namespace WClasses;
using std::cout;
using std::cerr;
using std::string;
using std::vector;
using std::set;

void loadData(WMatrix& data, const char* szFilename)
{
	// Load the dataset by extension
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
}

WTransducer* InstantiateAlgorithm(WArgReader& args);

WBaselineLearner* InstantiateBaseline(WArgReader& args)
{
	WBaselineLearner* pModel = new WBaselineLearner();
	return pModel;
}

WKNN* InstantiateKNN(WArgReader& args)
{
	if(args.size() < 1)
		throw Ex("The number of neighbors must be specified for knn");
	int neighborCount = args.pop_uint();
	WKNN* pModel = new WKNN();
	pModel->setNeighborCount(neighborCount);
	while(args.next_is_flag())
	{
		if(args.if_pop("-equalweight"))
			pModel->setInterpolationMethod(WKNN::Mean);
		else if(args.if_pop("-scalefeatures"))
			pModel->setOptimizeScaleFactors(true);
		else if(args.if_pop("-cosine"))
			pModel->setMetric(new WCosineSimilarity(), true);
		else if(args.if_pop("-pearson"))
			pModel->setMetric(new WPearsonCorrelation(), true);
		else
			throw Ex("Invalid knn option: ", args.peek());
	}
	return pModel;
}

WLinearRegressor* InstantiateLinearRegressor(WArgReader& args)
{
	WLinearRegressor* pModel = new WLinearRegressor();
	return pModel;
}

WNaiveBayes* InstantiateNaiveBayes(WArgReader& args)
{
	WNaiveBayes* pModel = new WNaiveBayes();
	while(args.next_is_flag())
	{
		if(args.if_pop("-ess"))
			pModel->setEquivalentSampleSize(args.pop_double());
		else
			throw Ex("Invalid naivebayes option: ", args.peek());
	}
	return pModel;
}

WNaiveInstance* InstantiateNaiveInstance(WArgReader& args)
{
	WNaiveInstance* pModel = new WNaiveInstance();
	while(args.next_is_flag())
	{
		if(args.if_pop("-neighbors"))
			pModel->setNeighbors(args.pop_uint());
		else
			throw Ex("Invalid neighbortransducer option: ", args.peek());
	}
	return pModel;
}

void showInstantiateAlgorithmError(const char* szMessage, WArgReader& args)
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

WTransducer* InstantiateAlgorithm(WArgReader& args)
{
	int argPos = args.get_pos();
	if(args.size() < 1)
		throw Ex("No algorithm specified.");
	try
	{
		if(args.if_pop("baseline"))
			return InstantiateBaseline(args);
		else if(args.if_pop("knn"))
			return InstantiateKNN(args);
		else if(args.if_pop("linear"))
			return InstantiateLinearRegressor(args);
		else if(args.if_pop("naivebayes"))
			return InstantiateNaiveBayes(args);
		throw Ex("Unrecognized algorithm name: ", args.peek());
	}
	catch(const std::exception& e)
	{
		args.set_pos(argPos);
		if(strcmp(e.what(), "nevermind") != 0) // if an error message was not already displayed...
			showInstantiateAlgorithmError(e.what(), args);
		throw Ex("nevermind"); // this means "don't display another error message"
	}
	return nullptr;
}

void multiplyDense(WArgReader& args)
{
	// Load the sparse matrix
	if(args.size() < 1)
		throw Ex("No dataset specified.");
	WSparseMatrix* pA;
	std::unique_ptr<WSparseMatrix> hA(nullptr);
	{
		WDom doc;
		doc.loadJson(args.pop_string());
		pA = new WSparseMatrix(doc.root());
		hA.reset(pA);
	}

	// Load the dense matrix
	WMatrix b;
	b.loadArff(args.pop_string());

	// Parse options
	bool transpose = false;
	while(args.next_is_flag())
	{
		if(args.if_pop("-transpose"))
			transpose = true;
		else
			throw Ex("Invalid option: ", args.peek());
	}

	WMatrix* pResult = pA->multiply(&b, transpose);
	std::unique_ptr<WMatrix> hResult(pResult);
	pResult->print(cout);
}

void train(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid trainsparse option: ", args.peek());
	}

	// Load the sparse features
	if(args.size() < 1)
		throw Ex("Expected a filename of a sparse matrix.");
	WSparseMatrix* pSparseFeatures;
	std::unique_ptr<WSparseMatrix> hSparseFeatures(nullptr);
	{
		WDom doc;
		doc.loadJson(args.pop_string());
		pSparseFeatures = new WSparseMatrix(doc.root());
		hSparseFeatures.reset(pSparseFeatures);
	}

	// Load the dense labels
	WMatrix labels;
	labels.loadArff(args.pop_string());

	// Instantiate the modeler
	WTransducer* pSupLearner = InstantiateAlgorithm(args);
	std::unique_ptr<WTransducer> hModel(pSupLearner);
	if(args.size() > 0)
		throw Ex("Superfluous argument: ", args.peek());
	if(!pSupLearner->canTrainIncrementally())
		throw Ex("This algorithm cannot be trained with a sparse matrix. Only incremental learners (such as naivebayes, knn, and neuralnet) support this functionality.");
	pSupLearner->rand().setSeed(seed);
	WIncrementalLearner* pModel = (WIncrementalLearner*)pSupLearner;

	// Train the modeler
	pModel->trainSparse(*pSparseFeatures, labels);

	// Output the trained model
	WDom doc;
	WDomNode* pRoot = pModel->serialize(&doc);
	doc.setRoot(pRoot);
	doc.writeJson(cout);
}

void predict(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid predictsparse option: ", args.peek());
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

	// Load the sparse features
	if(args.size() < 1)
		throw Ex("No dataset specified.");
	WSparseMatrix* pData;
	std::unique_ptr<WSparseMatrix> hData(nullptr);
	{
		WDom doc2;
		doc2.loadJson(args.pop_string());
		pData = new WSparseMatrix(doc2.root());
		hData.reset(pData);
	}

	// Predict labels
	WMatrix labels(pData->rows(), pModeler->relLabels().size());
	WVec pFullRow(pData->cols());
	for(unsigned int i = 0; i < pData->rows(); i++)
	{
		pData->fullRow(pFullRow, i);
		pModeler->predict(pFullRow, labels[i]);
	}
	labels.print(cout);
}

void test(WArgReader& args)
{
	// Parse options
	unsigned int seed = getpid() * (unsigned int)time(nullptr);
	while(args.next_is_flag())
	{
		if(args.if_pop("-seed"))
			seed = args.pop_uint();
		else
			throw Ex("Invalid predictsparse option: ", args.peek());
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

	// Load the sparse features
	if(args.size() < 1)
		throw Ex("No dataset specified.");
	WSparseMatrix* pData;
	std::unique_ptr<WSparseMatrix> hData(nullptr);
	{
		WDom doc2;
		doc2.loadJson(args.pop_string());
		pData = new WSparseMatrix(doc2.root());
		hData.reset(pData);
	}

	// Load the dense labels
	WMatrix labels;
	labels.loadArff(args.pop_string());
	if(!labels.relation().isCompatible(pModeler->relLabels()))
		throw Ex("The data is not compatible with the data used to trainn the model. (The meta-data is different.)");

	// Test
	WVec prediction(labels.cols());
	WVec pFullRow(pData->cols());
	WTEMPBUF(double, results, labels.cols());
	WVecWrapper vw2(results, labels.cols());
	vw2.fill(0.0);
	for(size_t i = 0; i < pData->rows(); i++)
	{
		pData->fullRow(pFullRow, i);
		pModeler->predict(pFullRow, prediction);
		WVec& pTarget = labels.row(i);
		for(size_t j = 0; j < labels.cols(); j++)
		{
			if(labels.relation().valueCount(j) == 0)
			{
				double d = pTarget[j] - prediction[j];
				results[j] += (d * d);
			}
			else
			{
				if((int)prediction[j] == (int)pTarget[j])
					results[j]++;
			}
		}
	}
	WVecWrapper vw(results, labels.cols());
	vw *= (1.0 / pData->rows());
	vw.print(cout);
}

void transpose(WArgReader& args)
{
	// Load the sparse matrix
	if(args.size() < 1)
		throw Ex("No dataset specified.");
	WSparseMatrix* pA;
	std::unique_ptr<WSparseMatrix> hA(nullptr);
	{
		WDom doc;
		doc.loadJson(args.pop_string());
		pA = new WSparseMatrix(doc.root());
		hA.reset(pA);
	}

	// Transpose it
	WSparseMatrix* pB = pA->transpose();
	std::unique_ptr<WSparseMatrix> hB(pB);

	// Print it
	{
		WDom doc;
		doc.setRoot(pB->serialize(&doc));
		doc.writeJson(cout);
	}
}

void addWordsToVocabFromTextFile(WVocabulary* pVocab, const char* szFilename)
{
	size_t len;
	char* pFile = WFile::loadFile(szFilename, &len);
	std::unique_ptr<char[]> hFile(pFile);
	pVocab->newDoc();
	pVocab->addWordsFromTextBlock(pFile, len);
}

void makeTextFileVector(WSparseMatrix* pFeatures, WMatrix* pLabels, int clss, size_t row, WVocabulary* pVocab, const char* szFilename, bool binary)
{
	size_t len;
	char* pFile = WFile::loadFile(szFilename, &len);
	std::unique_ptr<char[]> hFile(pFile);
	WWordIterator it(pFile, len);
	const char* pWord;
	size_t wordLen;
	while(true)
	{
		if(!it.next(&pWord, &wordLen))
			break;
		size_t col = pVocab->wordIndex(pWord, wordLen);
		if(col != INVALID_INDEX)
		{
			if(binary)
				pFeatures->set(row, col, 1.0);
			else
				pFeatures->set(row, col, pFeatures->get(row, col) + pVocab->weight(col));
		}
	}
	if(pLabels)
		pLabels->row(row)[0] = (double)clss;
}

void docsToSparseMatrix(WArgReader& args)
{
	// Parse options
	bool useStemmer = true;
	bool binary = false;
	string featuresFilename = "features.sparse";
	string labelsFilename = "labels.arff";
	string vocabFile = "";
	while(args.next_is_flag())
	{
		if(args.if_pop("-nostem"))
			useStemmer = false;
		else if(args.if_pop("-binary"))
			binary = true;
		else if(args.if_pop("-out"))
		{
			featuresFilename = args.pop_string();
			labelsFilename = args.pop_string();
		}
		else if(args.if_pop("-vocabfile"))
			vocabFile = args.pop_string();
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Parse the vocabulary
	WVocabulary vocab(useStemmer);
	vocab.addTypicalStopWords();
	vector<string> folders;
	while(args.size() > 0)
	{
		const char* szFolder = args.pop_string();
		folders.push_back(szFolder);
		char cwd[300];
		if(!getcwd(cwd, 300))
			throw Ex("Failed to get cwd");
		if(chdir(szFolder) != 0)
			throw Ex("Failed to change directory to: ", szFolder, ", from: ", cwd);
		{
			vector<string> files;
			WFile::fileList(files);
			for(vector<string>::iterator it = files.begin(); it != files.end(); it++)
			{
				const char* filename = it->c_str();
				PathData pd;
				WFile::parsePath(filename, &pd);
				if(_stricmp(filename + pd.extStart, ".txt") == 0)
					addWordsToVocabFromTextFile(&vocab, filename);
				else
					printf("Skipping file: %s. (Only .txt files are supported.)\n", filename);
			}
		}
		if(chdir(cwd) != 0)
			throw Ex("failed to change dir");
	}
	if(folders.size() == 0)
		throw Ex("At least one folder name must be specified");
	printf("-----\n");

	// Make the sparse feature matrix and the label matrix
	WSparseMatrix sparseFeatures(vocab.docCount(), vocab.wordCount());
	WMatrix* pLabels = nullptr;
	if(folders.size() > 1)
	{
		vector<size_t> classes;
		classes.push_back(folders.size());
		pLabels = new WMatrix(classes);
		pLabels->newRows(vocab.docCount());
	}
	std::unique_ptr<WMatrix> hLabels(pLabels);
	size_t row = 0;
	for(int clss = 0; clss < (int)folders.size(); clss++)
	{
		const char* szFolder = folders[clss].c_str();
		char cwd[300];
		if(!getcwd(cwd, 300))
			throw Ex("Failed to get cwd");
		if(chdir(szFolder) != 0)
			throw Ex("Failed to change directory to: ", szFolder, ", from: ", cwd);
		{
			vector<string> files;
			WFile::fileList(files);
			for(vector<string>::iterator it = files.begin(); it != files.end(); it++)
			{
				const char* filename = it->c_str();
				PathData pd;
				WFile::parsePath(filename, &pd);
				if(_stricmp(filename + pd.extStart, ".txt") == 0)
				{
					printf("%d) %s\n", (int)row, filename);
					makeTextFileVector(&sparseFeatures, pLabels, clss, row++, &vocab, filename, binary);
				}
			}
		}
		if(chdir(cwd) != 0)
			throw Ex("Failed to change dir");
	}

	// Save the files
	if(vocabFile.length() > 0)
	{
		FILE* pFile = fopen(vocabFile.c_str(), "w");
		FileHolder hFile(pFile);
		for(size_t i = 0; i < vocab.wordCount(); i++)
		{
			const char* szWord = vocab.stats(i).m_szWord;
			fprintf(pFile, "%s\n", szWord);
		}
	}
	WDom doc;
	doc.setRoot(sparseFeatures.serialize(&doc));
	doc.saveJson(featuresFilename.c_str());
	if(pLabels)
		pLabels->saveArff(labelsFilename.c_str());
}

void shuffle(WArgReader& args)
{
	// Load
	WDom doc;
	doc.loadJson(args.pop_string());
	WSparseMatrix* pData = new WSparseMatrix(doc.root());
	std::unique_ptr<WSparseMatrix> hData(pData);

	// Parse options
	unsigned int nSeed = getpid() * (unsigned int)time(nullptr);
	string labelsIn;
	string labelsOut;
	while(args.size() > 0)
	{
		if(args.if_pop("-seed"))
			nSeed = args.pop_uint();
		else if(args.if_pop("-labels"))
		{
			labelsIn = args.pop_string();
			labelsOut = args.pop_string();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Shuffle and print
	WRand prng(nSeed);
	WMatrix* pLabels = nullptr;
	std::unique_ptr<WMatrix> hLabels(nullptr);
	if(labelsIn.length() > 0)
	{
		pLabels = new WMatrix();
		hLabels.reset(pLabels);
		loadData(*pLabels, labelsIn.c_str());
	}
	pData->shuffle(&prng, pLabels);
	WDom doc2;
	doc2.setRoot(pData->serialize(&doc2));
	doc2.writeJson(cout);
	if(pLabels)
		pLabels->saveArff(labelsOut.c_str());
}

void split(WArgReader& args)
{
	// Load
	WDom doc;
	doc.loadJson(args.pop_string());
	WSparseMatrix* pData = new WSparseMatrix(doc.root());
	std::unique_ptr<WSparseMatrix> hData(pData);
	size_t pats1 = args.pop_uint();
	size_t pats2 = pData->rows() - pats1;
	if(pats2 >= pData->rows())
		throw Ex("out of range. The data only has ", to_str(pData->rows()), " rows.");
	const char* szFilename1 = args.pop_string();
	const char* szFilename2 = args.pop_string();

	// Split
	WSparseMatrix* pPart1 = pData->subMatrix(0, 0, pData->cols(), pats1);
	std::unique_ptr<WSparseMatrix> hPart1(pPart1);
	WSparseMatrix* pPart2 = pData->subMatrix(0, pats1, pData->cols(), pats2);
	std::unique_ptr<WSparseMatrix> hPart2(pPart2);
	doc.setRoot(pPart1->serialize(&doc));
	doc.saveJson(szFilename1);
	doc.setRoot(pPart2->serialize(&doc));
	doc.saveJson(szFilename2);
}

void splitFold(WArgReader& args)
{
	// Load
	WDom doc;
	doc.loadJson(args.pop_string());
	WSparseMatrix* pData = new WSparseMatrix(doc.root());
	std::unique_ptr<WSparseMatrix> hData(pData);
	size_t fold = args.pop_uint();
	size_t folds = args.pop_uint();
	if(fold >= folds)
		throw Ex("fold index out of range. It must be less than the total number of folds.");

	// Options
	string filenameTrain = "train.sparse";
	string filenameTest = "test.sparse";
	while(args.size() > 0)
	{
		if(args.if_pop("-out"))
		{
			filenameTrain = args.pop_string();
			filenameTest = args.pop_string();
		}
		else
			throw Ex("Invalid option: ", args.peek());
	}

	// Copy relevant portions of the data
	WSparseMatrix train(0, pData->cols());
	WSparseMatrix test(0, pData->cols());
	size_t begin = pData->rows() * fold / folds;
	size_t end = pData->rows() * (fold + 1) / folds;
	for(size_t i = 0; i < begin; i++)
		train.copyRow(pData->row(i));
	for(size_t i = begin; i < end; i++)
		test.copyRow(pData->row(i));
	for(size_t i = end; i < pData->rows(); i++)
		train.copyRow(pData->row(i));
	doc.setRoot(train.serialize(&doc));
	doc.saveJson(filenameTrain.c_str());
	doc.setRoot(test.serialize(&doc));
	doc.saveJson(filenameTest.c_str());
}

void ShowUsage(const char* appName)
{
	cout << "Full Usage Information\n";
	cout << "[Square brackets] are used to indicate required arguments.\n";
	cout << "<Angled brackets> are used to indicate optional arguments.\n";
	cout << "\n";
	UsageNode* pUsageTree = makeSparseUsageTree();
	std::unique_ptr<UsageNode> hUsageTree(pUsageTree);
	pUsageTree->print(cout, 0, 3, 76, 1000, true);
	UsageNode* pUsageTree2 = makeAlgorithmUsageTree();
	std::unique_ptr<UsageNode> hUsageTree2(pUsageTree2);
	pUsageTree2->print(cout, 0, 3, 76, 1000, true);
	cout.flush();
}

void showError(WArgReader& args, const char* szAppName, const char* szMessage)
{
	cerr << "_________________________________\n";
	cerr << szMessage << "\n\n";
	args.set_pos(1);
	const char* szCommand = args.peek();
	UsageNode* pUsageTree = makeSparseUsageTree();
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

int main(int argc, char* argv[])
{
#ifdef _DEBUG
	WApp::enableFloatingPointExceptions();
#endif
	int nRet = 0;
	PathData pd;
	WFile::parsePath(argv[0], &pd);
	const char* appName = argv[0] + pd.fileStart;
	WArgReader args(argc, argv);
	try
	{
		args.pop_string(); // advance past the name of this app
		if(args.size() >= 1)
		{
			if(args.if_pop("usage"))
				ShowUsage(appName);
			else if(args.if_pop("docstosparsematrix")) docsToSparseMatrix(args);
			else if(args.if_pop("multiplydense")) multiplyDense(args);
			else if(args.if_pop("predict")) predict(args);
			else if(args.if_pop("shuffle")) shuffle(args);
			else if(args.if_pop("split")) split(args);
			else if(args.if_pop("splitfold")) splitFold(args);
			else if(args.if_pop("test")) test(args);
			else if(args.if_pop("train")) train(args);
			else if(args.if_pop("transpose")) transpose(args);
			else
			{
				nRet = 1;
				string s = args.peek();
				s += " is not a recognized command.";
				showError(args, appName, s.c_str());
			}
		}
		else
		{
			nRet = 1;
			showError(args, appName, "Brief Usage Information:");
		}
	}
	catch(const std::exception& e)
	{
		nRet = 1;
		if(strcmp(e.what(), "nevermind") != 0) // if an error message was not already displayed...
			showError(args, appName, e.what());
	}
	return nRet;
}
