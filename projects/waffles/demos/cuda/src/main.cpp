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

#include <exception>
#include <iostream>
#define WCUDA
#include <WClasses/WApp.h>
#include <WClasses/WError.h>
#include <WClasses/WVec.h>
#include <WClasses/WMatrix.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WTime.h>
#include <WClasses/WCudaMatrix.h>

using namespace WClasses;
using std::cerr;
using std::cout;

void test_upload_download(WCudaEngine& e)
{
	WVec v0(3);
	v0[0] = 0.1234;
	v0[1] = 0.2345;
	v0[2] = 0.3456;
	WCudaVector cv0;
	cv0.upload(v0);
	WVec v1;
	cv0.download(v1);
	for(size_t i = 0; i < 3; i++)
	{
		if(std::abs(v1[i] - v0[i]) > 1e-12)
			throw Ex("failed");
	}
}

void test_copy(WCudaEngine& e)
{
	WVec v0(3);
	v0[0] = 0.1234;
	v0[1] = 0.2345;
	v0[2] = 0.3456;
	WCudaVector cv0;
	cv0.upload(v0);
	WCudaVector cv1;
	cv1.copy(e, cv0);
	WVec v1;
	cv1.download(v1);
	for(size_t i = 0; i < 3; i++)
	{
		if(std::abs(v1[i] - v0[i]) > 1e-12)
			throw Ex("failed");
	}
}

void test_matrix_vector_multiply(WCudaEngine& e)
{
	WVec v1(2);
	v1[0] = 1.0;
	v1[1] = 2.0;
	WMatrix m(2, 3);
	m[0][0] = 1.0;
	m[0][1] = 2.0;
	m[0][2] = 3.0;
	m[1][0] = 4.0;
	m[1][1] = 5.0;
	m[1][2] = 6.0;

	WCudaVector cv1;
	cv1.upload(v1);
	WCudaMatrix cm;
	cm.upload(m);
	WCudaVector cv2;
	cm.rowVectorTimesThis(e, cv1, cv2);

	WVec v2(3);
	cv2.download(v2);
	if(std::abs(v2[0] - 9) > 1e-12)
		throw Ex("failed");
	if(std::abs(v2[1] - 12) > 1e-12)
		throw Ex("failed");
	if(std::abs(v2[2] - 15) > 1e-12)
		throw Ex("failed");
}

void test_tanh(WCudaEngine& e)
{
	WVec v2(3);
	v2[0] = 1.0;
	v2[1] = 2.0;
	v2[2] = 3.0;
	WCudaVector cv2;
	cv2.upload(v2);
	cv2.activateTanh(e);
	cv2.download(v2);
	if(std::abs(v2[0] - 0.76159415595576) > 1e-12)
		throw Ex("failed");
	if(std::abs(v2[1] - 0.96402758007582) > 1e-12)
		throw Ex("failed");
	if(std::abs(v2[2] - 0.99505475368673) > 1e-12)
		throw Ex("failed");
}

double train(WCudaEngine& e, WNeuralNet& nn, WMatrix& feat, WMatrix& lab, WRand& rand)
{
	nn.init(feat.cols(), lab.cols(), rand);
	WContextNeuralNet* pCtx = nn.newContext(rand);
	WVec gradient(nn.weightCount());
	gradient.fill(0.0);
	double learningRate = 0.03;
	double momentum = 0.0;
	double timeBef = WTime::seconds();
	for(size_t i = 0; i < feat.rows(); i++)
	{
		WVec& pred = pCtx->forwardProp(feat[i]);
		pCtx->blame().copy(lab[i]);
		pCtx->blame() -= pred;
		pCtx->backProp();
		gradient *= momentum;
		pCtx->updateGradient(feat[i], gradient);
		nn.step(learningRate, gradient);
	}
	double timeAft = WTime::seconds();
	return timeAft - timeBef;
}

double train_gpu(WCudaEngine& e, WNeuralNet& nn, WMatrix& feat, WMatrix& lab, WRand& rand)
{
	nn.init(feat.cols(), lab.cols(), rand);
	nn.uploadCuda();
	WContextNeuralNet* pCtx = nn.newContext(rand, e);
	WCudaMatrix featGPU;
	featGPU.upload(feat);
	WCudaMatrix labGPU;
	labGPU.upload(lab);

	WCudaVector gradient(nn.weightCount());
	gradient.fill(e, 0.0);
	double learningRate = 0.03;
	double momentum = 0.0;
	double timeBef = WTime::seconds();
	for(size_t i = 0; i < feat.rows(); i++)
	{
		WCudaVector& pred = pCtx->forwardPropCuda(featGPU[i]);
		pCtx->blameCuda().copy(e, labGPU[i]);
		pCtx->blameCuda().add(e, pred, -1.0);
		WCudaVector f(featGPU[i]);
		pCtx->backPropCuda();
		gradient.scale(e, momentum);
		pCtx->updateGradientCuda(f, gradient);
		nn.stepCuda(*pCtx, learningRate, gradient);
	}
	double timeAft = WTime::seconds();
	nn.downloadCuda();
	return timeAft - timeBef;
}

void test_GBlockLinear(WCudaEngine& e)
{
	WRand rand(0);
	WNeuralNet nn;
	WBlockLinear* bl = new WBlockLinear(2, 3);
	nn.add(bl);
	nn.init(3, 2, rand);
	WContextNeuralNet* pCtx = nn.newContext(rand, e);
	WVec v0(3);
	v0[0] = 0.1234;
	v0[1] = 0.2345;
	v0[2] = 0.3456;
	WVec v1(2);
	bl->forwardProp(*pCtx, v0, v1);

	WCudaVector cv0;
	cv0.upload(v0);
	WCudaVector cv1;
	cv1.resize(2);
	bl->uploadCuda();
	bl->forwardPropCuda(*pCtx, cv0, cv1);
	WVec v2;
	cv1.download(v2);
	if(std::abs(v1[0] - v2[0]) > 1e-10)
		throw Ex("failed");
	if(std::abs(v1[1] - v2[1]) > 1e-10)
		throw Ex("failed");
}

void test_GCudaLayer(WCudaEngine& e)
{
	WUniformRelation rel(3);
	size_t width = 1000;
	//	size_t epochs = 2000;

	WRand rand(0);
	WMatrix feat(100, 3);
	for(size_t i = 0; i < feat.rows(); i++)
		feat[i].fillNormal(rand, 0.3);

	WMatrix lab(100, 2);
	for(size_t i = 0; i < lab.rows(); i++)
	{
		WVec& f = feat[i];
		WVec& l = lab[i];
		l[0] = (f[0] + f[1]) * f[2];
		l[1] = (f[1] + f[2]) * f[0];
	}

	WNeuralNet nn;
	nn.add(new WBlockLinear(width), new WBlockTanh());
	nn.add(new WBlockLinear(width), new WBlockTanh());
	nn.add(new WBlockLinear(width), new WBlockTanh());
	nn.add(new WBlockLinear(width), new WBlockTanh());
	nn.add(new WBlockLinear(3), new WBlockTanh());

	for(int i = 0; i < 4; i++)
	{
		double elapsed_cpu = train(e, nn, feat, lab, rand);
		cout << "CPU: " << to_str(elapsed_cpu) << "\n";

		double elapsed_gpu = train_gpu(e, nn, feat, lab, rand);
		cout << "GPU: " << to_str(elapsed_gpu) << "\n";
	}
}

/*
void test_convolutional(WCudaEngine& e)
{
	cout << "Testing convolutional...\n";
	size_t epochs = 1000;

	// Make some random training data
	WRand r(0);
	WMatrix feat(10, 9);
	feat.fillNormal(r, 0.1);
	WMatrix lab(10, 1);
	lab.fillNormal(r, 0.1);

	// Make a regular CNN
	WNeuralNet nn1;
	nn1.addLayer(new WLayerConvolutional2D(3, 3, 1, 2, 2, 1));
	nn1.addLayer(new WLayerClassic(4, 1));
	nn1.beginIncrementalLearning(feat, lab);

	// Make a parallel CNN
	WNeuralNet nn2;
	nn2.addLayer(new WLayerConvolutional2DCuda(e, 3, 3, 1, 2, 2, 1));
	nn2.addLayer(new WLayerClassicCuda(e, 4, 1));
	nn2.beginIncrementalLearning(feat, lab);

	// Make another parallel CNN
	WNeuralNet nn3;
	nn3.addLayer(new WLayerConvolutional2DCuda(e, 3, 3, 1, 2, 2, 1));
	nn3.addLayer(new WLayerClassicCuda(e, 4, 1));
	nn3.beginIncrementalLearning(feat, lab);

	cout << "Copying (so they will have identical weights)...\n";
	((WLayerConvolutional2DCuda*)&nn2.layer(0))->upload(*(WLayerConvolutional2D*)&nn1.layer(0));
	((WLayerClassicCuda*)&nn2.layer(1))->upload(*(WLayerClassic*)&nn1.layer(1));
	((WLayerConvolutional2DCuda*)&nn3.layer(0))->upload(*(WLayerConvolutional2D*)&nn1.layer(0));
	((WLayerClassicCuda*)&nn3.layer(1))->upload(*(WLayerClassic*)&nn1.layer(1));

	cout << "Testing to make sure they make identical predictions (before training)...\n";
	WVec pred(1);
	//cout << "  Input: [" << to_str(feat) << "]\n";
	nn1.predict(feat[0], pred);
	cout << "Classic: [" << to_str(pred) << "]\n";
	nn2.predict(feat[0], pred);
	cout << "  Cuda1: [" << to_str(pred) << "]\n";
	nn3.predict(feat[0], pred);
	cout << "  Cuda2: [" << to_str(pred) << "]\n";


	cout << "Training the classic network...\n";
	double timeBef1 = WTime::seconds();
	for(size_t i = 0; i < epochs; i++)
	{
		nn1.trainIncremental(feat[i % 10], lab[i % 10]);
	}
	double timeAft1 = WTime::seconds();


	cout << "Training the first parallel network...\n";
	r.setSeed(0);
	double timeBef2 = WTime::seconds();
	for(size_t i = 0; i < epochs; i++)
	{
		nn2.trainIncremental(feat[i % 10], lab[i % 10]);
	}
	double timeAft2 = WTime::seconds();

	cout << "Training the second parallel network without synchronization...\n";
	e.setHogWild(true);
	r.setSeed(0);
	double timeBef3 = WTime::seconds();
	for(size_t i = 0; i < epochs; i++)
	{
		nn3.trainIncremental(feat[i % 10], lab[i % 10]);
	}
	double timeAft3 = WTime::seconds();

	cout << "Classic training time: " << to_str(timeAft1 - timeBef1) << " seconds\n";
	cout << "  Cuda1 training time: " << to_str(timeAft2 - timeBef2) << " seconds\n";
	cout << "  Cuda2 training time: " << to_str(timeAft3 - timeBef3) << " seconds\n";
	cout << "Speedup1: " << to_str((timeAft1 - timeBef1) / (timeAft2 - timeBef2)) << "\n";
	cout << "Speedup2: " << to_str((timeAft1 - timeBef1) / (timeAft3 - timeBef3)) << "\n";

	cout << "Testing to make sure both networks still make the same predictions...\n";
	//cout << "  Input: [" << to_str(feat) << "]\n";
	nn1.predict(feat[0], pred);
	cout << "Classic: [" << to_str(pred) << "]\n";
	nn2.predict(feat[0], pred);
	cout << "  Cuda1: [" << to_str(pred) << "]\n";
	nn3.predict(feat[0], pred);
	cout << "  Cuda2: [" << to_str(pred) << "]\n";
}
*/
void doit()
{
	WCudaEngine e;
	test_upload_download(e);
	test_copy(e);
	test_matrix_vector_multiply(e);
	test_tanh(e);
	test_GBlockLinear(e);
	test_GCudaLayer(e);
	//test_convolutional(e);
}

int main(int argc, char* argv[])
{
#ifdef _DEBUG
	WApp::enableFloatingPointExceptions();
#endif
	int nRet = 0;
	try
	{
		doit();
	}
	catch(const std::exception& e)
	{
		cerr << e.what() << "\n";
		nRet = 1;
	}

	return nRet;
}

