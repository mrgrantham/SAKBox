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
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include "WDistance.h"
#include "WDom.h"
#include "WVec.h"
#include <math.h>
#include <cassert>
#include <memory>

using std::map;

namespace WClasses
{

WDistanceMetric::WDistanceMetric(WDomNode* pNode)
: m_scaleFactors(pNode->get("scalars"))
{
	m_pRelation = WRelation::deserialize(pNode->get("relation"));
	if(m_pRelation->size() != m_scaleFactors.size())
		throw Ex("Mismatching sizes");
	m_ownRelation = true;
}

// virtual
WDistanceMetric::~WDistanceMetric()
{
	setRelation(nullptr, false);
}

void WDistanceMetric::setRelation(const WRelation* pRelation, bool own)
{
	if(m_ownRelation)
		delete((WRelation*)m_pRelation);
	m_pRelation = pRelation;
	m_ownRelation = own;
	if(pRelation)
	{
		m_scaleFactors.resize(pRelation->size());
		m_scaleFactors.fill(1.0);
	}
	else
		m_scaleFactors.resize(0);
}

WDomNode* WDistanceMetric::baseDomNode(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "class", name());
	pNode->add(pDoc, "relation", m_pRelation->serialize(pDoc));
	pNode->add(pDoc, "scalars", m_scaleFactors.serialize(pDoc));
	return pNode;
}

// static
WDistanceMetric* WDistanceMetric::deserialize(WDomNode* pNode)
{
	const std::string className = pNode->getClassName();
	const char* szClass = className.c_str();
	if(strcmp(szClass, "WRowDistance") == 0)
		return new WRowDistance(pNode);
	if(strcmp(szClass, "WDenseCosineDistance") == 0)
		return new WDenseCosineDistance(pNode);
	if(strcmp(szClass, "WLNormDistance") == 0)
		return new WLNormDistance(pNode);
	if(strcmp(szClass, "WKernelDistance") == 0)
		return new WKernelDistance(pNode);
	throw Ex("Unrecognized class: ", szClass);
	return nullptr;
}

void WDistanceMetric_exerciseMetric(WDistanceMetric& metric)
{
	// Make two vectors
	WVec a(2);
	a.fill(1.0);
	WVec b(2);
	b.fill(2.0);
	WUniformRelation rel(2);
	metric.init(&rel, false);

	// Measure distance
	double sqDist = metric.squaredDistance(a, b);

	// Roundtrip through serialization
	WDom doc;
	WDomNode* pNode = metric.serialize(&doc);
	WDistanceMetric* pRoundtripped = WDistanceMetric::deserialize(pNode);
	std::unique_ptr<WDistanceMetric> hRoundtripped(pRoundtripped);

	// Make sure the roundtripped metric still returns the same result
	double sqDist2 = pRoundtripped->squaredDistance(a, b);
	if(std::abs(sqDist - sqDist2) > 1e-12)
		throw Ex("failed");
}

// static
void WDistanceMetric::test()
{
	WRowDistance d1;
	WDistanceMetric_exerciseMetric(d1);
	WLNormDistance d2(1.4);
	WDistanceMetric_exerciseMetric(d2);
	WDenseCosineDistance d3;
	WDistanceMetric_exerciseMetric(d3);
	WKernelDistance d4(WKernel::kernelComplex1(), true);
	WDistanceMetric_exerciseMetric(d4);
}

// --------------------------------------------------------------------

WRowDistance::WRowDistance()
: WDistanceMetric(), m_diffWithUnknown(1.0)
{
}

WRowDistance::WRowDistance(WDomNode* pNode)
: WDistanceMetric(pNode)
{
	m_diffWithUnknown = pNode->getDouble("dwu");
}

// virtual
WDomNode* WRowDistance::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	pNode->add(pDoc, "dwu", m_diffWithUnknown);
	return pNode;
}

// virtual
void WRowDistance::init(const WRelation* pRelation, bool own)
{
	setRelation(pRelation, own);
}

// virtual
double WRowDistance::squaredDistance(const WVec& a, const WVec& b) const
{
	if(a.size() != m_pRelation->size() || b.size() != m_pRelation->size())
		throw Ex("unexpected size");
	double sum = 0;
	size_t count = m_pRelation->size();
	double d;
	for(size_t i = 0; i < count; i++)
	{
		if(m_pRelation->valueCount(i) == 0)
		{
			if(a[i] == UNKNOWN_REAL_VALUE || b[i] == UNKNOWN_REAL_VALUE)
				d = m_diffWithUnknown;
			else
				d = (b[i] - a[i]) * m_scaleFactors[i];
		}
		else
		{
			if((int)a[i] == UNKNOWN_DISCRETE_VALUE || (int)b[i] == UNKNOWN_DISCRETE_VALUE)
				d = m_diffWithUnknown;
			else
				d = ((int)b[i] == (int)a[i] ? 0 : 1);
		}
		sum += (d * d);
	}
	return sum;
}

// --------------------------------------------------------------------

WLNormDistance::WLNormDistance(double norm)
: WDistanceMetric(), m_norm(norm), m_diffWithUnknown(1.0)
{
}

WLNormDistance::WLNormDistance(WDomNode* pNode)
: WDistanceMetric(pNode), m_norm(pNode->getDouble("norm")), m_diffWithUnknown(pNode->getDouble("dwu"))
{
}

// virtual
WDomNode* WLNormDistance::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	pNode->add(pDoc, "norm", m_norm);
	pNode->add(pDoc, "dwu", m_diffWithUnknown);
	return pNode;
}

// virtual
void WLNormDistance::init(const WRelation* pRelation, bool own)
{
	setRelation(pRelation, own);
}

// virtual
double WLNormDistance::squaredDistance(const WVec& a, const WVec& b) const
{
	if(a.size() != m_pRelation->size() || b.size() != m_pRelation->size())
		throw Ex("unexpected size");
	double sum = 0;
	size_t count = m_pRelation->size();
	double d;
	for(size_t i = 0; i < count; i++)
	{
		if(m_pRelation->valueCount(i) == 0)
		{
			if(a[i] == UNKNOWN_REAL_VALUE || b[i] == UNKNOWN_REAL_VALUE)
				d = m_diffWithUnknown;
			else
				d = (b[i] - a[i]) * m_scaleFactors[i];
		}
		else
		{
			if((int)a[i] == UNKNOWN_DISCRETE_VALUE || (int)b[i] == UNKNOWN_DISCRETE_VALUE)
				d = 1;
			else
				d = ((int)b[i] == (int)a[i] ? 0 : 1);
		}
		sum += pow(fabs(d), m_norm);
	}
	d = pow(sum, 1.0 / m_norm);
	return (d * d);
}

// --------------------------------------------------------------------

WDenseCosineDistance::WDenseCosineDistance()
: WDistanceMetric()
{
}

WDenseCosineDistance::WDenseCosineDistance(WDomNode* pNode)
: WDistanceMetric(pNode)
{
}

// virtual
WDomNode* WDenseCosineDistance::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	return pNode;
}

// virtual
void WDenseCosineDistance::init(const WRelation* pRelation, bool own)
{
	setRelation(pRelation, own);
}

// virtual
double WDenseCosineDistance::squaredDistance(const WVec& a, const WVec& b) const
{
	if(a.size() != m_pRelation->size() || b.size() != m_pRelation->size())
		throw Ex("unexpected size");
	double sum_sq_a = 0.0;
	double sum_sq_b = 0.0;
	double sum_co_prod = 0.0;
	for(size_t i = 0; i < a.size(); i++)
	{
		sum_sq_a += (a[i] * a[i]);
		sum_sq_b += (b[i] * b[i]);
		sum_co_prod += (a[i] * b[i]);
	}
	double denom = sqrt(sum_sq_a * sum_sq_b);
	if(denom > 0.0)
		return 1.0 - sum_co_prod / denom;
	else
		return 1.0;
}

// --------------------------------------------------------------------

WKernelDistance::WKernelDistance(WKernel* pKernel, bool own)
: WDistanceMetric(), m_pKernel(pKernel), m_ownKernel(own)
{
}

WKernelDistance::WKernelDistance(WDomNode* pNode)
: WDistanceMetric(pNode), m_ownKernel(true)
{
	m_pKernel = WKernel::deserialize(pNode->get("kernel"));
}

// virtual
WKernelDistance::~WKernelDistance()
{
	if(m_ownKernel)
		delete(m_pKernel);
}

// virtual
WDomNode* WKernelDistance::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc);
	pNode->add(pDoc, "kernel", m_pKernel->serialize(pDoc));
	return pNode;
}

// virtual
void WKernelDistance::init(const WRelation* pRelation, bool own)
{
	setRelation(pRelation, own);
}

// virtual
double WKernelDistance::squaredDistance(const WVec& a, const WVec& b) const
{
	return 1.0 - m_pKernel->apply(a, b);
}

// --------------------------------------------------------------------

// static
WSparseSimilarity* WSparseSimilarity::deserialize(WDomNode* pNode)
{
	const std::string className = pNode->getClassName();
	const char* szClass = className.c_str();
	WSparseSimilarity* pObj = nullptr;
	if(strcmp(szClass, "WCosineSimilarity") == 0)
		return new WCosineSimilarity(pNode);
	else if(strcmp(szClass, "WEuclidSimilarity") == 0)
		return new WEuclidSimilarity(pNode);
	else if(strcmp(szClass, "WPearsonCorrelation") == 0)
		return new WPearsonCorrelation(pNode);
	else
		throw Ex("Unrecognized class: ", szClass);
	pObj->m_regularizer = pNode->getDouble("reg");
	return pObj;
}

WDomNode* WSparseSimilarity::baseDomNode(WDom* pDoc, const char* szClassName) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "class", szClassName);
	pNode->add(pDoc, "reg", m_regularizer);
	return pNode;
}

// --------------------------------------------------------------------

// virtual
WDomNode* WCosineSimilarity::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WCosineSimilarity");
	return pNode;
}

// virtual
double WCosineSimilarity::similarity(const map<size_t, double>& a, const map<size_t, double>& b)
{
	map<size_t, double>::const_iterator itA = a.begin();
	map<size_t, double>::const_iterator itB = b.begin();
	if(itA == a.end())
		return 0.0;
	if(itB == b.end())
		return 0.0;
	double sum_sq_a = 0.0;
	double sum_sq_b = 0.0;
	double sum_co_prod = 0.0;
	while(true)
	{
		if(itA->first < itB->first)
		{
			if(++itA == a.end())
				break;
		}
		else if(itB->first < itA->first)
		{
			if(++itB == b.end())
				break;
		}
		else
		{
			sum_sq_a += (itA->second * itA->second);
			sum_sq_b += (itB->second * itB->second);
			sum_co_prod += (itA->second * itB->second);
			if(++itA == a.end())
				break;
			if(++itB == b.end())
				break;
		}
	}
	double denom = sqrt(sum_sq_a * sum_sq_b) + m_regularizer;
	if(denom > 0.0)
		return sum_co_prod / denom;
	else
		return 0.0;
}

// virtual
double WCosineSimilarity::similarity(const map<size_t, double>& a, const WVec& b)
{
	map<size_t, double>::const_iterator itA = a.begin();
	if(itA == a.end())
		return 0.0;
	double sum_sq_a = 0.0;
	double sum_sq_b = 0.0;
	double sum_co_prod = 0.0;
	while(itA != a.end())
	{
		sum_sq_a += (itA->second * itA->second);
		sum_sq_b += (b[itA->first] * b[itA->first]);
		sum_co_prod += (itA->second * b[itA->first]);
		itA++;
	}
	double denom = sqrt(sum_sq_a * sum_sq_b) + m_regularizer;
	if(denom > 0.0)
		return sum_co_prod / denom;
	else
		return 0.0;
}

// virtual
double WCosineSimilarity::similarity(const WVec& a, const WVec& b)
{
	if(a.size() != b.size())
		throw Ex("mismatching sizes");
	double sum_sq_a = 0.0;
	double sum_sq_b = 0.0;
	double sum_co_prod = 0.0;
	for(size_t i = 0; i < a.size(); i++)
	{
		sum_sq_a += (a[i] * a[i]);
		sum_sq_b += (b[i] * b[i]);
		sum_co_prod += (a[i] * b[i]);
	}
	double denom = sqrt(sum_sq_a * sum_sq_b) + m_regularizer;
	if(denom > 0.0)
		return sum_co_prod / denom;
	else
		return 0.0;
}

// --------------------------------------------------------------------

// virtual
WDomNode* WPearsonCorrelation::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WPearsonCorrelation");
	return pNode;
}

// virtual
double WPearsonCorrelation::similarity(const map<size_t, double>& a, const map<size_t, double>& b)
{
	// Compute the mean of the overlapping portions
	map<size_t, double>::const_iterator itA = a.begin();
	map<size_t, double>::const_iterator itB = b.begin();
	if(itA == a.end())
		return 0.0;
	if(itB == b.end())
		return 0.0;
	double mean_a = 0.0;
	double mean_b = 0.0;
	size_t count = 0;
	while(true)
	{
		if(itA->first < itB->first)
		{
			if(++itA == a.end())
				break;
		}
		else if(itB->first < itA->first)
		{
			if(++itB == b.end())
				break;
		}
		else
		{
			mean_a += itA->second;
			mean_b += itB->second;
			count++;
			if(++itA == a.end())
				break;
			if(++itB == b.end())
				break;
		}
	}
	double d = count > 0 ? 1.0 / count : 0.0;
	mean_a *= d;
	mean_b *= d;

	// Compute the similarity
	itA = a.begin();
	itB = b.begin();
	double sum = 0.0;
	double sum_of_sq = 0.0;
	while(true)
	{
		if(itA->first < itB->first)
		{
			if(++itA == a.end())
				break;
		}
		else if(itB->first < itA->first)
		{
			if(++itB == b.end())
				break;
		}
		else
		{
			d = (itA->second - mean_a) * (itB->second - mean_b);
			sum += d;
			sum_of_sq += (d * d);
			if(++itA == a.end())
				break;
			if(++itB == b.end())
				break;
		}
	}
	double denom = sqrt(sum_of_sq) + m_regularizer;
	if(denom > 0.0)
		return std::max(-1.0, std::min(1.0, sum / denom));
	else
		return 0.0;
}

// virtual
double WPearsonCorrelation::similarity(const map<size_t, double>& a, const WVec& b)
{
	// Compute the mean of the overlapping portions
	map<size_t, double>::const_iterator itA = a.begin();
	double mean_a = 0.0;
	double mean_b = 0.0;
	size_t count = 0;
	while(itA != a.end())
	{
		mean_a += itA->second;
		mean_b += b[itA->first];
		count++;
		itA++;
	}
	double d = 1.0 / count;
	mean_a *= d;
	mean_b *= d;

	// Compute the similarity
	itA = a.begin();
	double sum = 0.0;
	double sum_of_sq = 0.0;
	while(itA != a.end())
	{
		d = (itA->second - mean_a) * (b[itA->first] - mean_b);
		sum += d;
		sum_of_sq += (d * d);
		itA++;
	}
	double denom = sqrt(sum_of_sq) + m_regularizer;
	if(denom > 0.0)
		return sum / denom;
	else
		return 0.0;
}

// virtual
double WPearsonCorrelation::similarity(const WVec& a, const WVec& b)
{
	if(a.size() != b.size())
		throw Ex("mismatching sizes");

	// Compute the mean of the overlapping portions
	double mean_a = 0.0;
	double mean_b = 0.0;
	for(size_t i = 0; i < a.size(); i++)
	{
		mean_a += a[i];
		mean_b += b[i];
	}
	double d = 1.0 / a.size();
	mean_a *= d;
	mean_b *= d;

	// Compute the similarity
	double sum = 0.0;
	double sum_of_sq = 0.0;
	for(size_t i = 0; i < a.size(); i++)
	{
		d = (a[i] - mean_a) * (b[i] - mean_b);
		sum += d;
		sum_of_sq += (d * d);
	}
	double denom = sqrt(sum_of_sq) + m_regularizer;
	if(denom > 0.0)
		return sum / denom;
	else
		return 0.0;
}

// --------------------------------------------------------------------

// virtual
WDomNode* WEuclidSimilarity::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WEuclidSimilarity");
	return pNode;
}

// virtual
double WEuclidSimilarity::similarity(const map<size_t, double>& a, const map<size_t, double>& b)
{
	map<size_t, double>::const_iterator itA = a.begin();
	map<size_t, double>::const_iterator itB = b.begin();
	if(itA == a.end())
		return 0.0;
	if(itB == b.end())
		return 0.0;
	double sum_sq = 0.0;
	while(true)
	{
		if(itA->first < itB->first)
		{
			if(++itA == a.end())
				break;
		}
		else if(itB->first < itA->first)
		{
			if(++itB == b.end())
				break;
		}
		else
		{
			double d = (itB->second - itA->second);
			sum_sq += (d * d);
			if(++itA == a.end())
				break;
			if(++itB == b.end())
				break;
		}
	}
	if(sum_sq > 0.0)
		return 1.0 / sum_sq;
	else
		return 1e12;
}

// virtual
double WEuclidSimilarity::similarity(const map<size_t, double>& a, const WVec& b)
{
	map<size_t, double>::const_iterator itA = a.begin();
	if(itA == a.end())
		return 0.0;
	double sum_sq = 0.0;
	while(itA != a.end())
	{
		double d = (b[itA->first] - itA->second);
		sum_sq += (d * d);
		itA++;
	}
	if(sum_sq > 0.0)
		return 1.0 / sum_sq;
	else
		return 1e12;
}

// virtual
double WEuclidSimilarity::similarity(const WVec& a, const WVec& b)
{
	if(a.size() != b.size())
		throw Ex("mismatching sizes");

	double sum_sq = 0.0;
	for(size_t i = 0; i < a.size(); i++)
	{
		double d = (b[i] - a[i]);
		sum_sq += (d * d);
	}
	if(sum_sq > 0.0)
		return 1.0 / sum_sq;
	else
		return 1e12;
}


} // namespace WClasses
