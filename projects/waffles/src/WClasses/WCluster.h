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
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#ifndef __GCLUSTER_H__
#define __GCLUSTER_H__

#include "WTransform.h"
#include "WLearner.h"
#include <deque>

namespace WClasses
{

class WDistanceMetric;
class WSparseSimilarity;

/// The base class for clustering algorithms. Classes that inherit from this
/// class must implement a method named "cluster" which performs clustering, and
/// a method named "whichCluster" which reports which cluster the specified row
/// is determined to be a member of.
class WClusterer : public WTransform
{
protected:
	size_t m_clusterCount;
	WDistanceMetric* m_pMetric;
	bool m_ownMetric;

public:
	WClusterer(size_t nClusterCount);
	~WClusterer() override;

	/// If own is true, then this object will delete pMetric when it is destroyed.
	void setMetric(WDistanceMetric* pMetric, bool own);

	/// Return the number of clusters
	size_t clusterCount() { return m_clusterCount; }

	/// Clusters pIn and outputs a dataset with one column that specifies
	/// the cluster number for each row.
	WMatrix* reduce(const WMatrix& in) override
	{
		cluster(&in);
		WMatrix* pOut = new WMatrix(new WUniformRelation(1, m_clusterCount));
		size_t nCount = in.rows();
		pOut->newRows(nCount);
		for(size_t i = 0; i < nCount; i++)
			pOut->row(i)[0] = (double)whichCluster(i);
		return pOut;
	}

	/// Performs clustering.
	virtual void cluster(const WMatrix* pData) = 0;

	/// Reports which cluster the specified row is a member of.
	virtual size_t whichCluster(size_t nVector) = 0;
};



/// This is a base class for clustering algorithms that operate on sparse matrices
class WSparseClusterer
{
protected:
	size_t m_clusterCount;
	WSparseSimilarity* m_pMetric;
	bool m_ownMetric;

public:
	WSparseClusterer(size_t clusterCount);
	virtual ~WSparseClusterer();

	/// Return the number of clusters
	size_t clusterCount() { return m_clusterCount; }

	/// Perform clustering.
	virtual void cluster(WSparseMatrix* pData) = 0;

	/// Report which cluster the specified row is a member of.
	virtual size_t whichCluster(size_t nVector) = 0;

	/// If own is true, then this takes ownership of pMetric
	void setMetric(WSparseSimilarity* pMetric, bool own);
};




/// This merges each cluster with its closest neighbor. (The distance between
/// clusters is computed as the distance between the closest members of the
/// clusters times (n^b), where n is the total number of points from both
/// clusters, and b is a balancing factor.
class WAgglomerativeClusterer : public WClusterer
{
protected:
	size_t* m_pClusters;

public:
	WAgglomerativeClusterer(size_t nClusterCount);
	~WAgglomerativeClusterer() override;

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Performs clustering
	void cluster(const WMatrix* pData) override;

	/// Identifies the cluster of the specified row
	size_t whichCluster(size_t nVector) override;
};


/// This is a semi-supervised agglomerative clusterer. It can only handle
/// one output, and it must be nominal. All inputs must be continuous. Also,
/// it assumes that all output values are represented in the training set.
class WAgglomerativeTransducer : public WTransducer
{
protected:
	WDistanceMetric* m_pMetric;
	bool m_ownMetric;

public:
	WAgglomerativeTransducer();
	~WAgglomerativeTransducer() override;

	/// Specify the metric to use to determine the distance between points.
	/// If own is true, then this object will take care to delete pMetric.
	void setMetric(WDistanceMetric* pMetric, bool own);

	/// This model has no parameters to tune, so this method is a noop.
	void autoTune(WMatrix& features, WMatrix& labels);

protected:
	/// See the comment for WTransducer::transduce.
	/// Throws if labels1 has more than one column.
	std::unique_ptr<WMatrix> transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2) override;

	/// See the comment for WTransducer::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override { return false; }
};


/// An implementation of the K-means clustering algorithm.
class WKMeans : public WClusterer
{
protected:
	WMatrix* m_pCentroids;
	size_t* m_pClusters;
	size_t m_reps;
	WRand* m_pRand;

public:
	WKMeans(size_t nClusters, WRand* pRand);
	~WKMeans() override;

	/// Performs clustering
	void cluster(const WMatrix* pData) override;

	/// Identifies the cluster of the specified row
	size_t whichCluster(size_t nVector) override;

	/// Selects random centroids and initializes internal data structures
	void init(const WMatrix* pData);

	/// Assigns each row to the cluster of the nearest centroid as measured
	/// with the dissimilarity metric. Returns the sum-squared-distance of each row with its centroid.
	double assignClusters(const WMatrix* pData);

	/// Computes new centroids for each cluster.
	void recomputeCentroids(const WMatrix* pData);

	/// Returns a k x d matrix, where each row is one of the k centroids.
	WMatrix* centroids() { return m_pCentroids; }

	/// Specify the number of times to cluster the data. The best clustering (as measured
	/// by the sum-squared-difference between each point and its cluster-centroid) will be kept.
	void setReps(size_t r) { m_reps = r; }

protected:
	bool clusterAttempt(size_t nMaxIterations);
	bool selectSeeds(const WMatrix* pSeeds);
};


/// A K-means clustering algorithm where every point has partial membership in each cluster.
/// This algorithm is specified in Li, D. and Deogun, J. and Spaulding, W. and Shuart, B.,
/// Towards missing data imputation: A study of fuzzy K-means clustering method, In Rough Sets
/// and Current Trends in Computing, Springer, pages 573--579, 2004.
class WFuzzyKMeans : public WClusterer
{
protected:
	WMatrix* m_pCentroids;
	WMatrix* m_pWeights;
	double m_fuzzifier;
	size_t m_reps;
	WRand* m_pRand;

public:
	WFuzzyKMeans(size_t nClusters, WRand* pRand);
	~WFuzzyKMeans() override;

	/// Performs clustering
	void cluster(const WMatrix* pData) override;

	/// Identifies the cluster of the specified row
	size_t whichCluster(size_t nVector) override;

	/// Selects random centroids and initializes internal data structures
	void init(const WMatrix* pData);

	/// Assigns each row to partial membership in each cluster, as measured
	/// with the dissimilarity metric. Returns the weighted-sum-distance of each row with the centroids.
	double recomputeWeights(const WMatrix* pData);

	/// Computes new centroids for each cluster.
	void recomputeCentroids(const WMatrix* pData);

	/// Returns a k x d matrix, where each row is one of the k centroids.
	WMatrix* centroids() { return m_pCentroids; }

	/// Specifies how fuzzy the membership in each cluster should be. d should be
	/// greater than 1, and is typically about 1.3.
	void setFuzzifier(double d) { m_fuzzifier = d; }

	/// Specify the number of times to cluster the data. The best clustering (as measured
	/// by the weighted-sum-difference between each point with the centroids) will be kept.
	void setReps(size_t r) { m_reps = r; }

protected:
	bool clusterAttempt(size_t nMaxIterations);
	bool selectSeeds(const WMatrix* pSeeds);
};


/// An implementation of the K-medoids clustering algorithm
class WKMedoids : public WClusterer
{
protected:
	size_t* m_pMedoids;
	double m_d;
	const WMatrix* m_pData;

public:
	WKMedoids(size_t clusters);
	~WKMedoids() override;

	/// Performs clustering
	void cluster(const WMatrix* pData) override;

	/// Identifies the cluster of the specified row
	size_t whichCluster(size_t nVector) override;

protected:
	double curErr(const WMatrix* pData);
};



/// An implementation of the K-medoids clustering algorithm for sparse data
class WKMedoidsSparse : public WSparseClusterer
{
protected:
	size_t* m_pMedoids;
	double m_d;
	WSparseMatrix* m_pData;

public:
	WKMedoidsSparse(size_t clusters);
	~WKMedoidsSparse() override;

	/// Performs clustering
	void cluster(WSparseMatrix* pData) override;

	/// Identifies the cluster of the specified row
	size_t whichCluster(size_t nVector) override;

protected:
	double curGoodness();
};



/// An implementation of the K-means clustering algorithm.
class WKMeansSparse : public WSparseClusterer
{
protected:
	size_t m_nDims;
	size_t m_nClusters;
	size_t* m_pClusters;
	WRand* m_pRand;

public:
	WKMeansSparse(size_t nClusters, WRand* pRand);
	~WKMeansSparse() override;

	/// Performs clustering
	void cluster(WSparseMatrix* pData) override;

	/// Identifies the cluster of the specified row
	size_t whichCluster(size_t nVector) override;
};



/// A transduction algorithm that uses a max-flow/min-cut graph-cut algorithm
/// to partition the data until each class is in a separate cluster. Unlabeled points
/// are then assigned the label of the cluster in which they fall.
class WGraphCutTransducer : public WTransducer
{
protected:
	size_t m_neighborCount;

public:
	WGraphCutTransducer();
	~WGraphCutTransducer() override;

	/// Sets the number of neighbors to use to form the graph. The default is 12
	void setNeighbors(size_t k);

	/// Returns the number of neighbors to which each point is connected
	size_t neighbors() { return m_neighborCount; }

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(WMatrix& features, WMatrix& labels);

protected:
	/// See the comment for WTransducer::transduce.
	/// Only supports one-dimensional labels.
	std::unique_ptr<WMatrix> transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2) override;

	/// See the comment for WTransducer::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override { return false; }
};


} // namespace WClasses

#endif // __GCLUSTER_H__
