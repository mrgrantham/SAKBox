/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Michael R. Smith,
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

#include "WRecommender.h"
#include "WRecommenderLib.h"
#include "WSparseMatrix.h"
#include "WCluster.h"
#include "WMatrix.h"
#include "WHeap.h"
#include "WRand.h"
#include "WNeuralNet.h"
#include "WDistance.h"
#include <math.h>
#include <map>
#include <vector>
#include <cmath>
#include "WDom.h"
#include "WTime.h"
#include "WHolders.h"
#include "WApp.h"
#include "WLearner.h"
#include "WLearnerLib.h"
#include "usage.h"
#include <memory>

using std::map;
using std::multimap;
using std::pair;
using std::set;
using std::vector;

namespace WClasses
{

void WCollaborativeFilter_dims(WMatrix& data, size_t* pOutUsers, size_t* pOutItems)
{
	double m = data.columnMin(0);
	double r = data.columnMax(0);
	if(m < 0)
		throw Ex("col 0 (user) indexes out of range");
	*pOutUsers = size_t(ceil(r)) + 1;
	m = data.columnMin(1);
	r = data.columnMax(1);
	if(m < 0)
		throw Ex("col 1 (item) indexes out of range");
	*pOutItems = size_t(ceil(r)) + 1;
	if(data.rows() * 8 < *pOutUsers)
		throw Ex("col 0 (user) indexes out of range");
	if(data.rows() * 8 < *pOutItems)
		throw Ex("col 1 (item) indexes out of range");
}

WCollaborativeFilter::WCollaborativeFilter()
: m_rand(0)
{
}

WCollaborativeFilter::WCollaborativeFilter(const WDomNode* pNode, WLearnerLoader& ll)
: m_rand(0)
{
}

void WCollaborativeFilter::trainDenseMatrix(const WMatrix& data, const WMatrix* pLabels)
{
	if(!data.relation().areContinuous())
		throw Ex("WCollaborativeFilter::trainDenseMatrix only supports continuous attributes.");

	// Convert to 3-column form
	WMatrix* pMatrix = new WMatrix(0, 3);
	std::unique_ptr<WMatrix> hMatrix(pMatrix);
	size_t dims = data.cols();
	for(size_t i = 0; i < data.rows(); i++)
	{
		const WVec& row = data.row(i);
		for(size_t j = 0; j < dims; j++)
		{
			if(row[j] != UNKNOWN_REAL_VALUE)
			{
				WVec& vec = pMatrix->newRow();
				vec[0] = (double)i;
				vec[1] = (double)j;
				vec[2] = row[j];
			}
		}
	}

	if(pLabels)
	{
		size_t labelDims = pLabels->cols();
		for(size_t i = 0; i < pLabels->rows(); i++)
		{
			const WVec& row = pLabels->row(i);
			for(size_t j = 0; j < labelDims; j++)
			{
				if(row[j] != UNKNOWN_REAL_VALUE)
				{
					WVec& vec = pMatrix->newRow();
					vec[0] = (double)i;
					vec[1] = (double)(dims + j);
					vec[2] = row[j];
				}
			}
		}
	}

	// Train
	train(*pMatrix);
}

WDomNode* WCollaborativeFilter::baseDomNode(WDom* pDoc, const char* szClassName) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "class", szClassName);
	return pNode;
}

double WCollaborativeFilter::crossValidate(WMatrix& data, size_t folds, double* pOutMAE)
{
	// Randomly assign each rating to one of the folds
	size_t ratings = data.rows();
	size_t* pFolds = new size_t[ratings];
	std::unique_ptr<size_t[]> hFolds(pFolds);
	for(size_t i = 0; i < ratings; i++)
		pFolds[i] = (size_t)m_rand.next(folds);

	// Evaluate accuracy
	double ssse = 0.0;
	double smae = 0.0;
	for(size_t i = 0; i < folds; i++)
	{
		// Split the data
		WMatrix dataTrain(data.relation().clone());
		WReleaseDataHolder hDataTrain(&dataTrain);
		WMatrix dataTest(data.relation().clone());
		WReleaseDataHolder hDataTest(&dataTest);
		size_t* pF = pFolds;
		for(size_t j = 0; j < data.rows(); j++)
		{
			if(*pF == i)
				dataTest.takeRow(&data[j]);
			else
				dataTrain.takeRow(&data[j]);
			pF++;
		}

		double mae;
		ssse += trainAndTest(dataTrain, dataTest, &mae);
		smae += mae;
	}

	if(pOutMAE)
		*pOutMAE = smae / folds;
	return ssse / folds;
}

double WCollaborativeFilter::trainAndTest(WMatrix& dataTrain, WMatrix& dataTest, double* pOutMAE)
{
	train(dataTrain);
	double sse = 0.0;
	double se = 0.0;
	[[maybe_unused]] size_t hits = 0;
	for(size_t j = 0; j < dataTest.rows(); j++)
	{
		WVec& vec = dataTest[j];
		double prediction = predict(size_t(vec[0]), size_t(vec[1]));
		if(prediction < -1e100 || prediction > 1e100)
		{
			throw Ex("Unreasonable prediction");
		}
		double err = vec[2] - prediction;
		se += std::abs(err);
		sse += (err * err);
		hits++;
	}
	if(pOutMAE)
		*pOutMAE = se / dataTest.rows();
	return sse / dataTest.rows();
}

class TarPredComparator
{
public:
	TarPredComparator() {}

	bool operator()(const std::pair<double, double>& a, const std::pair<double, double>& b) const
	{
		return a.second > b.second;
	}
};

WMatrix* WCollaborativeFilter::precisionRecall(WMatrix& data, bool ideal)
{
	// Divide into two equal-size folds
	size_t ratings = data.rows();
	size_t halfRatings = ratings / 2;
	size_t* pFolds = new size_t[ratings];
	size_t f0 = ratings - halfRatings;
	size_t f1 = halfRatings;
	for(size_t i = 0; i < ratings; i++)
	{
		if(m_rand.next(f0 + f1) < f0)
		{
			pFolds[i] = 0;
			f0--;
		}
		else
		{
			pFolds[i] = 1;
			f1--;
		}
	}

	// Make a vector of target values and corresponding predictions
	vector<std::pair<double, double> > tarPred;
	tarPred.reserve(halfRatings);

	// Split the data
	WMatrix dataTrain(data.relation().clone());
	WReleaseDataHolder hDataTrain(&dataTrain);
	WMatrix dataTest(data.relation().clone());
	WReleaseDataHolder hDataTest(&dataTest);
	size_t* pF = pFolds;
	for(size_t j = 0; j < data.rows(); j++)
	{
		if(*pF == 0)
			dataTrain.takeRow(&data[j]);
		else
			dataTest.takeRow(&data[j]);
		pF++;
	}

	if(ideal)
	{
		// Simulate perfect predictions
		for(size_t i = 0; i < dataTest.rows(); i++)
		{
			WVec& vec = dataTest[i];
			tarPred.push_back(std::make_pair(vec[2], vec[2]));
		}
	}
	else
	{
		// Train
		train(dataTrain);

		// Predict the ratings in the test data
		for(size_t i = 0; i < dataTest.rows(); i++)
		{
			WVec& vec = dataTest[i];
			double prediction = predict(size_t(vec[0]), size_t(vec[1]));
			WAssert(prediction != UNKNOWN_REAL_VALUE);
			tarPred.push_back(std::make_pair(vec[2], prediction));
		}
	}

	// Make precision-recall data
	TarPredComparator comp;
	std::sort(tarPred.begin(), tarPred.end(), comp);
	double totalRelevant = 0.0;
	double totalIrrelevant = 0.0;
	for(vector<std::pair<double, double> >::iterator it = tarPred.begin(); it != tarPred.end(); it++)
	{
		totalRelevant += it->first;
		totalIrrelevant += (1.0 - it->first); // Here we assume that all ratings range from 0 to 1.
	}
	double retrievedRelevant = 0.0;
	double retrievedIrrelevant = 0.0;
	WMatrix* pResults = new WMatrix(0, 3);
	for(vector<std::pair<double, double> >::iterator it = tarPred.begin(); it != tarPred.end(); it++)
	{
		retrievedRelevant += it->first;
		retrievedIrrelevant += (1.0 - it->first); // Here we assume that all ratings range from 0 to 1.
		double precision = retrievedRelevant / (retrievedRelevant + retrievedIrrelevant);
		double recall = retrievedRelevant / totalRelevant; // recall is the same as the truePositiveRate
		double falsePositiveRate = retrievedIrrelevant / totalIrrelevant;
		WVec& row = pResults->newRow();
		row[0] = recall;
		row[1] = precision;
		row[2] = falsePositiveRate;
	}
	return pResults;
}

// static
double WCollaborativeFilter::areaUnderCurve(WMatrix& data)
{
	double a = 0.0;
	double b = 0.0;
	double prevX = 0.0;
	double prevY = 0.0;
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& row = data[i];
		a += (row[2] - prevX) * row[0];
		b += (row[2] - prevX) * prevY;
		prevX = row[2];
		prevY = row[0];
	}
	a += 1.0 - prevX;
	b += (1.0 - prevX) * prevY;
	return 0.5 * (a + b);
}

void WCF_basicTest_makeData(WMatrix& m, WRand& rand)
{
	// Generate perfectly linear ratings based on random preferences
	// with both item and user bias
	for(size_t i = 0; i < 300; i++)
	{
		double a = rand.uniform();
		double b = rand.normal();
		double c = rand.uniform();
		double userBias = rand.normal();
		WVec& vec1 = m.newRow();
		vec1[0] = (double)i; // user
		vec1[1] = 0; // item
		vec1[2] = a + 0.0 + 0.2 * c + userBias; // rating
		WVec& vec2 = m.newRow();
		vec2[0] = (double)i; // user
		vec2[1] = 1; // item
		vec2[2] = 0.2 * a + 0.2 * b + c * c + 0.2 + userBias; // rating
		WVec& vec3 = m.newRow();
		vec3[0] = (double)i; // user
		vec3[1] = 2; // item
		vec3[2] = 0.6 * a + 0.1 * b + 0.2 * c * c * c - 0.3 + userBias; // rating
		WVec& vec4 = m.newRow();
		vec4[0] = (double)i; // user
		vec4[1] = 3; // item
		vec4[2] = 0.5 * a + 0.5 * b - 0.5 * c + 0.0 + userBias; // rating
		WVec& vec5 = m.newRow();
		vec5[0] = (double)i; // user
		vec5[1] = 4; // item
		vec5[2] = -0.2 * a + 0.4 * b - 0.3 * sin(c) + 0.1 + userBias; // rating
	}
}

void WCollaborativeFilter::basicTest(double maxMSE)
{
	WRand rnd(0);
	WMatrix m(0, 3);
	WCF_basicTest_makeData(m, rnd);
	double mse = crossValidate(m, 2);
	if(mse > maxMSE)
		throw Ex("Failed. Expected MSE=", to_str(maxMSE), ". Actual MSE=", to_str(mse), ".");
	else if(mse + 0.085 < maxMSE)
		std::cerr << "\nTest needs to be tightened. MSE: " << mse << ", maxMSE: " << maxMSE << "\n";
}





WBaselineRecommender::WBaselineRecommender()
: WCollaborativeFilter(), m_items(0)
{
}

WBaselineRecommender::WBaselineRecommender(const WDomNode* pNode, WLearnerLoader& ll)
: WCollaborativeFilter(pNode, ll)
{
	m_ratings.deserialize(pNode->get("ratings"));
}

// virtual
WBaselineRecommender::~WBaselineRecommender()
{
}

// virtual
void WBaselineRecommender::train(WMatrix& data)
{
	// Determine the sizes
	if(data.cols() != 3)
		throw Ex("Expected 3 cols");
	//	double m = data.columnMin(1);
	double r = data.columnMax(1);
	m_items = size_t(ceil(r)) + 1;
	if(data.rows() * 8 < m_items)
		throw Ex("column 1 (item) indexes out of range");

	// Allocate space
	m_ratings.resize(m_items);
	size_t* pCounts = new size_t[m_items];
	std::unique_ptr<size_t[]> hCounts(pCounts);
	size_t* pC = pCounts;
	WVec& rr = m_ratings;
	for(size_t i = 0; i < m_items; i++)
	{
		pC[i] = 0;
		rr[i] = 0.0;
	}
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data[i];
		size_t c = size_t(vec[1]);
		rr[c] *= ((double)pCounts[c] / (pCounts[c] + 1));
		rr[c] += (vec[2] / (pCounts[c] + 1));
		pCounts[c]++;
	}
}

// virtual
double WBaselineRecommender::predict(size_t user, size_t item)
{
	if(item >= m_items)
		return 0.0;
	return m_ratings[item];
}

// virtual
void WBaselineRecommender::impute(WVec& vec, size_t dims)
{
	size_t n = std::min(dims, m_items);
	size_t i;
	for(i = 0; i < n; i++)
	{
		if(vec[i] == UNKNOWN_REAL_VALUE)
			vec[i] = m_ratings[i];
	}
	for(; i < dims; i++)
	{
		if(vec[i] == UNKNOWN_REAL_VALUE)
			vec[i] = 0.0;
	}
}

// virtual
WDomNode* WBaselineRecommender::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBaselineRecommender");
	pNode->add(pDoc, "ratings", m_ratings.serialize(pDoc));
	return pNode;
}

// static
void WBaselineRecommender::test()
{
	WBaselineRecommender rec;
	rec.basicTest(1.16);
}







WInstanceRecommender::WInstanceRecommender(size_t neighbors)
: WCollaborativeFilter(), m_neighbors(neighbors), m_ownMetric(true), m_pData(nullptr), m_pBaseline(nullptr), m_significanceWeight(0)
{
	m_pMetric = new WCosineSimilarity();
}

WInstanceRecommender::WInstanceRecommender(const WDomNode* pNode, WLearnerLoader& ll)
: WCollaborativeFilter(pNode, ll)
{
	m_neighbors = (size_t)pNode->getInt("neighbors");
	m_pMetric = WSparseSimilarity::deserialize(pNode->get("metric"));
	m_ownMetric = true;
	m_pData = new WSparseMatrix(pNode->get("data"));
	m_pBaseline = new WBaselineRecommender(pNode->get("bl"), ll);
	m_significanceWeight = (size_t)pNode->getInt("sigWeight");
}

// virtual
WInstanceRecommender::~WInstanceRecommender()
{
	delete(m_pData);
	if(m_ownMetric)
		delete(m_pMetric);
	delete(m_pBaseline);
}

void WInstanceRecommender::setMetric(WSparseSimilarity* pMetric, bool own)
{
	if(m_ownMetric)
		delete(m_pMetric);
	m_pMetric = pMetric;
	m_ownMetric = own;
}

// virtual
void WInstanceRecommender::train(WMatrix& data)
{
	if(data.cols() != 3)
		throw Ex("Expected 3 cols");

	// Compute the baseline recommendations
	delete(m_pBaseline);
	m_pBaseline = new WBaselineRecommender();
	m_pBaseline->train(data);

	// Store the data
	size_t users, items;
	WCollaborativeFilter_dims(data, &users, &items);
	delete(m_pData);
	m_pData = new WSparseMatrix(users, items, UNKNOWN_REAL_VALUE);
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data[i];
		m_pData->set(size_t(vec[0]), size_t(vec[1]), vec[2]);
	}
}

// virtual
double WInstanceRecommender::predict(size_t user, size_t item)
{
	if(!m_pData)
		throw Ex("This model has not been trained");
	if(user >= m_pData->rows() || item >= m_pData->cols())
		return 0.0;

	// Find the k-nearest neighbors
	multimap<double, size_t> depq; // double-ended priority-queue that maps from similarity to user-id
	for(size_t neigh = 0; neigh < m_pData->rows(); neigh++)
	{
		// Only consider other users that have rated this item
		if(neigh == user)
			continue;
		double rating = m_pData->get(neigh, item);
		if(rating == UNKNOWN_REAL_VALUE)
			continue;

		// Compute the similarity
		size_t count = WSparseVec::count_matching_elements(m_pData->row(user), m_pData->row(neigh));
		double similarity = m_pMetric->similarity(m_pData->row(user), m_pData->row(neigh));

		if(count < m_significanceWeight)
			similarity *= (double)count / m_significanceWeight; // SAKBox: was integer division (always 0 here)

		// If the queue is overfull, drop the worst item
		depq.insert(std::make_pair(similarity, neigh));
		if(depq.size() > m_neighbors)
			depq.erase(depq.begin());
	}

	// Combine the ratings of the nearest neighbors to make a prediction
	double weighted_sum = 0.0;
	double sum_weight = 0.0;
	for(multimap<double, size_t>::iterator it = depq.begin(); it != depq.end(); it++)
	{
		double weight = std::max(0.0, std::min(1.0, it->first));
		double val = m_pData->get(it->second, item);
		weighted_sum += weight * val;
		sum_weight += weight;
	}
	if(sum_weight > 0.0)
		return weighted_sum / sum_weight;
	else
		return m_pBaseline->predict(user, item);
}

multimap<double, ArrayWrapper> WInstanceRecommender::getNeighbors(size_t user, size_t item)
{
	if(!m_pData)
		throw Ex("This model has not been trained");
	if(user >= m_pData->rows() || item >= m_pData->cols())
		throw Ex("User and/or item not in the provided data set");

	// Find the k-nearest neighbors
	if(m_user_depq.find(user) == m_user_depq.end())
	{
		multimap<double, ArrayWrapper> depq; // double-ended priority-queue that maps from similarity to user-id
		for(size_t neigh = 0; neigh < m_pData->rows(); neigh++)
		{
			// Only consider other users that have rated this item
			if(neigh == user)
				continue;
			double rating = m_pData->get(neigh, item);
			if(rating == UNKNOWN_REAL_VALUE)
				continue;

			// Compute the similarity
			size_t count = WSparseVec::count_matching_elements(m_pData->row(user), m_pData->row(neigh));
			double similarity = m_pMetric->similarity(m_pData->row(user), m_pData->row(neigh));

			if(count < m_significanceWeight)
				similarity *= (double)count / m_significanceWeight; // SAKBox: was integer division (always 0 here)

			// If the queue is overfull, drop the worst item
			ArrayWrapper temp = {{neigh, count}};
			depq.insert(std::make_pair(similarity, temp));
			if(depq.size() > m_neighbors)
				depq.erase(depq.begin());
		}
		m_user_depq[user] = depq;
	}

	return m_user_depq[user];
}

// virtual
void WInstanceRecommender::impute(WVec& vec, size_t dims)
{
	if(!m_pData)
		throw Ex("This model has not been trained");
	if(dims != m_pData->cols())
		throw Ex("The vector has a different size than this model was trained with");

	// Find the k-nearest neighbors
	multimap<double, size_t> depq; // double-ended priority-queue that maps from similarity to user-id
	for(size_t neigh = 0; neigh < m_pData->rows(); neigh++)
	{
		// Compute the similarity
		size_t count = vec.size();
		double similarity = m_pMetric->similarity(m_pData->row(neigh), vec);

		if(count < m_significanceWeight)
			similarity *= (double)count / m_significanceWeight; // SAKBox: was integer division (always 0 here)

		// If the queue is overfull, drop the worst item
		depq.insert(std::make_pair(similarity, neigh));
		if(depq.size() > m_neighbors)
			depq.erase(depq.begin());
	}

	// Impute missing values by combining the ratings from the neighbors
	for(size_t i = 0; i < m_pData->cols(); i++)
	{
		if(vec[i] == UNKNOWN_REAL_VALUE)
		{
			double weighted_sum = 0.0;
			double sum_weight = 0.0;
			for(multimap<double, size_t>::iterator it = depq.begin(); it != depq.end(); it++)
			{
				double val = m_pData->get(it->second, i);
				if(val != UNKNOWN_REAL_VALUE)
				{
					double weight = std::max(0.0, std::min(1.0, it->first));
					weighted_sum += weight * val;
					sum_weight += weight;
				}
			}
			if(sum_weight > 0.0)
				vec[i] = weighted_sum / sum_weight;
			else
				vec[i] = m_pBaseline->predict(0, i); // baseline ignores the user
		}
	}
}

// virtual
WDomNode* WInstanceRecommender::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WInstanceRecommender");
	pNode->add(pDoc, "neighbors", m_neighbors);
	pNode->add(pDoc, "metric", m_pMetric->serialize(pDoc));
	pNode->add(pDoc, "data", m_pData->serialize(pDoc));
	pNode->add(pDoc, "bl", m_pBaseline->serialize(pDoc));
	pNode->add(pDoc, "sigWeight", m_significanceWeight);
	return pNode;
}

double WInstanceRecommender::getRating(size_t user, size_t item)
{
	return m_pData->get(user, item);
}

// static
void WInstanceRecommender::test()
{
	WInstanceRecommender rec(8);
	rec.basicTest(0.63);
}





WSparseClusterRecommender::WSparseClusterRecommender(size_t clusters)
: WCollaborativeFilter(), m_clusters(clusters), m_pPredictions(nullptr), m_pClusterer(nullptr), m_ownClusterer(false), m_users(0), m_items(0)
{
}

// virtual
WSparseClusterRecommender::~WSparseClusterRecommender()
{
	if(m_ownClusterer)
		delete(m_pClusterer);
	delete(m_pPredictions);
}

void WSparseClusterRecommender::setClusterer(WSparseClusterer* pClusterer, bool own)
{
	if(pClusterer->clusterCount() != m_clusters)
		throw Ex("Mismatching number of clusters");
	if(m_ownClusterer)
		delete(m_pClusterer);
	m_pClusterer = pClusterer;
	m_ownClusterer = own;
}

// virtual
void WSparseClusterRecommender::train(WMatrix& data)
{
	if(data.cols() != 3)
		throw Ex("Expected 3 cols");

	// Convert the data to a sparse matrix
	size_t users, items;
	WCollaborativeFilter_dims(data, &users, &items);
	m_users = users;
	m_items = items;
	WSparseMatrix sm(users, items, UNKNOWN_REAL_VALUE);
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data.row(i);
		sm.set(size_t(vec[0]), size_t(vec[1]), vec[2]);
	}

	// Make sure we have a clusterer
	if(!m_pClusterer)
		setClusterer(new WKMeansSparse(m_clusters, &m_rand), true);

	// Cluster the data
	m_pClusterer->cluster(&sm);

	// Gather the mean predictions in each cluster
	delete(m_pPredictions);
	m_pPredictions = new WMatrix(m_clusters, sm.cols());
	m_pPredictions->fill(0.0);
	size_t* pCounts = new size_t[sm.cols() * m_clusters];
	std::unique_ptr<size_t[]> hCounts(pCounts);
	memset(pCounts, '\0', sizeof(size_t) * sm.cols() * m_clusters);
	for(size_t i = 0; i < sm.rows(); i++)
	{
		size_t clust = m_pClusterer->whichCluster(i);
		WVec& row = m_pPredictions->row(clust);
		size_t* pRowCounts = pCounts + (sm.cols() * clust);
		for(WSparseMatrix::Iter it = sm.rowBegin(i); it != sm.rowEnd(i); it++)
		{
			row[it->first] *= ((double)pRowCounts[it->first] / (pRowCounts[it->first] + 1));
			row[it->first] += (it->second / (pRowCounts[it->first] + 1));
			pRowCounts[it->first]++;
		}
	}
}

// virtual
double WSparseClusterRecommender::predict(size_t user, size_t item)
{
	size_t clust = m_pClusterer->whichCluster(user);
	WVec& row = m_pPredictions->row(clust);
	return row[item];
}

// virtual
void WSparseClusterRecommender::impute(WVec& vec, size_t dims)
{
	throw Ex("Sorry, WSparseClusterRecommender::impute is not yet implemented");
	// todo: Find the closest centroid, and use it to impute all values
}

// virtual
WDomNode* WSparseClusterRecommender::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WSparseClusterRecommender");
	throw Ex("Sorry, this method has not been implemented yet");
	return pNode;
}

// static
void WSparseClusterRecommender::test()
{
	WSparseClusterRecommender rec(6);
	// SAKBox: was 1.31, which passed with WKMeansSparse broken (MSE 1.256,
	// worse than WBaselineRecommender's 1.152); fixed, it's 0.882
	rec.basicTest(0.95);
}














WDenseClusterRecommender::WDenseClusterRecommender(size_t clusters)
: WCollaborativeFilter(), m_clusters(clusters), m_pPredictions(nullptr), m_pClusterer(nullptr), m_ownClusterer(false), m_users(0), m_items(0)
{
}

// virtual
WDenseClusterRecommender::~WDenseClusterRecommender()
{
	if(m_ownClusterer)
		delete(m_pClusterer);
	delete(m_pPredictions);
}

void WDenseClusterRecommender::setClusterer(WClusterer* pClusterer, bool own)
{
	if(pClusterer->clusterCount() != m_clusters)
		throw Ex("Mismatching number of clusters");
	if(m_ownClusterer)
		delete(m_pClusterer);
	m_pClusterer = pClusterer;
	m_ownClusterer = own;
}

void WDenseClusterRecommender::setFuzzifier(double d)
{
	if(!m_pClusterer)
		setClusterer(new WFuzzyKMeans(m_clusters, &m_rand), true);
	((WFuzzyKMeans*)m_pClusterer)->setFuzzifier(d);
}

// virtual
void WDenseClusterRecommender::train(WMatrix& data)
{
	if(data.cols() != 3)
		throw Ex("Expected 3 cols");

	if(!m_pClusterer)
		setClusterer(new WFuzzyKMeans(m_clusters, &m_rand), true);

	// Cluster the data
	size_t users, items;
	WCollaborativeFilter_dims(data, &users, &items);
	m_users = users;
	m_items = items;
	{
		// SAKBox: WMatrix doesn't clear new rows, so the ratings a user hadn't
		// given were whatever was in memory; mark them unknown instead (the
		// fuzzy k-means and its distance skip unknown values)
		WMatrix dense(users, items);
		dense.fill(UNKNOWN_REAL_VALUE);
		for(size_t i = 0; i < data.rows(); i++)
		{
			WVec& vec = data.row(i);
			dense[size_t(vec[0])][size_t(vec[1])] = vec[2];
		}
		m_pClusterer->cluster(&dense);
	}

	// Gather the mean predictions in each cluster
	delete(m_pPredictions);
	m_pPredictions = new WMatrix(m_clusters, items);
	m_pPredictions->fill(0.0);
	size_t* pCounts = new size_t[items * m_clusters];
	std::unique_ptr<size_t[]> hCounts(pCounts);
	memset(pCounts, '\0', sizeof(size_t) * items * m_clusters);
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data.row(i);
		size_t user = size_t(vec[0]);
		size_t item = size_t(vec[1]);
		size_t clust = m_pClusterer->whichCluster(user);
		WVec& row = m_pPredictions->row(clust);
		size_t* pRowCounts = pCounts + (items * clust);
		row[item] *= ((double)pRowCounts[item] / (pRowCounts[item] + 1));
		row[item] += (vec[2] / (pRowCounts[item] + 1));
		pRowCounts[item]++;
	}
}

// virtual
double WDenseClusterRecommender::predict(size_t user, size_t item)
{
	if(user >= m_users || item >= m_items)
		return 0.0;
	size_t clust = m_pClusterer->whichCluster(user);
	WVec& row = m_pPredictions->row(clust);
	return row[item];
}

// virtual
void WDenseClusterRecommender::impute(WVec& vec, size_t dims)
{
	throw Ex("Sorry, WDenseClusterRecommender::impute is not yet implemented");
	// todo: Find the closest centroid, and use it to impute all values
}

// virtual
WDomNode* WDenseClusterRecommender::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WDenseClusterRecommender");
	throw Ex("Sorry, this method has not been implemented yet");
	return pNode;
}

// static
void WDenseClusterRecommender::test()
{
	WDenseClusterRecommender rec(6);
	// SAKBox: was 0.0, a placeholder (and this test wasn't run); measured 0.338
	rec.basicTest(0.40);
}









WMatrixFactorization::WMatrixFactorization(size_t intrinsicDims)
: WCollaborativeFilter(), m_intrinsicDims(intrinsicDims), m_regularizer(0.01), m_pP(nullptr), m_pQ(nullptr), m_pPMask(nullptr), m_pQMask(nullptr), m_pPWeights(nullptr), m_pQWeights(nullptr), m_nonNeg(false), m_minIters(1), m_decayRate(0.97)
{
}

WMatrixFactorization::WMatrixFactorization(const WDomNode* pNode, WLearnerLoader& ll)
: WCollaborativeFilter(pNode, ll)
{
	m_regularizer = pNode->getDouble("reg");
	m_minIters = (size_t)pNode->getInt("mi");
	m_decayRate = pNode->getDouble("dr");
	m_pP = new WMatrix(pNode->get("p"));
	m_pQ = new WMatrix(pNode->get("q"));
	WDomNode* pPMask = pNode->getIfExists("pm");
	if(pPMask)
		m_pPMask = new WMatrix(pPMask);
	else
		m_pPMask = nullptr;
	WDomNode* pPWeights = pNode->getIfExists("pw");
	if(pPWeights)
		m_pPWeights = new WMatrix(pPWeights);
	else
		m_pPWeights = nullptr;
	WDomNode* pQMask = pNode->getIfExists("qm");
	if(pQMask)
		m_pQMask = new WMatrix(pQMask);
	else
		m_pQMask = nullptr;
	WDomNode* pQWeights = pNode->getIfExists("qw");
	if(pQWeights)
		m_pQWeights = new WMatrix(pQWeights);
	else
		m_pQWeights = nullptr;
	if(m_pP->cols() != m_pQ->cols())
		throw Ex("Mismatching matrix sizes");
	m_intrinsicDims = m_pP->cols() - 1;
}

// virtual
WMatrixFactorization::~WMatrixFactorization()
{
	delete(m_pQ);
	delete(m_pP);
	delete(m_pPMask);
	delete(m_pQMask);
	delete(m_pPWeights);
	delete(m_pQWeights);
}

// virtual
WDomNode* WMatrixFactorization::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WMatrixFactorization");
	pNode->add(pDoc, "reg", m_regularizer);
	pNode->add(pDoc, "mi", m_minIters);
	pNode->add(pDoc, "dr", m_decayRate);
	pNode->add(pDoc, "p", m_pP->serialize(pDoc));
	pNode->add(pDoc, "q", m_pQ->serialize(pDoc));
	if(m_pPMask)
	{
		pNode->add(pDoc, "pm", m_pPMask->serialize(pDoc));
		pNode->add(pDoc, "pw", m_pPWeights->serialize(pDoc));
	}
	if(m_pQMask)
	{
		pNode->add(pDoc, "qm", m_pQMask->serialize(pDoc));
		pNode->add(pDoc, "qw", m_pQWeights->serialize(pDoc));
	}
	return pNode;
}

void WMatrixFactorization::clampUserElement(size_t user, size_t attr, double val)
{
	if(attr >= m_intrinsicDims)
		throw Ex("out of range");
	if(!m_pPMask)
	{
		m_pPMask = new WMatrix(0, m_intrinsicDims);
		m_pPWeights = new WMatrix(2, m_intrinsicDims);
		m_pPWeights->fill(0.0);
	}
	while(m_pPMask->rows() <= user)
		m_pPMask->newRow().fill(UNKNOWN_REAL_VALUE);
	m_pPMask->row(user)[attr] = val;
}

void WMatrixFactorization::clampItemElement(size_t item, size_t attr, double val)
{
	if(attr >= m_intrinsicDims)
		throw Ex("out of range");
	if(!m_pQMask)
	{
		m_pQMask = new WMatrix(0, m_intrinsicDims);
		m_pQWeights = new WMatrix(2, m_intrinsicDims);
		m_pQWeights->fill(0.0);
	}
	while(m_pQMask->rows() <= item)
		m_pQMask->newRow().fill(UNKNOWN_REAL_VALUE);
	m_pQMask->row(item)[attr] = val;
}

void WMatrixFactorization::clampUsers(const WMatrix& data, size_t offset)
{
	size_t vals = data.cols() - 1;
	for(size_t i = 0; i < data.rows(); i++)
	{
		const WVec& row = data[i];
		size_t index = (size_t)row[0];
		offset--;
		for(size_t j = 1; j <= vals; j++)
			clampUserElement(index, offset + j, row[j]);
	}
}

void WMatrixFactorization::clampItems(const WMatrix& data, size_t offset)
{
	size_t vals = data.cols() - 1;
	for(size_t i = 0; i < data.rows(); i++)
	{
		const WVec& row = data[i];
		size_t index = (size_t)row[0];
		offset--;
		for(size_t j = 1; j <= vals; j++)
			clampItemElement(index, offset + j, row[j]);
	}
}

double WMatrixFactorization::validate(WMatrix& data)
{
	double sse = 0;
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data[i];
		WVec& pref = m_pP->row(size_t(vec[0]));
		WVec& weights = m_pQ->row(size_t(vec[1]));
		double pred = weights[0] + pref[0];
		for(size_t j = 1; j <= m_intrinsicDims; j++)
			pred += pref[j] * weights[j];
		double err = vec[2] - pred;
		sse += (err * err);
	}
	return sse;
}

void WMatrixFactorization::clampP(size_t i)
{
	WVec& p = m_pP->row(i);
	WVec& mask = m_pPMask->row(i);
	WVec& bias = m_pPWeights->row(0);
	WVec& weights = m_pPWeights->row(1);
	for(size_t j = 0; j < m_intrinsicDims; j++)
	{
		if(mask[j] != UNKNOWN_REAL_VALUE)
			p[j + 1] = bias[j] + weights[j] * mask[j];
	}
}

void WMatrixFactorization::clampQ(size_t i)
{
	WVec& q = m_pQ->row(i);
	WVec& mask = m_pQMask->row(i);
	WVec& bias = m_pQWeights->row(0);
	WVec& weights = m_pQWeights->row(1);
	for(size_t j = 0; j < m_intrinsicDims; j++)
	{
		if(mask[j] != UNKNOWN_REAL_VALUE)
			q[j + 1] = bias[j] + weights[j] * mask[j];
	}
}

void WMatrixFactorization_absValues(double* pVec, size_t dims)
{
	while(true)
	{
		*pVec = std::abs(*pVec);
		if(--dims == 0)
			return;
	}
}

// virtual
void WMatrixFactorization::train(WMatrix& data)
{
	size_t users, items;
	WCollaborativeFilter_dims(data, &users, &items);

	// Initialize P and Q with small random values
	delete(m_pP);
	size_t colsP = 1 + m_intrinsicDims;
	m_pP = new WMatrix(users, colsP);
	for(size_t i = 0; i < m_pP->rows(); i++)
	{
		WVec& vec = m_pP->row(i);
		vec.fillNormal(m_rand, 0.02);
		if(m_nonNeg)
			WMatrixFactorization_absValues(m_pP->row(i).data() + 1, m_intrinsicDims);
	}
	delete(m_pQ);
	m_pQ = new WMatrix(items, 1 + m_intrinsicDims);
	for(size_t i = 0; i < m_pQ->rows(); i++)
	{
		WVec& vec = m_pQ->row(i);
		vec.fillNormal(m_rand, 0.02);
		if(m_nonNeg)
			WMatrixFactorization_absValues(m_pQ->row(i).data() + 1, m_intrinsicDims);
	}

	// Make a shallow copy of the data (so we can shuffle it)
	WMatrix dataCopy(data.relation().clone());
	WReleaseDataHolder hDataCopy(&dataCopy);
	for(size_t i = 0; i < data.rows(); i++)
		dataCopy.takeRow(&data[i]);

	// Train
	double prevErr = 1e10;
	double learningRate = 0.01;
	WVec pT(m_intrinsicDims + 1);
	[[maybe_unused]] size_t epochs = 0;
	while(learningRate >= 0.001)
	{
		WMatrix backupP(*m_pP);
		WMatrix backupQ(*m_pQ);
		for(size_t iter = 0; iter < m_minIters; iter++)
		{
			// Shuffle the ratings
			dataCopy.shuffle(m_rand);

			// Do an epoch of training
			for(size_t j = 0; j < dataCopy.rows(); j++)
			{
				WVec& vec = dataCopy[j];
				size_t user = (size_t)vec[0];
				size_t item = (size_t)vec[1];
				if(m_pPMask && user < m_pPMask->rows())
					clampP(user);
				if(m_pQMask && item < m_pQMask->rows())
					clampQ(item);

				// Compute the error for this rating
				WVec& p = m_pP->row(user);
				WVec& q = m_pQ->row(item);
				double pred = q[0] + p[0];
				for(size_t i = 1; i <= m_intrinsicDims; i++)
					pred += p[i] * q[i];
				double err = vec[2] - pred;

				// Update Q
				q[0] += learningRate * (err - m_regularizer * (q[0]));
				for(size_t i = 1; i <= m_intrinsicDims; i++)
				{
					pT[i] = q[i];
					q[i] += learningRate * (err * p[i] - m_regularizer * q[i]);
					if(m_nonNeg)
						q[i] = std::max(0.0, q[i]);
				}
				if(m_pQMask && item < m_pQMask->rows())
				{
					// Update the bias and weights for clamped values
					WVec& mask = m_pQMask->row(item);
					WVec& bb = m_pQWeights->row(0);
					WVec& w = m_pQWeights->row(1);
					for(size_t i = 0; i < m_intrinsicDims; i++)
					{
						if(mask[i] != UNKNOWN_REAL_VALUE)
						{
							bb[i] += 0.1 * learningRate * err * p[i + 1];
							w[i] += 0.1 * learningRate * err * p[i + 1] * mask[i];
						}
					}
				}

				// Update P
				p[0] += learningRate * (err - m_regularizer * p[0]);
				for(size_t i = 1; i <= m_intrinsicDims; i++)
				{
					p[i] += learningRate * (err * pT[i] - m_regularizer * p[i]);
					if(m_nonNeg)
						p[i] = std::max(0.0, p[i]);
				}
				if(m_pPMask && user < m_pPMask->rows())
				{
					// Update the bias and weights for clamped values
					WVec& mask = m_pPMask->row(user);
					WVec& bb = m_pPWeights->row(0);
					WVec& w = m_pPWeights->row(1);
					for(size_t i = 0; i < m_intrinsicDims; i++)
					{
						if(mask[i] != UNKNOWN_REAL_VALUE)
						{
							bb[i] += 0.1 * learningRate * err * pT[i + 1];
							w[i] += 0.1 * learningRate * err * pT[i + 1] * mask[i];
						}
					}
				}
			}
			epochs++;
		}

		// Stopping criteria
		double rsse = sqrt(validate(data));
		if(rsse >= 1e-12 && 1.0 - (rsse / prevErr) >= 0.001) {}
		else // This awkward if/else structure causes "nan" to be handled in a useful way
		{
			if(rsse <= prevErr) {}
			else // This awkward if/else structure causes "nan" to be handled in a useful way
			{
				// We didn't even get better, so restore from backup
				m_pP->copyBlock(backupP, 0, 0, INVALID_INDEX, INVALID_INDEX, 0, 0, false);
				m_pQ->copyBlock(backupQ, 0, 0, INVALID_INDEX, INVALID_INDEX, 0, 0, false);
			}
			learningRate *= m_decayRate; // decay the learning rate
		}
		prevErr = rsse;
	}
}

// virtual
double WMatrixFactorization::predict(size_t user, size_t item)
{
	if(!m_pP)
		throw Ex("Not trained yet");
	if(user >= m_pP->rows() || item >= m_pQ->rows())
		return 0.0;
	WVec& q = m_pQ->row(item);
	WVec& p = m_pP->row(user);
	double pred = p[0] + q[0];
	for(size_t i = 1; i <= m_intrinsicDims; i++)
		pred += p[i] * q[i];
	return pred;
}

void WMatrixFactorization_vectorToRatings(const WVec& vec, size_t dims, WMatrix& data)
{
	for(size_t i = 0; i < dims; i++)
	{
		if(vec[i] != UNKNOWN_REAL_VALUE)
		{
			WVec& row = data.newRow();
			row[0] = 0.0;
			row[1] = (double)i;
			row[2] = vec[i];
		}
	}
}

// virtual
void WMatrixFactorization::impute(WVec& vec, size_t dims)
{
	if(!m_pP)
		throw Ex("Not trained yet");

	// Convert the vector to a set of ratings
	WMatrix data(0, 3);
	WMatrixFactorization_vectorToRatings(vec, std::min(dims, m_pQ->rows()), data);

	// Initialize a preference vector
	WVec pP(1 + m_intrinsicDims);
	pP.fillNormal(m_rand, 0.02);

	// Refine the preference vector
	double prevErr = 1e308;
	double learningRate = 0.05;
	while(learningRate >= 0.001)
	{
		// Shuffle the ratings
		data.shuffle(m_rand);

		// Do an epoch of training
		for(size_t i = 0; i < data.rows(); i++)
		{
			// Compute the error for this rating
			WVec& v = data[i];
			WVec& q = m_pQ->row(size_t(v[1]));
			double pred = pP[0] + q[0];
			for(size_t j = 1; j <= m_intrinsicDims; j++)
				pred += pP[j] * q[j];
			double err = v[2] - pred;

			// Update the preference vec
			pP[0] += learningRate * (err - m_regularizer * pP[0]);
			for(size_t j = 1; j <= m_intrinsicDims; j++)
				pP[j] += learningRate * (err * q[j] - m_regularizer * pP[j]);
			pP.clip(-1.8, 1.8);
		}

		// Stopping criteria
		double rsse = sqrt(validate(data));
		if(rsse >= 1e-12 && 1.0 - (rsse / prevErr) >= 0.001) // If the amount of improvement is large
		{
		}
		else
			learningRate *= m_decayRate; // decay the learning rate
		prevErr = rsse;
	}

	// Impute missing values
	size_t n = std::min(dims, m_pQ->rows());
	size_t i;
	for(i = 0; i < n; i++)
	{
		if(vec[i] == UNKNOWN_REAL_VALUE)
		{
			WVec& q = m_pQ->row(i);
			double pred = pP[0] + q[0];
			for(size_t j = 1; j <= m_intrinsicDims; j++)
				pred += pP[j] * q[j];
			vec[i] = pred;
		}
	}
	for(; i < dims; i++)
	{
		if(vec[i] == UNKNOWN_REAL_VALUE)
			vec[i] = 0.0;
	}
}

// static
void WMatrixFactorization::test()
{
	WMatrixFactorization rec(3);
	rec.setRegularizer(0.002);
	rec.basicTest(0.17);
}






/*
WHybridNonlinearPCA::WHybridNonlinearPCA(size_t intrinsicDims)
: WNonlinearPCA(intrinsicDims), m_itemAttrs(NULL), m_itemMax(NULL), m_itemMin(NULL), m_itemMap(NULL), m_numNeighbors(100), m_pRatingCount(NULL)
{
	m_neighbors = NULL;
}

// virtual
WHybridNonlinearPCA::~WHybridNonlinearPCA()
{
}

void WHybridNonlinearPCA::train(WMatrix& data)
{
	size_t usrs, items;
	WCollaborativeFilter_dims(data, &items, &usrs);
	m_items = items;

	// Copy and normalize the ratings
	WMatrix* pClone = new WMatrix();
	pClone->copy(&data);
	std::unique_ptr<WMatrix> hClone(pClone);
	delete[] m_pMins;
	m_pMins = new double[items];
	delete[] m_pMaxs;
	m_pMaxs = new double[items];
	delete[] m_pRatingCount;
	m_pRatingCount = new size_t[usrs];
	WVec::setAll(m_pMins, 1e200, items);
	WVec::setAll(m_pMaxs, -1e200, items);
	WIndexVec::setAll(m_pRatingCount, 0, usrs);
	for(size_t i = 0; i < pClone->rows(); i++)
	{
		WVec& vec = pClone->row(i);
		m_pMins[size_t(vec[0])] = std::min(m_pMins[size_t(vec[0])], vec[2]);
		m_pMaxs[size_t(vec[0])] = std::max(m_pMaxs[size_t(vec[0])], vec[2]);
	}
	for(size_t i = 0; i < items; i++)
	{
		if(m_pMins[i] >= 1e200)
			m_pMins[i] = 0.0;
		if(m_pMaxs[i] < m_pMins[i] + 1e-12)
			m_pMaxs[i] = m_pMins[i] + 1.0;
	}
	for(size_t i = 0; i < pClone->rows(); i++)
	{
		WVec&  vec = pClone->row(i);
		vec[2] = (vec[2] - m_pMins[size_t(vec[0])]) / (m_pMaxs[size_t(vec[0])] - m_pMins[size_t(vec[0])]);
		m_itemSet.insert((size_t)vec[1]);
		m_pRatingCount[(size_t)vec[1]]++;
	}

	// Prep the model for incremental training
	size_t numAttr = m_itemAttrs->cols() - 1;
	WUniformRelation featureRel(m_intrinsicDims + numAttr);
	WUniformRelation labelRel(items);
	m_pModel->setUseInputBias(m_useInputBias);
	m_pModel->beginIncrementalLearning(featureRel, labelRel);
	WNeuralNet nn;
	nn.addLayer(new WLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE));
	nn.setUseInputBias(m_useInputBias);
	nn.beginIncrementalLearning(featureRel, labelRel);
	double* pPrefGradient = new double[m_intrinsicDims + numAttr];
	std::unique_ptr<double[]> hPrefGradient(pPrefGradient);

	// Train
	int startPass = 0;
	if(!m_useThreePass)
		startPass = 2;
	else if(m_pModel->layerCount() == 1)
		startPass = 2;
	for(int pass = startPass; pass < 3; pass++)
	{
		WNeuralNet* pNN = (pass == 0 ? &nn : m_pModel);
		if(pass == startPass) //-1)
		{
			delete(m_pUsers);
			m_pUsers = new WMatrix(usrs, m_intrinsicDims + numAttr);
			delete[] m_itemMap;
			m_itemMap = new size_t[m_itemAttrs->rows()];
			WIndexVec::setAll(m_itemMap, 0, m_itemAttrs->rows());
			size_t count = 0;
			double* itemVec = m_itemAttrs->row(count);
			for(size_t i = 0; i < usrs; i++)
			{
				double* vec = m_pUsers->row(i);
				WVec::setAll(vec, 0, m_intrinsicDims + numAttr);
				for(size_t j = 0; j < m_intrinsicDims; j++)
					*(vec++) = 0.01 * m_rand.normal();
				if(*itemVec == i)
				{
					m_itemMap[count]=i;
					*(itemVec) = 0;
					itemVec++;
					for(size_t j = 1; j < numAttr+1; j++)
					{
						*(vec++) = *(itemVec++) * 0.01;
					}
					itemVec = m_itemAttrs->row(++count);
				}
			}
		}
		double rateBegin = 0.1;
		double rateEnd = 0.001;
		double prevErr = 1e10;
		for(double learningRate = rateBegin; learningRate > rateEnd; )
		{
			WNeuralNet backupNet;
			backupNet.copyStructure(pNN);
			WMatrix backupUsers(m_pUsers->rows(), m_intrinsicDims);
			backupUsers.copyBlock(*m_pUsers, 0, 0, INVALID_INDEX, m_intrinsicDims, 0, 0, false);
			for(size_t j = 0; j < m_minIters; j++)
			{
				// Shuffle the ratings
				pClone->shuffle(m_rand);

				// Do an epoch of training
				m_pModel->setLearningRate(learningRate);
				for(size_t i = 0; i < pClone->rows(); i++)
				{
					// Forward-prop
					double* vec = pClone->row(i);
					size_t user = size_t(vec[1]);
					size_t item = size_t(vec[0]);
					double* pPrefs = m_pUsers->row(user);
					pNN->forwardPropSingleOutput(pPrefs, item);

					// Update weights
					pNN->backpropagateSingleOutput(item, vec[2]);
					if(pass < 2)
						pNN->scaleWeightsSingleOutput(item, 1.0 - (learningRate * m_regularizer));
					if(pass != 1)
						pNN->gradientOfInputsSingleOutput(item, pPrefGradient);
					pNN->descendGradientSingleOutput(item, pPrefs, learningRate, pNN->momentum());
					if(pass != 1)
					{
						// Update inputs
						if(pass == 0)
							WVec::multiply(pPrefs, 1.0 - (learningRate * m_regularizer), m_intrinsicDims);
						WVec::addScaled(pPrefs, -learningRate, pPrefGradient, m_intrinsicDims);
					}
				}
			}

			// Stopping criteria
			double rmse = sqrt(validate(pNN, *pClone));
			if(rmse >= 1e-12 && 1.0 - (rmse / prevErr) >= 0.001) {} else // this awkward if/else structure causes "nan" values to be handled in a useful way
			{
				if(rmse <= prevErr) {} else // this awkward if/else structure causes "nan" values to be handled in a useful way
				{
					pNN->copyWeights(&backupNet);
					m_pUsers->copyBlock(backupUsers, 0, 0, INVALID_INDEX, m_intrinsicDims, 0, 0, false);
				}
				learningRate *= m_decayRate; // decay the learning rate
			}
			prevErr = rmse;
		}
	}
	//Insert neighbors here after 0 out all of their indexes
	m_neighbors = new WKdTree(m_itemAttrs, m_numNeighbors);
}

// virtual
double WHybridNonlinearPCA::predict(size_t item, size_t user)
{
	//The user and item are reversed (the item is the user and user is the item)
	//If the user is new
	if(user >= m_pUsers->rows() || item >= m_items)
		return 0.0;


	//If the item has not yet been rated but has item features
	if(m_itemSet.find(user) == m_itemSet.end())
	{
		//find closest instances
		double* vec = m_pUsers->row(user);
		WTEMPBUF(double, features, m_pUsers->cols() - m_intrinsicDims + 1);
		features[0] = 0;
		for(size_t i = m_intrinsicDims; i < m_pUsers->cols(); i++)
		{
			features[i-m_intrinsicDims+1] = vec[i] / 0.01;
		}
		WTEMPBUF(size_t, neighbors, m_numNeighbors);
		WTEMPBUF(double, distances, m_numNeighbors);
		m_neighbors->neighbors(neighbors, distances, features);

		//sort the neighbors based on distance
		m_neighbors->sortNeighbors(m_numNeighbors, neighbors, distances);
/// Commentted out portion is for returning a weighted average instead of a weighted mode
//		double sum =0;
//		double denom = 0;

		size_t counts [11];
		WIndexVec::setAll(counts, 0, 11);
		size_t i = 0;
		while(distances[i] == 0 && i < m_numNeighbors)
		{
			size_t neighbor = m_itemMap[neighbors[i]];
			if(m_pRatingCount[neighbor] > 50)
			{
				double prediction = round(((m_pMaxs[item] - m_pMins[item]) * m_pModel->forwardPropSingleOutput(m_pUsers->row(neighbor), item) + m_pMins[item])* 2.0) / 2.0;
//			double prediction = (m_pMaxs[item] - m_pMins[item]) * m_pModel->forwardPropSingleOutput(m_pUsers->row(neighbor), item) + m_pMins[item];
				if(prediction > m_pMaxs[item])
					prediction = m_pMaxs[item];
				if(prediction <  m_pMins[item])
					prediction =  m_pMins[item];
				counts[(size_t) (prediction * 2)] += m_pRatingCount[neighbor];
			}
			i++;

		}
//		return sum / denom;
		size_t predMode = 0;
		for(size_t j = 1; j < 11; j++)
		{
			if(counts[j] > counts[predMode])
				predMode = j;
		}
		return predMode / 2.0;
	}

	return (m_pMaxs[item] - m_pMins[item]) * m_pModel->forwardPropSingleOutput(m_pUsers->row(user), item) + m_pMins[item];
}

double WHybridNonlinearPCA::validate(WNeuralNet* pNN, WMatrix& data)
{
	double sse = 0;
	for(size_t i = 0; i < data.rows(); i++)
	{
		double* vec = data[i];
		double* pPrefs = m_pUsers->row(size_t(vec[1]));
		double predictedRating = pNN->forwardPropSingleOutput(pPrefs, size_t(vec[0]));
		double d = vec[2] - predictedRating;
		sse += (d * d);
	}
	return sse / data.rows();
}

void WHybridNonlinearPCA::setItemAttributes(WMatrix& itemAttrs)
{
	delete(m_itemAttrs);
	m_itemAttrs = new WMatrix();
	m_itemAttrs->copy(&itemAttrs);
	delete(m_itemMax);
	m_itemMax = new double[m_itemAttrs->cols()-1];
	delete(m_itemMin);
	m_itemMin = new double[m_itemAttrs->cols()-1];
	WVec::setAll(m_itemMin, 1e200, m_itemAttrs->cols() - 1);
	WVec::setAll(m_itemMax, -1e200, m_itemAttrs->cols() - 1);

	//Normalize the item attributes
	for(size_t i = 1; i < m_itemAttrs->cols(); i++)
	{
		if (m_itemAttrs->relation().areContinuous(i, 1))
		{
			m_itemMax[i-1] = m_itemAttrs->columnMax(i);
			m_itemMin[i-1] = m_itemAttrs->columnMin(i);
			if (m_itemMax[i-1] > 1 || m_itemMin[i-1] < 0)
				m_itemAttrs->normalizeColumn(i, m_itemMax[i-1], m_itemMin[i-1]);
		}
	}
}

*/




/*
WNonlinearPCA::WNonlinearPCA(size_t intrinsicDims)
: WCollaborativeFilter(), m_intrinsicDims(intrinsicDims), m_items(0), m_pMins(NULL), m_pMaxs(NULL), m_useInputBias(true), m_useThreePass(true), m_minIters(1), m_decayRate(0.97), m_regularizer(0.0001)
{
	m_pModel = new WNeuralNet();
	m_pUserMask = NULL;
	m_pItemMask = NULL;
	m_pUsers = NULL;
}

WNonlinearPCA::WNonlinearPCA(const WDomNode* pNode, WLearnerLoader& ll)
: WCollaborativeFilter(pNode, ll)
{
	m_useInputBias = pNode->getBool("uib");
	m_pUsers = new WMatrix(pNode->get("users"));
	m_pModel = new WNeuralNet(pNode->get("model"), ll);
	WDomNode* pUserMask = pNode->getIfExists("usermask");
	if(pUserMask)
		m_pUserMask = new WMatrix(pUserMask);
	else
		m_pUserMask = NULL;
	WDomNode* pItemMask = pNode->getIfExists("itemmask");
	if(pItemMask)
		m_pItemMask = new WMatrix(pItemMask);
	else
		m_pItemMask = NULL;
	m_items = m_pModel->outputLayer().outputs();
	m_pMins = new double[m_items];
	WDomListIterator it1(pNode->get("mins"));
	if(it1.remaining() != m_items)
		throw Ex("invalid number of elements");
	WVec::deserialize(m_pMins, it1);
	m_pMaxs = new double[m_items];
	WDomListIterator it2(pNode->get("maxs"));
	if(it2.remaining() != m_items)
		throw Ex("invalid number of elements");
	WVec::deserialize(m_pMaxs, it2);
	m_intrinsicDims = m_pModel->layer(0).inputs();
}

// virtual
WNonlinearPCA::~WNonlinearPCA()
{
	delete[] m_pMins;
	delete[] m_pMaxs;
	delete(m_pModel);
	delete(m_pUsers);
	delete(m_pUserMask);
	delete(m_pItemMask);
}

// virtual
WDomNode* WNonlinearPCA::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WNonlinearPCA");
	pNode->add(pDoc, "uib", pDoc->newBool(m_useInputBias));
	pNode->add(pDoc, "users", m_pUsers->serialize(pDoc));
	pNode->add(pDoc, "model", m_pModel->serialize(pDoc));
	if(m_pUserMask)
		pNode->add(pDoc, "usermask", m_pUserMask->serialize(pDoc));
	if(m_pItemMask)
		pNode->add(pDoc, "itemmask", m_pItemMask->serialize(pDoc));
	size_t itemCount = m_pModel->outputLayer().outputs();
	pNode->add(pDoc, "mins", WVec::serialize(pDoc, m_pMins, itemCount));
	pNode->add(pDoc, "maxs", WVec::serialize(pDoc, m_pMaxs, itemCount));
	return pNode;
}

double WNonlinearPCA::validate(WNeuralNet* pNN, WMatrix& data)
{
	double sse = 0;
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data[i];
		WVec& prefs = m_pUsers->row(size_t(vec[0]));
		double predictedRating = pNN->forwardPropSingleOutput(prefs, size_t(vec[1]));
		double d = vec[2] - predictedRating;
		sse += (d * d);
	}
	return sse / data.rows();
}

void WNonlinearPCA::clampUserElement(size_t user, size_t attr, double val)
{
val *= 0.01;
	if(attr >= m_intrinsicDims - (m_useInputBias ? 1 : 0))
		throw Ex("out of range");
	if(!m_pUserMask)
		m_pUserMask = new WMatrix(0, m_intrinsicDims - (m_useInputBias ? 1 : 0));
	while(m_pUserMask->rows() <= user)
		WVec::setAll(m_pUserMask->newRow(), UNKNOWN_REAL_VALUE, m_intrinsicDims - (m_useInputBias ? 1 : 0));
	m_pUserMask->row(user)[attr] = val;
}

void WNonlinearPCA::clampItemElement(size_t item, size_t attr, double val)
{
val *= 0.01;
	if(attr >= m_pModel->outputLayer().inputs())
		throw Ex("out of range");
	if(!m_pItemMask)
		m_pItemMask = new WMatrix(0, m_pModel->outputLayer().inputs());
	while(m_pItemMask->rows() <= item)
		WVec::setAll(m_pItemMask->newRow(), UNKNOWN_REAL_VALUE, m_pModel->outputLayer().inputs());
	m_pItemMask->row(item)[attr] = val;
}

void WNonlinearPCA::clampUsers(const WMatrix& data, size_t offset)
{
	size_t vals = data.cols() - 1;
	for(size_t i = 0; i < data.rows(); i++)
	{
		const WVec& row = data[i];
		size_t index = (size_t)*row;
		row++;
		for(size_t j = 0; j < vals; j++)
			clampUserElement(index, offset + j, *(row++));
	}
}

void WNonlinearPCA::clampItems(const WMatrix& data, size_t offset)
{
	size_t vals = data.cols() - 1;
	for(size_t i = 0; i < data.rows(); i++)
	{
		const WVec& row = data[i];
		size_t index = (size_t)*row;
		row++;
		for(size_t j = 0; j < vals; j++)
			clampItemElement(index, offset + j, *(row++));
	}
}

void WNonlinearPCA::clampUsersInternal(size_t i)
{
	WVec& profile = m_pUsers->row(i) + (m_useInputBias ? 1 : 0);
	const WVec& mask = m_pUserMask->row(i);
	for(size_t k = (m_useInputBias ? 1 : 0); k < m_intrinsicDims; k++)
	{
		if(*mask != UNKNOWN_REAL_VALUE)
			*profile = *mask;
		profile++;
		mask++;
	}
}

void WNonlinearPCA::clampItemsInternal(size_t i)
{
	WMatrix& itemWeights = ((WLayerClassic*)&m_pModel->outputLayer())->weights();
	const WVec& mask = m_pItemMask->row(i);
	size_t dims = m_pModel->outputLayer().inputs();
	for(size_t k = 0; k < dims; k++)
	{
		if(*mask != UNKNOWN_REAL_VALUE)
			itemWeights[k][i] = *mask;
		mask++;
	}
}


// virtual
void WNonlinearPCA::train(WMatrix& data)
{
	size_t usrs, items;
	WCollaborativeFilter_dims(data, &usrs, &items);
	m_items = items;

	// Copy and normalize the ratings
	WMatrix* pClone = new WMatrix();
	pClone->copy(&data);
	std::unique_ptr<WMatrix> hClone(pClone);
	delete[] m_pMins;
	m_pMins = new double[items];
	delete[] m_pMaxs;
	m_pMaxs = new double[items];
	WVec::setAll(m_pMins, 1e200, items);
	WVec::setAll(m_pMaxs, -1e200, items);
	for(size_t i = 0; i < pClone->rows(); i++)
	{
		WVec& vec = pClone->row(i);
		m_pMins[size_t(vec[1])] = std::min(m_pMins[size_t(vec[1])], vec[2]);
		m_pMaxs[size_t(vec[1])] = std::max(m_pMaxs[size_t(vec[1])], vec[2]);
	}
	for(size_t i = 0; i < items; i++)
	{
		if(m_pMins[i] >= 1e200)
			m_pMins[i] = 0.0;
		if(m_pMaxs[i] < m_pMins[i] + 1e-12)
			m_pMaxs[i] = m_pMins[i] + 1.0;
	}
	for(size_t i = 0; i < pClone->rows(); i++)
	{
		WVec&  vec = pClone->row(i);
		vec[2] = (vec[2] - m_pMins[size_t(vec[1])]) / (m_pMaxs[size_t(vec[1])] - m_pMins[size_t(vec[1])]);
	}

	// Prep the model for incremental training
	WUniformRelation featureRel(m_intrinsicDims);
	WUniformRelation labelRel(items);
	m_pModel->setUseInputBias(m_useInputBias);
	m_pModel->beginIncrementalLearning(featureRel, labelRel);
	WNeuralNet nn;
	nn.addLayer(new WLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE));
	nn.setUseInputBias(m_useInputBias);
	nn.beginIncrementalLearning(featureRel, labelRel);
	WVec& prefGradient = new double[m_intrinsicDims];
	std::unique_ptr<double[]> hPrefGradient(prefGradient);

	// Train
	size_t startPass = 0;
	if(!m_useThreePass)
		startPass = 2;
	else if(m_pModel->layerCount() == 1)
		startPass = 2;
	for(size_t pass = startPass; pass < 3; pass++)
	{
		WNeuralNet* pNN = (pass == 0 ? &nn : m_pModel);
		if(pass == startPass)
		{
			// Initialize the user matrix
			delete(m_pUsers);
			m_pUsers = new WMatrix(usrs, m_intrinsicDims);
			for(size_t i = 0; i < usrs; i++)
			{
				WVec& vec = m_pUsers->row(i);
				for(size_t j = 0; j < m_intrinsicDims; j++)
					*(vec++) = 0.01 * m_rand.normal();
				if(m_pUserMask && i < m_pUserMask->rows())
					clampUsersInternal(i);
			}
		}
		if(m_pItemMask)
		{
			for(size_t i = 0; i < m_pItemMask->rows(); i++)
				clampItemsInternal(i);
		}
		double rateBegin = 0.1;
		double rateEnd = 0.001;
		double prevErr = 1e10;
		for(double learningRate = rateBegin; learningRate > rateEnd; )
		{
			WNeuralNet backupNet;
			backupNet.copyStructure(pNN);
			WMatrix backupUsers(*m_pUsers);
			for(size_t j = 0; j < m_minIters; j++)
			{
				// Shuffle the ratings
				pClone->shuffle(m_rand);

				// Do an epoch of training
				m_pModel->setLearningRate(learningRate);
				for(size_t i = 0; i < pClone->rows(); i++)
				{
					// Forward-prop
					WVec& vec = pClone->row(i);
					size_t user = size_t(vec[0]);
					size_t item = size_t(vec[1]);
					WVec& prefs = m_pUsers->row(user);
					pNN->forwardPropSingleOutput(prefs, item);

					// Update weights
					pNN->backpropagateSingleOutput(item, vec[2]);
					if(pass < 2)
						pNN->scaleWeightsSingleOutput(item, 1.0 - (learningRate * m_regularizer));
					if(pass != 1)
						pNN->gradientOfInputsSingleOutput(item, prefGradient);
					pNN->descendGradientSingleOutput(item, prefs, learningRate, pNN->momentum());
					if(m_pItemMask && item < m_pItemMask->rows())
						clampItemsInternal(item);
					if(pass != 1)
					{
						// Update inputs
						if(pass == 0)
							WVec::multiply(prefs, 1.0 - (learningRate * m_regularizer), m_intrinsicDims);
						WVec::addScaled(prefs, -learningRate, prefGradient, m_intrinsicDims);
						if(m_pUserMask && user < m_pUserMask->rows())
							clampUsersInternal(user);
					}
				}
			}

			// Stopping criteria
			double rmse = sqrt(validate(pNN, *pClone));
			if(rmse >= 1e-12 && 1.0 - (rmse / prevErr) >= 0.001) {} else // this awkward if/else structure causes "nan" values to be handled in a useful way
			{
				if(rmse <= prevErr) {} else // this awkward if/else structure causes "nan" values to be handled in a useful way
				{
					pNN->copyWeights(&backupNet);
					m_pUsers->copyBlock(backupUsers, 0, 0, INVALID_INDEX, INVALID_INDEX, 0, 0, false);
				}
				learningRate *= m_decayRate; // decay the learning rate
			}
			prevErr = rmse;
		}
	}
}

// virtual
double WNonlinearPCA::predict(size_t user, size_t item)
{
	if(user >= m_pUsers->rows() || item >= m_items)
		return 0.0;
	else
		return (m_pMaxs[item] - m_pMins[item]) * m_pModel->forwardPropSingleOutput(m_pUsers->row(user), item) + m_pMins[item];
}

// virtual
void WNonlinearPCA::impute(WVec& vec, size_t dims)
{
	throw Ex("Sorry, WNonlinearPCA::impute is not implemented yet");
}

// static
void WNonlinearPCA::test()
{
	WNonlinearPCA rec(3);
	rec.model()->addLayer(new WLayerClassic(FLEXIBLE_SIZE, 3));
	rec.model()->addLayer(new WLayerClassic(3, FLEXIBLE_SIZE));
	rec.basicTest(0.18);
}
*/















WBagOfRecommenders::WBagOfRecommenders()
: WCollaborativeFilter(), m_itemCount(0)
{
}

WBagOfRecommenders::WBagOfRecommenders(const WDomNode* pNode, WLearnerLoader& ll)
: WCollaborativeFilter(pNode, ll)
{
	m_itemCount = (size_t)pNode->getInt("ic");
	for(WDomListIterator it(pNode->get("filters")); it.current(); it.advance())
		m_filters.push_back(ll.loadCollaborativeFilter(it.current()));
}

WBagOfRecommenders::~WBagOfRecommenders()
{
	clear();
}

void WBagOfRecommenders::clear()
{
	for(vector<WCollaborativeFilter*>::iterator it = m_filters.begin(); it != m_filters.end(); it++)
		delete(*it);
	m_filters.clear();
}

void WBagOfRecommenders::addRecommender(WCollaborativeFilter* pRecommender)
{
	pRecommender->rand().setSeed(m_rand.next()); // Ensure that each recommender has a different seed
	m_filters.push_back(pRecommender);
}

// virtual
void WBagOfRecommenders::train(WMatrix& data)
{
	for(vector<WCollaborativeFilter*>::iterator it = m_filters.begin(); it != m_filters.end(); it++)
	{
		// Make a matrix that randomly samples about half of the elements in pData
		WMatrix tmp(data.relation().clone());
		WReleaseDataHolder hTmp(&tmp);
		for(size_t i = 0; i < data.rows(); i++)
		{
			if(m_rand.next(2) == 0)
				tmp.takeRow(&data[i]);
		}

		// Train with it
		(*it)->train(tmp);
	}
}

// virtual
double WBagOfRecommenders::predict(size_t user, size_t item)
{
	double sum = 0.0;
	for(vector<WCollaborativeFilter*>::iterator it = m_filters.begin(); it != m_filters.end(); it++)
		sum += (*it)->predict(user, item);
	return sum / m_filters.size();
}

// virtual
void WBagOfRecommenders::impute(WVec& vec, size_t dims)
{
	throw Ex("Sorry, not implemented yet");
	/*
	size_t n = std::min(m_itemCount, dims);
	WTEMPBUF(double, pBuf1, n);
	WTEMPBUF(double, pBuf2, n);
	WVec::setAll(pBuf2, 0.0, n);
	double count = 0.0;
	for(vector<WCollaborativeFilter*>::iterator it = m_filters.begin(); it != m_filters.end(); it++)
	{
		WVec::copy(pBuf1, vec, n);
		(*it)->impute(pBuf1, dims);
		WVec::multiply(pBuf2, count / (count + 1), n);
		WVec::addScaled(pBuf2, 1.0 / (count + 1), pBuf1, n);
		count++;
	}
	size_t i;
	for(i = 0; i < n; i++)
	{
		if(*vec == UNKNOWN_REAL_VALUE)
			*vec = pBuf2[i];
		vec++;
	}
	for( ; i < dims; i++)
	{
		if(*vec == UNKNOWN_REAL_VALUE)
			*vec = 0.0;
		vec++;
	}
	*/
}

// virtual
WDomNode* WBagOfRecommenders::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBagOfRecommenders");
	pNode->add(pDoc, "ic", m_itemCount);
	WDomNode* pFilters = pNode->add(pDoc, "filters", pDoc->newList());
	for(vector<WCollaborativeFilter*>::const_iterator it = m_filters.begin(); it != m_filters.end(); it++)
		pFilters->add(pDoc, (*it)->serialize(pDoc));
	return pNode;
}

// static
void WBagOfRecommenders::test()
{
	WBagOfRecommenders rec;
	rec.addRecommender(new WBaselineRecommender());
	rec.addRecommender(new WMatrixFactorization(3));
	//	WNonlinearPCA* nlpca = new WNonlinearPCA(3);
	//	nlpca->model()->addLayer(new WLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE));
	//	rec.addRecommender(nlpca);
	rec.basicTest(0.69);
}






//virtual
WContentBasedFilter::~WContentBasedFilter()
{
	delete(m_itemAttrs);
	clear();
}

//virtual
void WContentBasedFilter::train(WMatrix& data)
{
	clear();
	m_userMap.clear();
	m_userRatings.clear();

	size_t users, items;
	WCollaborativeFilter_dims(data, &users, &items);
	m_items = items;
	m_users = users;
	std::set<size_t> userSet;


	if(m_itemAttrs == nullptr)
		throw Ex("The items attributes has to be set");

	//create a training set and learning algorithm for each user
	for(size_t i = 0; i < data.rows(); i++)
	{
		WVec& vec = data.row(i);
		m_userRatings.insert(std::make_pair((size_t)vec[0], (size_t)vec[1]));
		userSet.insert((size_t)vec[0]);
	}

	//Loop through the set of users
	for(std::set<size_t>::iterator it = userSet.begin(); it != userSet.end(); ++it)
	{
		m_args.set_pos(m_init_pos);
		pair<multimap<size_t, size_t>::iterator, multimap<size_t, size_t>::iterator> ratedItems;
		ratedItems = m_userRatings.equal_range(*it);

		//create the training data for the user
		WMatrix* trainingData = new WMatrix(m_itemAttrs->relation().clone());
		WRelation* relation = data.relation().cloneSub(data.cols() - 1, 1);
		WMatrix* labels = new WMatrix(relation);
		for(multimap<size_t, size_t>::iterator ratings = ratedItems.first; ratings != ratedItems.second; ++ratings)
		{
			trainingData->newRow().copy(m_itemAttrs->row(m_itemMap[(*ratings).second]));

			WVec& temp = labels->newRow();
			temp[0] = data[(*ratings).second][2];
		}

		//train a learning algorithm for each user
		WSupervisedLearner* pLearn = (WSupervisedLearner*)WLearnerLib::InstantiateAlgorithm(m_args, trainingData, labels);
		if(m_args.size() > 0)
			throw Ex("Superfluous argument: ", m_args.peek());
		pLearn->train(*trainingData, *labels);
		m_userMap[(*it)] = m_learners.size();
		m_learners.push_back(pLearn);
	}
}

//virtual
double WContentBasedFilter::predict(size_t user, size_t item)
{
	if(user >= m_users || item >= m_items)
		return 0.0;
	WVec pOut(1);
	m_learners[m_userMap[user]]->predict(m_itemAttrs->row(m_itemMap[item]), pOut);
	return pOut[0];
}

//virtual
void WContentBasedFilter::impute(WVec& vec, size_t dims)
{
	/*
	for(size_t i = 0; i < dims; i++)
	{
		if(*vec == UNKNOWN_REAL_VALUE)
			(*vec) = m_learners[]
	}
*/
	std::cerr << "Not yet implemented\n";
}

//virtual
WDomNode* WContentBasedFilter::serialize(WDom* pDoc) const
{
	return nullptr;
}

void WContentBasedFilter::clear()
{
	//		for(vector<WSupervisedLearner*>::iterator it = m_learners.begin(); it != m_learners.end(); it++)
	//				delete(*it);
	m_learners.clear();
}

void WContentBasedFilter::setItemAttributes(WMatrix& itemAttrs)
{
	delete(m_itemAttrs);
	m_itemAttrs = new WMatrix(itemAttrs);
	for(size_t i = 0; i < m_itemAttrs->rows(); i++)
	{
		WVec& vec = m_itemAttrs->row(i);
		m_itemMap[(size_t)vec[0]] = i;
	}
	m_itemAttrs->swapColumns(0, m_itemAttrs->cols() - 1);
	m_itemAttrs->deleteColumns(m_itemAttrs->cols() - 1, 1);
}






WContentBoostedCF::WContentBoostedCF(WArgReader copy)
: WCollaborativeFilter(), m_ratingCounts(nullptr), m_pseudoRatingSum(nullptr)
{
	int orig_argc = copy.get_argc();
	int orig_pos = copy.get_pos();
	while(strcmp(copy.pop_string(), "--") != 0)
		if(copy.size() == 0)
			throw Ex("Expecting \"--\" to denote the parameters for the instance-based CF\n");
	int dashLoc = copy.get_pos() - 1;
	copy.set_argc(dashLoc);
	copy.set_pos(orig_pos);
	m_cbf = WRecommenderLib::InstantiateContentBasedFilter(copy);
	copy.set_pos(dashLoc + 1);
	copy.set_argc(orig_argc);
	m_cf = WRecommenderLib::InstantiateInstanceRecommender(copy);
}

WContentBoostedCF::~WContentBoostedCF()
{
	delete(m_cbf);
	delete(m_cf);
	delete[] m_ratingCounts;
	delete[] m_pseudoRatingSum;
}

void WContentBoostedCF::train(WMatrix& data)
{
	//make a copy of the training data
	WMatrix* pClone = new WMatrix(data);
	std::unique_ptr<WMatrix> hClone(pClone);
	m_cbf->train(*pClone);

	//Create the psuedo user-ratings vector for every user
	m_userMap = m_cbf->getUserMap();
	map<size_t, size_t> items = m_cbf->getItemMap();
	multimap<size_t, size_t> userRatings = m_cbf->getUserRatings();
	delete[] m_ratingCounts;
	delete[] m_pseudoRatingSum;
	m_ratingCounts = new size_t[m_userMap.size()];
	WIndexVec::setAll(m_ratingCounts, 0, m_userMap.size());

	m_pseudoRatingSum = new double[m_userMap.size()];
	WVecWrapper vw(m_pseudoRatingSum, m_userMap.size());
	vw.fill(0.0);

	for(size_t i = 0; i < pClone->rows(); i++)
	{
		WVec& vec = pClone->row(i);
		m_pseudoRatingSum[m_userMap[(size_t)vec[0]]] += vec[2];
	}

	//Loop through all of the users
	for(map<size_t, size_t>::iterator user = m_userMap.begin(); user != m_userMap.end(); ++user)
	{
		pair<multimap<size_t, size_t>::iterator, multimap<size_t, size_t>::iterator> ratings;
		ratings = userRatings.equal_range(user->first);

		//Loop through all of the items
		for(map<size_t, size_t>::iterator item = items.begin(); item != items.end(); ++item)
		{
			//Check if user has rated item
			bool isRated = false;
			multimap<size_t, size_t>::iterator rating;
			for(rating = ratings.first; rating != ratings.second; ++rating)
			{
				if(rating->second == item->first)
					isRated = true;
				m_ratingCounts[m_userMap[user->first]]++;
			}
			if(!isRated)
			{
				//make prediction
				WVec& rat = pClone->newRow();
				rat[0] = (double)user->first;
				rat[1] = (double)item->first;
				rat[2] = m_cbf->predict(user->first, item->first);
				WAssert(rat[2] != UNKNOWN_REAL_VALUE);
				m_pseudoRatingSum[m_userMap[user->first]] += rat[2];
			}
		}
	}

	//Train CF on the psuedo user-ratings
	m_cf->train(*pClone);
	m_cf->clearUserDEPQ();
}

double WContentBoostedCF::predict(size_t user, size_t item)
{
	double max = 2;
	multimap<double, ArrayWrapper> neighbors = m_cf->getNeighbors(user, item);

	// Combine the ratings of the nearest neighbors to make a prediction
	size_t num = m_ratingCounts[m_userMap[user]];
	double selfWeight = (num > 50) ? 1.0 : num / 50.0;
	double weighted_sum = max * selfWeight * (m_cbf->predict(user, item)); // - (m_pseudoRatingSum[m_userMap[user]] / m_ratingCounts[m_userMap[user]]));
	double sum_weight = max * selfWeight;
	for(multimap<double, ArrayWrapper>::iterator it = neighbors.begin(); it != neighbors.end(); it++)
	{
		double weight = std::max(0.0, std::min(1.0, it->first));
		size_t neighNum = m_ratingCounts[m_userMap[(size_t)it->first]];
		double neighWeight = (neighNum > 50) ? 1.0 : neighNum / 50.0;
		double sigWeight = (it->second.values[1] > 50) ? 1.0 : it->second.values[1] / 50.0;
		weight *= ((2 * selfWeight * neighWeight) / (selfWeight + neighWeight)) + sigWeight;
		double val = m_cf->getRating(it->second.values[0], item);
		weighted_sum += weight * val;
		sum_weight += weight;
	}

	//	return (m_pseudoRatingSum[m_userMap[user]] / m_ratingCounts[m_userMap[user]]) + (weighted_sum / sum_weight);
	return weighted_sum / sum_weight;
}

void WContentBoostedCF::impute(WVec& vec, size_t dims)
{
	std::cerr << "Not yet implemented\n";
}




} // namespace WClasses
