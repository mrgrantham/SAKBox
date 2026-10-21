/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
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
#include <WClasses/WApp.h>
#include <WClasses/WCluster.h>
#include <WClasses/WDecisionTree.h>
#include <WClasses/WEnsemble.h>
#include <WClasses/WTransform.h>
#include <WClasses/WHolders.h>
#include <WClasses/WKNN.h>
#include <WClasses/WLearner.h>
#include <WClasses/WLinear.h>
#include <WClasses/WError.h>
#include <WClasses/WNaiveBayes.h>
#include <WClasses/WNaiveInstance.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WRand.h>
#include <WClasses/WTime.h>
#include <WClasses/WVec.h>
#include <WClasses/WWidgets.h>
#include <WClasses/WThread.h>
#include "gui/Gui.h"
#include "png/WImagePng.h"
#include <exception>
#include <iostream>

#ifdef WINDOWS
#include <direct.h>
#endif

using namespace WClasses;
using std::cout;
using std::cerr;

class InterpolateController : public ControllerBase
{
protected:

public:
	InterpolateController();
	~InterpolateController() override;

	void RunModal();
};


#define USE_IMAGE

class InterpolateDialog : public WWidgetDialog
{
protected:
	InterpolateController* m_pController;
	WImage* m_pImageSmall;
	WImage* m_pImageBig;
	size_t m_scale;
	WWidgetCanvas* m_pCanvasSmall;
	WWidgetCanvas* m_pCanvasBig;
	WWidgetGrid* m_pModels;
	WRand* m_pRand;
	bool m_animate;
	WMatrix* m_pTrainingFeatures;
	WMatrix* m_pTrainingLabels;
	volatile WNeuralNetLearner* m_pNNForTraining;
	WNeuralNetLearner* m_pNNCopyForVisualizing;
	WSupervisedLearner* m_pTrainedModel;
	WSpinLock m_weightsLock;
	volatile size_t m_workerMode; // 0 = exit, 1 = hybernate, 2 = yield, 3 = train
	volatile bool m_workerWorking;
	bool m_testing;
	WCoordVectorIterator* m_pCvi;
	WVec m_in;
	WVec m_out;

public:
	InterpolateDialog(InterpolateController* pController, int w, int h)
	: WWidgetDialog(w, h, 0xff90d0f0), m_in(2), m_out(3) // a=ff, r=90, g=d0, b=f0
	{
		m_pController = pController;
		m_pRand = new WRand(0);
		m_pImageSmall = new WImage();
#ifdef USE_IMAGE
		loadPng(m_pImageSmall, "input.png");
#else
		m_pImageSmall->setSize(50, 50);
		m_pImageSmall->clear(0xffff8080);
		m_pImageSmall->circleFill(25, 25, 10.0, 0xff80ff80);
#endif
		//m_pImageSmall->loadPng("input.png");
		m_scale = 12;
		m_pImageBig = new WImage();
		m_pImageBig->setSize(m_pImageSmall->width() * m_scale, m_pImageSmall->height() * m_scale);
		m_pCanvasSmall = new WWidgetCanvas(this, 5, 35, m_pImageSmall->width(), m_pImageSmall->height(), m_pImageSmall);
		m_pCanvasBig = new WWidgetCanvas(this, 310, 5, m_pImageBig->width(), m_pImageBig->height(), m_pImageBig);
		m_pModels = new WWidgetGrid(this, 2, 5, 90, 290, 500, 0xff60a0c0);
		m_pModels->setRowHeight(36);
		m_pModels->setColumnWidth(0, 30);
		m_pModels->setColumnWidth(1, 242);
		m_pTrainedModel = nullptr;
		m_pNNForTraining = nullptr;
		m_pNNCopyForVisualizing = nullptr;

		// Make the training data
		m_pTrainingFeatures = new WMatrix(m_pImageSmall->width() * m_pImageSmall->height(), 2);
		m_pTrainingLabels = new WMatrix(m_pImageSmall->width() * m_pImageSmall->height(), 3);
		size_t ranges[2];
		ranges[0] = m_pImageSmall->width();
		ranges[1] = m_pImageSmall->height();
		WCoordVectorIterator cvi(2, ranges);
		unsigned int* pPix = m_pImageSmall->pixels();
		for(size_t i = 0; true; i++)
		{
			WAssert(i < m_pTrainingFeatures->rows());
			WVec& pFeature = m_pTrainingFeatures->row(i);
			cvi.currentNormalized(pFeature.data());
			WAssert(i < m_pTrainingLabels->rows());
			WVec& pLabel = m_pTrainingLabels->row(i);
			pLabel[0] = ((double)gRed(*pPix)) / 255;
			pLabel[1] = ((double)gGreen(*pPix)) / 255;
			pLabel[2] = ((double)gBlue(*pPix)) / 255;
			if(!cvi.advance())
				break;
			pPix++;
		}

		ranges[0] = m_pImageBig->width();
		ranges[1] = m_pImageBig->height();
		m_pCvi = new WCoordVectorIterator(2, ranges);
		m_testing = false;

		// Launch the worker thread
		m_workerMode = 1; // hybernate
		m_workerWorking = true;
		/*HANDLE threadHandle = */ WThread::spawnThread(launchworkerThread, this);

		addModels();
	}

	~InterpolateDialog() override
	{
		m_workerMode = 0; // tell the worker thread to exit
		while(m_workerWorking)
			WThread::sleep(0);
		delete(m_pTrainingFeatures);
		delete(m_pTrainingLabels);
		delete(m_pImageSmall);
		delete(m_pImageBig);
		delete(m_pRand);
		delete(m_pTrainedModel);
		delete(m_pNNForTraining);
		delete(m_pNNCopyForVisualizing);
		delete(m_pCvi);
	}

	void onCheckBulletHole(WWidgetBulletHole* pBullet) override
	{
		int selectedIndex = -1;
		int n = m_pModels->rowCount();
		for(int i = 0; i < n; i++)
		{
			WWidgetBulletHole* pBH = (WWidgetBulletHole*)m_pModels->widget(0, i);
			if(pBH)
			{
				if(pBH == pBullet)
					selectedIndex = i;
				else
					pBH->setChecked(false);
			}
		}
		if(selectedIndex >= 0)
			onChoice(selectedIndex);
	}

	void addModel(const char* descr)
	{
		m_workerMode = 1; // hybernate
		int index = m_pModels->rowCount();
		m_pModels->setWidget(0, index, new WWidgetBulletHole(m_pModels, 0, 0, 20, 20));
		WWidgetTextLabel* pDescription = new WWidgetTextLabel(m_pModels, 0, 0, 242, 36, descr);
		pDescription->wrap();
		m_pModels->setWidget(1, index, pDescription);
	}

	void doSupervisedLearner(WSupervisedLearner* pModel)
	{
		m_pImageBig->clear(0x003060);
		m_workerMode = 1; // hybernate
		delete(m_pTrainedModel);
		m_pTrainedModel = pModel;
		pModel->train(*m_pTrainingFeatures, *m_pTrainingLabels);
		m_pCvi->reset();
		m_testing = true;
	}

	void doTransducer(WTransducer* pModel)
	{
		// Make the unlabeled data
		WMatrix dataUnlabeled(m_pImageBig->width() * m_pImageBig->height(), 2);
		size_t ranges[2];
		ranges[0] = m_pImageBig->width();
		ranges[1] = m_pImageBig->height();
		WCoordVectorIterator cvi(2, ranges);
		for(size_t i = 0; true; i++)
		{
			WVec& pFeature = dataUnlabeled.row(i);
			cvi.currentNormalized(pFeature.data());
			if(!cvi.advance())
				break;
		}

		// Predict labels
		std::unique_ptr<WClasses::WMatrix> hLabels = pModel->transduce(*m_pTrainingFeatures, *m_pTrainingLabels, dataUnlabeled);
		WMatrix* pLabels = hLabels.get();

		// Copy the labels into the big image
		cvi.reset();
		unsigned int* pPix = m_pImageBig->pixels();
		for(size_t i = 0; true; i++)
		{
			WVec& pRow = pLabels->row(i);
			int r = ClipChan((int)(pRow[0] * 255.0));
			int g = ClipChan((int)(pRow[1] * 255.0));
			int b = ClipChan((int)(pRow[2] * 255.0));
			*pPix = gARGB(0xff, r, g, b);
			if(!cvi.advance())
				break;
			pPix++;
		}
		m_pCanvasBig->setImage(m_pImageBig);
		delete(pModel);
	}

	// takes ownership of pNN
	void doBackProp(WNeuralNetLearner* pNN)
	{
		m_workerMode = 2; // yield
		WUniformRelation featureRel(2);
		WUniformRelation labelRel(3);
		{
			WSpinLockHolder hLock(&m_weightsLock, "replacing the network");
			delete(m_pNNForTraining);
			m_pNNForTraining = pNN;
			pNN->beginIncrementalLearning(featureRel, labelRel);
			/*			for(size_t i = 0; i < pNN->layerCount(); i++)
				pNN->getLayer(i)->setActivationFunction(new WActivationSoftPlus());
			pNN->setLearningRate(0.005);*/
			delete(m_pNNCopyForVisualizing);
			m_pNNCopyForVisualizing = new WNeuralNetLearner();
			// SAKBox: copyStructure is now copyTopology, which clones the layers
			// still bound to pNN's weights; init gives the copy its own
			m_pNNCopyForVisualizing->nn().copyTopology(pNN->nn());
			m_pNNCopyForVisualizing->nn().init(*m_pRand);
		}
		m_workerMode = 3; // train
	}

	void addModels()
	{
		addModel("1-nn"); // 0
		addModel("2-nn, equal weighting"); // 1
		addModel("2-nn, linear weighting"); // 2
		addModel("4-nn, equal weighting"); // 3
		addModel("4-nn, linear weighting"); // 4
		addModel("8-nn, equal weighting"); // 5
		addModel("8-nn, linear weighting"); // 6
		addModel("Naive Bayes"); // 7
		addModel("Decision tree with random divisions"); // 8
		addModel("MeanMargins tree"); // 9
		addModel("Bag of 30 Decision trees"); // 10
		addModel("Bag of 30 MeanMargins trees"); // 11
		addModel("Logistic regression"); // 12
		addModel("Neural Net, 2-16-16-16-16-3, tanh, online back-prop"); // 13
		addModel("Naive Instance 20"); // 14
		addModel("Mean label (a.k.a. baseline)"); // 15
	}

	void onChoice(int index)
	{
		m_workerMode = 1;
		if(index == 0)
		{
			WKNN* pModel = new WKNN();
			doSupervisedLearner(pModel);
		}
		else if(index == 1)
		{
			WKNN* pModel = new WKNN();
			pModel->setNeighborCount(2);
			pModel->setInterpolationMethod(WKNN::Mean);
			doSupervisedLearner(pModel);
		}
		else if(index == 2)
		{
			WKNN* pModel = new WKNN();
			pModel->setNeighborCount(2);
			pModel->setInterpolationMethod(WKNN::Linear);
			doSupervisedLearner(pModel);
		}
		else if(index == 3)
		{
			WKNN* pModel = new WKNN();
			pModel->setNeighborCount(4);
			pModel->setInterpolationMethod(WKNN::Mean);
			doSupervisedLearner(pModel);
		}
		else if(index == 4)
		{
			WKNN* pModel = new WKNN();
			pModel->setNeighborCount(4);
			pModel->setInterpolationMethod(WKNN::Linear);
			doSupervisedLearner(pModel);
		}
		else if(index == 5)
		{
			WKNN* pModel = new WKNN();
			pModel->setNeighborCount(8);
			pModel->setInterpolationMethod(WKNN::Mean);
			doSupervisedLearner(pModel);
		}
		else if(index == 6)
		{
			WKNN* pModel = new WKNN();
			pModel->setNeighborCount(8);
			pModel->setInterpolationMethod(WKNN::Linear);
			doSupervisedLearner(pModel);
		}
		else if(index == 7)
		{
			WSupervisedLearner* pModel = new WAutoFilter(new WNaiveBayes(), true);
			doSupervisedLearner(pModel);
		}
		else if(index == 8)
		{
			WDecisionTree* pModel = new WDecisionTree();
			pModel->useRandomDivisions(1);
			doSupervisedLearner(pModel);
		}
		else if(index == 9)
		{
			WMeanMarginsTree* pModel = new WMeanMarginsTree();
			doSupervisedLearner(pModel);
		}
		else if(index == 10)
		{
			WBag* pBag = new WBag();
			for(int i = 0; i < 30; i++)
			{
				WDecisionTree* pTree = new WDecisionTree();
				pTree->rand().setSeed(i);
				pTree->useRandomDivisions();
				pBag->addLearner(pTree);
			}
			doSupervisedLearner(pBag);
		}
		else if(index == 11)
		{
			WBag* pBag = new WBag();
			for(int i = 0; i < 30; i++)
			{
				WMeanMarginsTree* pTree = new WMeanMarginsTree();
				pTree->rand().setSeed(i);
				pBag->addLearner(pTree);
			}
			doSupervisedLearner(pBag);
		}
		else if(index == 12)
		{
			WNeuralNetLearner* pModel = new WNeuralNetLearner();
			pModel->nn().add(new WBlockLinear(2, 3));
			pModel->nn().add(new WBlockTanh(3));
			doSupervisedLearner(pModel);
		}
		else if(index == 13)
		{
			WNeuralNetLearner* pNN = new WNeuralNetLearner();
			pNN->nn().add(new WBlockLinear(2, 16));
			pNN->nn().add(new WBlockTanh(16));
			pNN->nn().add(new WBlockLinear(16, 16));
			pNN->nn().add(new WBlockTanh(16));
			pNN->nn().add(new WBlockLinear(16, 16));
			pNN->nn().add(new WBlockTanh(16));
			pNN->nn().add(new WBlockLinear(16, 16));
			pNN->nn().add(new WBlockTanh(16));
			pNN->nn().add(new WBlockLinear(16, 3));
			pNN->nn().add(new WBlockTanh(3));

			doBackProp(pNN);
		}
		else if(index == 14)
		{
			WNaiveInstance* pModel = new WNaiveInstance();
			pModel->setNeighbors(20);
			doSupervisedLearner(pModel);
		}
		else if(index == 15)
		{
			WBaselineLearner* pModel = new WBaselineLearner();
			doSupervisedLearner(pModel);
		}
	}

	void workerThread()
	{
		WAssert(m_workerWorking);
		while(m_workerMode > 0)
		{
			if(m_workerMode == 2) // yield
				WThread::sleep(0);
			else if(m_workerMode == 3) // train
			{
				WSpinLockHolder hLock(&m_weightsLock, "training the network");
				WNeuralNetLearner* pNN = (WNeuralNetLearner*)m_pNNForTraining;
				WNeuralNetOptimizer& opt = pNN->optimizer();
				for(size_t i = 0; i < 100; i++)
				{
					size_t r = (size_t)m_pRand->next(m_pTrainingFeatures->rows());
					opt.optimizeIncremental(m_pTrainingFeatures->row(r), m_pTrainingLabels->row(r));
				}
			}
			else
				WThread::sleep(200); // hybernate
		}
		m_workerWorking = false;
	}

	static unsigned int launchworkerThread(void* pThis)
	{
		((InterpolateDialog*)pThis)->workerThread();
		return 0;
	}

	void Iterate()
	{
		if(m_workerMode == 3) // train
		{
			// Copy the weights
			m_workerMode = 2; // yield
			{
				WSpinLockHolder hLock(&m_weightsLock, "updating display");
				WNeuralNetLearner* pNN = (WNeuralNetLearner*)m_pNNForTraining;
				// SAKBox: the weights are now kept by the network, not its optimizer
				m_pNNCopyForVisualizing->nn().weights.copy(pNN->nn().weights);
			}
			m_workerMode = 3; // train

			// Display the image
			m_pCvi->reset();
			unsigned int* pPix = m_pImageBig->pixels();
			while(true)
			{
				m_pCvi->currentNormalized(m_in.data());
				m_pNNCopyForVisualizing->predict(m_in, m_out);
				int r = ClipChan((int)(m_out[0] * 255.0));
				int g = ClipChan((int)(m_out[1] * 255.0));
				int b = ClipChan((int)(m_out[2] * 255.0));
				*pPix = gARGB(0xff, r, g, b);
				if(!m_pCvi->advance())
					break;
				pPix++;
			}
			m_pCanvasBig->setImage(m_pImageBig);

			// Slow down the display thread a little bit
			WThread::sleep(100);
		}
		else if(m_testing)
		{
			double startTime = WTime::seconds();

			// Display the image
			while(true)
			{
				for(size_t i = 0; i < 100; i++)
				{
					m_pCvi->currentNormalized(m_in.data());
					m_pTrainedModel->predict(m_in, m_out);
					int r = ClipChan((int)(m_out[0] * 255.0));
					int g = ClipChan((int)(m_out[1] * 255.0));
					int b = ClipChan((int)(m_out[2] * 255.0));
					m_pImageBig->setPixel(m_pCvi->current()[0], m_pCvi->current()[1], gARGB(0xff, r, g, b));
					if(!m_pCvi->advance())
					{
						m_testing = false;
						break;
					}
				}
				if(WTime::seconds() - startTime >= 0.25)
					break;
			}
			m_pCanvasBig->setImage(m_pImageBig);
		}
	}
};


// -------------------------------------------------------------------------------

class InterpolateView : public ViewBase
{
protected:
	InterpolateDialog* m_pDialog;

public:
	InterpolateView(InterpolateController* pController)
	: ViewBase("Interpolate")
	{
		m_pDialog = new InterpolateDialog(pController, m_screenRect.w, m_screenRect.h);
	}

	~InterpolateView() override
	{
		delete(m_pDialog);
	}

	void onChar(char c) override
	{
		m_pDialog->handleChar(c);
	}

	void onMouseDown(int nButton, int x, int y) override
	{
		m_pDialog->pressButton(nButton, x - m_screenRect.x, y - m_screenRect.y);
	}

	void onMouseUp(int nButton, int x, int y) override
	{
		m_pDialog->releaseButton(nButton);
	}

	bool onMousePos(int x, int y) override
	{
		return m_pDialog->handleMousePos(x - m_screenRect.x, y - m_screenRect.y);
	}

protected:
	void draw(int* pixels, int pitch) override
	{
		m_pDialog->Iterate();

		// Clear the screen
		memset(pixels, 0, (size_t)pitch * m_screenRect.h);

		// Draw the dialog
		Uint32 format; // SDL_PixelFormatEnum
		SDL_QueryTexture(sdlTexture, &format, nullptr, nullptr, nullptr);
		blitImage(pixels, pitch, format, m_screenRect.x, m_screenRect.y, m_pDialog->image());
	}
};


// -------------------------------------------------------------------------------


InterpolateController::InterpolateController()
: ControllerBase()
{
	m_pView = new InterpolateView(this);
}

InterpolateController::~InterpolateController()
{
	delete(m_pView);
}

void InterpolateController::RunModal()
{
	double timeOld = WTime::seconds();
	double time;
	m_pView->update();
	while(m_bKeepRunning)
	{
		time = WTime::seconds();
		handleEvents(time - timeOld);
		m_pView->update();
		WThread::sleep(50); // slow things down
		timeOld = time;
	}
}





int main(int argc, char* argv[])
{
	// SAKBox: input.png is in the demo's bin folder, which `bazel run` puts in
	// its runfiles under this path
#ifdef WINDOWS
	if(_chdir("projects/waffles/demos/interpolate/bin") != 0)
#else
	if(chdir("projects/waffles/demos/interpolate/bin") != 0)
#endif
	{
	}
	int nRet = 0;
	try
	{
		InterpolateController c;
		c.RunModal();
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}

