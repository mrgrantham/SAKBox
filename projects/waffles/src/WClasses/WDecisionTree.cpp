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

#include "WDecisionTree.h"
#include "WError.h"
#include <stdlib.h>
#include "WVec.h"
#include "WPolynomial.h"
#include "WHillClimber.h"
#include "WDistribution.h"
#include "WRand.h"
#include "WDom.h"
#include "WTransform.h"
#include "WEnsemble.h"
#include "WHolders.h"
#include <string>
#include <iostream>
#include <memory>

using namespace WClasses;
using std::string;
using std::ostream;
using std::ostringstream;
using std::vector;

namespace WClasses
{

class WDecisionTreeNode
{
public:
	WDecisionTreeNode()
	{
	}

	virtual ~WDecisionTreeNode()
	{
	}

	virtual bool IsLeaf() = 0;
	virtual size_t GetBranchSize() = 0;
	virtual WDecisionTreeNode* DeepCopy(size_t nOutputCount, WDecisionTreeNode* pInterestingNode, WDecisionTreeNode** ppOutInterestingCopy) = 0;
	virtual void print(WDecisionTree* pTree, ostream& stream, vector<char>& prefix, const char* parentValue) = 0;
	virtual void CountValues(size_t nOutput, size_t* pnCounts) = 0;
	virtual double FindSumOutputValue(size_t nOutput) = 0;
	static WDecisionTreeNode* deserialize(const WDomNode* pNode);
	virtual WDomNode* serialize(WDom* pDoc, size_t outputCount) = 0;
};

class WDecisionTreeInteriorNode : public WDecisionTreeNode
{
	friend class WDecisionTree;
protected:
	size_t m_nAttribute;
	double m_dPivot;
	size_t m_nChildren;
	size_t m_defaultChild;
	WDecisionTreeNode** m_ppChildren;

public:
	WDecisionTreeInteriorNode(size_t nAttribute, double dPivot, size_t children, size_t defaultChild)
	: WDecisionTreeNode(), m_nAttribute(nAttribute), m_dPivot(dPivot), m_nChildren(children), m_defaultChild(defaultChild)
	{
		m_ppChildren = new WDecisionTreeNode*[children](); // all nullptr
	}

	WDecisionTreeInteriorNode(const WDomNode* pNode) : WDecisionTreeNode()
	{
		m_nAttribute = (size_t)pNode->getInt("attr");
		m_dPivot = pNode->getDouble("pivot");
		WDomNode* pChildren = pNode->get("children");
		WDomListIterator it(pChildren);
		m_nChildren = it.remaining();
		m_ppChildren = new WDecisionTreeNode*[m_nChildren];
		for(size_t i = 0; it.current(); i++)
		{
			m_ppChildren[i] = WDecisionTreeNode::deserialize(it.current());
			it.advance();
		}
		m_defaultChild = (size_t)pNode->getInt("def");
	}

	~WDecisionTreeInteriorNode() override
	{
		if(m_ppChildren)
		{
			for(size_t n = 0; n < m_nChildren; n++)
				delete(m_ppChildren[n]);
			delete[] m_ppChildren;
		}
	}

	WDomNode* serialize(WDom* pDoc, size_t outputCount) override
	{
		WDomNode* pNode = pDoc->newObj();
		pNode->add(pDoc, "attr", m_nAttribute);
		pNode->add(pDoc, "pivot", m_dPivot);
		WDomNode* pChildren = pDoc->newList();
		pNode->add(pDoc, "children", pChildren);
		for(size_t i = 0; i < m_nChildren; i++)
			pChildren->add(pDoc, m_ppChildren[i]->serialize(pDoc, outputCount));
		pNode->add(pDoc, "def", m_defaultChild);
		return pNode;
	}

	bool IsLeaf() override { return false; }

	size_t GetBranchSize() override
	{
		size_t size = 1;
		size_t i;
		for(i = 0; i < m_nChildren; i++)
			size += m_ppChildren[i]->GetBranchSize();
		return size;
	}

	WDecisionTreeNode* DeepCopy(size_t nOutputCount, WDecisionTreeNode* pInterestingNode, WDecisionTreeNode** ppOutInterestingCopy) override
	{
		WDecisionTreeInteriorNode* pNewNode = new WDecisionTreeInteriorNode(m_nAttribute, m_dPivot, m_nChildren, m_defaultChild);
		for(size_t n = 0; n < m_nChildren; n++)
			pNewNode->m_ppChildren[n] = m_ppChildren[n]->DeepCopy(nOutputCount, pInterestingNode, ppOutInterestingCopy);
		if(this == pInterestingNode)
			*ppOutInterestingCopy = pNewNode;
		return pNewNode;
	}

	void print(WDecisionTree* pTree, ostream& stream, vector<char>& prefix, const char* parentValue) override
	{
		for(size_t n = 0; n + 1 < prefix.size(); n++)
			stream << prefix[n];
		stream << "|\n";
		for(size_t n = 0; n + 1 < prefix.size(); n++)
			stream << prefix[n];
		if(parentValue)
			stream << "+" << parentValue << "->";
		if(pTree->relFeatures().valueCount(m_nAttribute) == 0)
		{
			ostringstream oss;
			pTree->relFeatures().printAttrValue(oss, m_nAttribute, m_dPivot);
			string s = oss.str();
			if(pTree->relFeatures().type() == WRelation::ARFF)
				stream << "Is " << ((WArffRelation&)pTree->relFeatures()).attrName(m_nAttribute) << " < " << s.c_str() << "?\n";
			else
				stream << "Is attr " << to_str(m_nAttribute) << " < " << s.c_str() << "?\n";
			if(m_nChildren != 2)
				throw Ex("expected this node to have two child nodes");
			prefix.push_back(' ');
			prefix.push_back(' ');
			prefix.push_back(' ');
			prefix.push_back('|');
			m_ppChildren[0]->print(pTree, stream, prefix, "Yes");
			prefix.pop_back();
			prefix.push_back(' ');
			m_ppChildren[1]->print(pTree, stream, prefix, "No");
			prefix.pop_back();
			prefix.pop_back();
			prefix.pop_back();
			prefix.pop_back();
		}
		else if(pTree->isBinary())
		{
			ostringstream oss;
			pTree->relFeatures().printAttrValue(oss, m_nAttribute, m_dPivot);
			string s = oss.str();
			if(pTree->relFeatures().type() == WRelation::ARFF)
				stream << "Is " << ((WArffRelation&)pTree->relFeatures()).attrName(m_nAttribute) << " == " << s.c_str() << "?\n";
			else
				stream << "Is attr " << to_str(m_nAttribute) << " == " << s.c_str() << "?\n";
			if(m_nChildren != 2)
				throw Ex("expected this node to have two child nodes");
			prefix.push_back(' ');
			prefix.push_back(' ');
			prefix.push_back(' ');
			prefix.push_back('|');
			m_ppChildren[0]->print(pTree, stream, prefix, "Yes");
			prefix.pop_back();
			prefix.push_back(' ');
			m_ppChildren[1]->print(pTree, stream, prefix, "No");
			prefix.pop_back();
			prefix.pop_back();
			prefix.pop_back();
			prefix.pop_back();
		}
		else
		{
			if(pTree->relFeatures().type() == WRelation::ARFF)
				stream << ((WArffRelation&)pTree->relFeatures()).attrName(m_nAttribute) << "?\n";
			else
				stream << "Attr " << to_str(m_nAttribute) << "?\n";
			prefix.push_back(' ');
			prefix.push_back(' ');
			prefix.push_back(' ');
			prefix.push_back('|');
			for(size_t n = 0; n < m_nChildren; n++)
			{
				if(n + 1 == m_nChildren)
				{
					prefix.pop_back();
					prefix.push_back(' ');
				}
				ostringstream oss;
				pTree->relFeatures().printAttrValue(oss, m_nAttribute, (double)n);
				string s = oss.str();
				m_ppChildren[n]->print(pTree, stream, prefix, s.c_str());
			}
			prefix.pop_back();
			prefix.pop_back();
			prefix.pop_back();
			prefix.pop_back();
		}
	}

	// Recursive function that counts the number of times a particular
	// value is found in a particular output in this branch of the tree
	void CountValues(size_t nOutput, size_t* pnCounts) override
	{
		for(size_t n = 0; n < m_nChildren; n++)
			m_ppChildren[n]->CountValues(nOutput, pnCounts);
	}

	double FindSumOutputValue(size_t nOutput) override
	{
		double dSum = 0;
		for(size_t n = 0; n < m_nChildren; n++)
			dSum += m_ppChildren[n]->FindSumOutputValue(nOutput);
		return dSum;
	}
};

class WDecisionTreeLeafNode : public WDecisionTreeNode
{
public:
	double* m_pOutputValues;
	size_t m_nSampleSize;

public:
	WDecisionTreeLeafNode(double* pOutputValues, size_t nSampleSize) : WDecisionTreeNode()
	{
		m_pOutputValues = pOutputValues;
		m_nSampleSize = nSampleSize;
	}

	WDecisionTreeLeafNode(const WDomNode* pNode) : WDecisionTreeNode()
	{
		m_nSampleSize = (size_t)pNode->getInt("size");
		WDomNode* pOut = pNode->get("out");
		WDomListIterator it(pOut);
		size_t count = it.remaining();
		m_pOutputValues = new double[count];
		for(size_t i = 0; it.current(); i++)
		{
			m_pOutputValues[i] = it.currentDouble();
			it.advance();
		}
	}

	~WDecisionTreeLeafNode() override
	{
		delete[] m_pOutputValues;
	}

	WDomNode* serialize(WDom* pDoc, size_t outputCount) override
	{
		WDomNode* pNode = pDoc->newObj();
		pNode->add(pDoc, "size", m_nSampleSize);
		WDomNode* pOut = pDoc->newList();
		pNode->add(pDoc, "out", pOut);
		for(size_t i = 0; i < outputCount; i++)
			pOut->add(pDoc, m_pOutputValues[i]);
		return pNode;
	}

	bool IsLeaf() override { return true; }

	size_t GetBranchSize() override
	{
		return 1;
	}

	WDecisionTreeNode* DeepCopy(size_t nOutputCount, WDecisionTreeNode* pInterestingNode, WDecisionTreeNode** ppOutInterestingCopy) override
	{
		double* pOutputValues = new double[nOutputCount];
		memcpy(pOutputValues, m_pOutputValues, sizeof(double) * nOutputCount);
		WDecisionTreeLeafNode* pNewNode = new WDecisionTreeLeafNode(pOutputValues, m_nSampleSize);
		if(this == pInterestingNode)
			*ppOutInterestingCopy = pNewNode;
		return pNewNode;
	}

	void print(WDecisionTree* pTree, ostream& stream, vector<char>& prefix, const char* parentValue) override
	{
		for(size_t n = 0; n + 1 < prefix.size(); n++)
			stream << prefix[n];
		stream << "|\n";
		for(size_t n = 0; n + 1 < prefix.size(); n++)
			stream << prefix[n];
		if(parentValue)
			stream << "+" << parentValue << "->";
		for(size_t n = 0; n < pTree->relLabels().size(); n++)
		{
			if(n > 0)
				stream << ", ";
			ostringstream oss;
			pTree->relLabels().printAttrValue(oss, n, m_pOutputValues[n]);
			string s = oss.str();
			if(pTree->relLabels().type() == WRelation::ARFF)
				stream << ((WArffRelation&)pTree->relLabels()).attrName(n) << "=" << s.c_str();
			else
				stream << s.c_str();
		}
		stream << "\n";
	}

	void CountValues(size_t nOutput, size_t* pnCounts) override
	{
		int nVal = (int)m_pOutputValues[nOutput];
		pnCounts[nVal] += m_nSampleSize;
	}

	double FindSumOutputValue(size_t nOutput) override
	{
		return m_pOutputValues[nOutput] * m_nSampleSize;
	}
};

}

// static
WDecisionTreeNode* WDecisionTreeNode::deserialize(const WDomNode* pNode)
{
	if(pNode->getIfExists("children"))
		return new WDecisionTreeInteriorNode(pNode);
	else
		return new WDecisionTreeLeafNode(pNode);
}

// -----------------------------------------------------------------

WDecisionTree::WDecisionTree()
: WSupervisedLearner(), m_leafThresh(1), m_maxLevels(0), m_binaryDivisions(false)
{
	m_pRoot = nullptr;
	m_eAlg = WDecisionTree::MINIMIZE_ENTROPY;
}

WDecisionTree::WDecisionTree(const WDomNode* pNode)
: WSupervisedLearner(pNode), m_leafThresh(1), m_maxLevels(0)
{
	m_eAlg = (DivisionAlgorithm)pNode->getInt("alg");
	m_pRoot = WDecisionTreeNode::deserialize(pNode->get("root"));
	m_binaryDivisions = pNode->getBool("bin");
}

// virtual
WDecisionTree::~WDecisionTree()
{
	clear();
}

// virtual
WDomNode* WDecisionTree::serialize(WDom* pDoc) const
{
	if(!m_pRoot)
		throw Ex("Attempted to serialize a model that has not been trained");
	WDomNode* pNode = baseDomNode(pDoc, "WDecisionTree");
	pNode->add(pDoc, "alg", (long long)m_eAlg);
	pNode->add(pDoc, "root", m_pRoot->serialize(pDoc, m_pRelLabels->size()));
	pNode->add(pDoc, "bin", m_binaryDivisions);
	return pNode;
}

size_t WDecisionTree::treeSize()
{
	return m_pRoot->GetBranchSize();
}

void WDecisionTree::useBinaryDivisions()
{
	m_binaryDivisions = true;
	delete(m_pRoot);
	m_pRoot = nullptr;
}

void WDecisionTree::print(ostream& stream, WArffRelation* pFeatureRel, WArffRelation* pLabelRel)
{
	if(!m_pRoot)
		throw Ex("not trained yet");
	if(pFeatureRel && pFeatureRel->type() == WRelation::ARFF)
	{
		if(!m_pRelFeatures->isCompatible(*pFeatureRel))
			throw Ex("Feature relation not compatible");
		delete(m_pRelFeatures);
		m_pRelFeatures = pFeatureRel->clone();
	}
	if(pLabelRel && pLabelRel->type() == WRelation::ARFF)
	{
		if(!m_pRelLabels->isCompatible(*pLabelRel))
			throw Ex("Label relation not compatible");
		delete(m_pRelLabels);
		m_pRelLabels = pLabelRel->clone();
	}
	vector<char> prefix;
	m_pRoot->print(this, stream, prefix, nullptr);
}

// virtual
void WDecisionTree::trainInner(const WMatrix& features, const WMatrix& labels)
{
	clear();

	// Make a list of available features
	vector<size_t> attrPool;
	attrPool.reserve(m_pRelFeatures->size());
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
		attrPool.push_back(i);

	// Copy the data
	WMatrix tmpFeatures(m_pRelFeatures->clone());
	tmpFeatures.copy(features);
	WMatrix tmpLabels(m_pRelLabels->clone());
	tmpLabels.copy(labels);

	m_pRoot = buildBranch(tmpFeatures, tmpLabels, attrPool, 0 /*depth*/, 4 /*tolerance*/);
}

void WDecisionTree::autoTune(WMatrix& features, WMatrix& labels)
{
	// Try binary splits
	m_binaryDivisions = false;
	double bestErr = crossValidate(features, labels, 2);
	m_binaryDivisions = true;
	double d = crossValidate(features, labels, 2);
	if(d < bestErr)
		bestErr = d;
	else
		m_binaryDivisions = false;

	// Find the best leaf threshold
	size_t cap = size_t(floor(sqrt(double(features.rows()))));
	size_t bestLeafThresh = 1;
	for(size_t i = 2; i < cap; i = std::max(i + 1, size_t(i * 1.5)))
	{
		m_leafThresh = i;
		double d2 = crossValidate(features, labels, 2);
		if(d2 < bestErr)
		{
			bestErr = d2;
			bestLeafThresh = i;
		}
		else if(i >= 27)
			break;
	}

	// Set the best values
	m_maxLevels = 0;
	m_leafThresh = bestLeafThresh;
}

double WDecisionTree_measureRealSplitInfo(WMatrix& features, WMatrix& labels, WMatrix& tmpFeatures, WMatrix& tmpLabels, size_t attr, double pivot)
{
	WAssert(tmpFeatures.rows() == 0 && tmpLabels.rows() == 0);
	if(pivot == UNKNOWN_REAL_VALUE)
		return 1e308;
	size_t rowCount = features.rows();
	features.splitByPivot(&tmpFeatures, attr, pivot, &labels, &tmpLabels);
	double dInfo;
	if(features.rows() > 0 && tmpFeatures.rows() > 0)
		dInfo = (labels.measureInfo() * labels.rows() + tmpLabels.measureInfo() * tmpLabels.rows()) / rowCount;
	else
		dInfo = 1e308;
	features.mergeVert(&tmpFeatures);
	labels.mergeVert(&tmpLabels);
	return dInfo;
}

double WDecisionTree_measureBinarySplitInfo(WMatrix& features, WMatrix& labels, WMatrix& tmpFeatures, WMatrix& tmpLabels, size_t attr, int pivot)
{
	WAssert(tmpFeatures.rows() == 0 && tmpLabels.rows() == 0);
	size_t rowCount = features.rows();
	features.splitCategoricalKeepIfEqual(&tmpFeatures, attr, pivot, &labels, &tmpLabels);
	double dInfo;
	if(features.rows() > 0 && tmpFeatures.rows() > 0)
		dInfo = (labels.measureInfo() * labels.rows() + tmpLabels.measureInfo() * tmpLabels.rows()) / rowCount;
	else
		dInfo = 1e308;
	features.mergeVert(&tmpFeatures);
	labels.mergeVert(&tmpLabels);
	return dInfo;
}

double WDecisionTree_pickPivotToReduceInfo(WMatrix& features, WMatrix& labels, WMatrix& tmpFeatures, WMatrix& tmpLabels, double* pPivot, size_t attr, WRand* pRand)
{
	double bestPivot = UNKNOWN_REAL_VALUE;
	double bestInfo = 1e100;
	size_t vals = features.relation().valueCount(attr);
	if(vals == 0)
	{
		size_t attempts = std::min(features.rows() - 1, (features.rows() * features.cols() > 100000 ? (size_t)1 : (size_t)8));
		for(size_t n = 0; n < attempts; n++)
		{
			WVec& row1 = features.row((size_t)pRand->next(features.rows()));
			WVec& row2 = features.row((size_t)pRand->next(features.rows()));
			if(row1[attr] == UNKNOWN_REAL_VALUE || row2[attr] == UNKNOWN_REAL_VALUE)
				continue;
			double pivot = 0.5 * (row1[attr] + row2[attr]);
			double info = WDecisionTree_measureRealSplitInfo(features, labels, tmpFeatures, tmpLabels, attr, pivot);
			if(info + 1e-14 < bestInfo) // the small value makes it deterministic across hardware
			{
				bestInfo = info;
				bestPivot = pivot;
			}
		}
	}
	else
	{
		for(int i = 0; (size_t)i < vals; i++)
		{
			double info = WDecisionTree_measureBinarySplitInfo(features, labels, tmpFeatures, tmpLabels, attr, i);
			if(info + 1e-14 < bestInfo) // the small value makes it deterministic across hardware
			{
				bestInfo = info;
				bestPivot = (double)i;
			}
		}
	}
	*pPivot = bestPivot;
	return bestInfo;
}

double WDecisionTree_measureNominalSplitInfo(WMatrix& features, WMatrix& labels, WMatrix& tmpFeatures, WMatrix& tmpLabels, size_t nAttribute)
{
	size_t nRowCount = features.rows() - features.countValue(nAttribute, UNKNOWN_DISCRETE_VALUE);
	int values = (int)features.relation().valueCount(nAttribute);
	double dInfo = 0;
	for(int n = 0; n < values; n++)
	{
		features.splitCategoricalKeepIfEqual(&tmpFeatures, nAttribute, n, &labels, &tmpLabels);
		dInfo += ((double)tmpLabels.rows() / nRowCount) * tmpLabels.measureInfo();
		features.mergeVert(&tmpFeatures);
		labels.mergeVert(&tmpLabels);
	}
	return dInfo;
}

size_t WDecisionTree::pickDivision(WMatrix& features, WMatrix& labels, double* pPivot, vector<size_t>& attrPool, size_t nDepth)
{
	WMatrix tmpFeatures(features.relation().clone());
	tmpFeatures.reserve(features.rows());
	WMatrix tmpLabels(labels.relation().clone());
	tmpLabels.reserve(features.rows());
	if(m_eAlg == MINIMIZE_ENTROPY)
	{
		// Pick the best attribute to divide on
		WAssert(labels.rows() > 0); // Can't work without data
		double bestInfo = 1e100;
		double pivot = 0.0;
		double bestPivot = 0;
		size_t index = 0;
		size_t bestIndex = attrPool.size();
		for(vector<size_t>::iterator it = attrPool.begin(); it != attrPool.end(); it++)
		{
			double info;
			if(m_binaryDivisions || features.relation().valueCount(*it) == 0)
				info = WDecisionTree_pickPivotToReduceInfo(features, labels, tmpFeatures, tmpLabels, &pivot, *it, &m_rand);
			else
				info = WDecisionTree_measureNominalSplitInfo(features, labels, tmpFeatures, tmpLabels, *it);
			if(info + 1e-14 < bestInfo) // the small value makes it deterministic across hardware
			{
				bestInfo = info;
				bestIndex = index;
				bestPivot = pivot;
			}
			index++;
		}
		*pPivot = bestPivot;
		return bestIndex;
	}
	else if(m_eAlg == RANDOM)
	{
		// Pick the best of m_randomDraws random attributes from the attribute pool
		WAssert(features.rows() > 0); // Can't work without data
		double bestInfo = 1e200;
		double bestPivot = 0;
		size_t bestIndex = attrPool.size();
		size_t patience = std::max((size_t)6, m_randomDraws * 2);
		for(size_t i = 0; i < m_randomDraws && patience > 0; i++)
		{
			size_t index = (size_t)m_rand.next(attrPool.size());
			size_t attr = attrPool[index];
			double pivot = 0.0;
			double info;
			if(features.relation().valueCount(attr) == 0)
			{
				double a = features[(size_t)m_rand.next(features.rows())][attr];
				double b = features[(size_t)m_rand.next(features.rows())][attr];
				if(a == UNKNOWN_REAL_VALUE)
				{
					if(b == UNKNOWN_REAL_VALUE)
						pivot = features.columnMedian(attr, false);
					else
						pivot = b;
				}
				else
				{
					if(b == UNKNOWN_REAL_VALUE)
						pivot = a;
					else
						pivot = 0.5 * (a + b);
				}
				if(m_randomDraws > 1)
					info = WDecisionTree_measureRealSplitInfo(features, labels, tmpFeatures, tmpLabels, attr, pivot);
				else
					info = 0.0;
			}
			else if(m_binaryDivisions)
			{
				pivot = features[(size_t)m_rand.next(features.rows())][attr];
				if(pivot == UNKNOWN_DISCRETE_VALUE)
					pivot = features.baselineValue(attr);
				if(m_randomDraws > 1)
					info = WDecisionTree_measureBinarySplitInfo(features, labels, tmpFeatures, tmpLabels, attr, (int)pivot);
				else
					info = 0.0;
			}
			else
			{
				if(m_randomDraws > 1)
					info = WDecisionTree_measureNominalSplitInfo(features, labels, tmpFeatures, tmpLabels, attr);
				else
					info = 0.0;
			}
			if(info + 1e-14 < bestInfo) // the small value makes it deterministic across hardware
			{
				bestInfo = info;
				bestIndex = index;
				bestPivot = pivot;
			}
		}
		if(bestIndex < attrPool.size() && !features.isAttrHomogenous(attrPool[bestIndex]))
		{
			*pPivot = bestPivot;
			return bestIndex;
		}

		// We failed to find a useful attribute with random draws. (This may happen if there is a large
		// ratio of homogeneous attributes.) Now, we need to be a little more systematic about finding a good
		// attribute. (This is not specified in the random forest algorithm, but it can make a big difference
		// with some problems.)
		size_t k = (size_t)m_rand.next(attrPool.size());
		for(size_t i = 0; i < attrPool.size(); i++)
		{
			size_t index = (i + k) % attrPool.size();
			size_t attr = attrPool[index];
			if(features.relation().valueCount(attr) == 0)
			{
				// Find the min
				double m = 1e300;
				for(size_t j = 0; j < features.rows(); j++)
				{
					double d = features[j][attr];
					if(d != UNKNOWN_REAL_VALUE)
						m = std::min(m, d);
				}

				// Randomly pick one of the non-min values
				size_t candidates = 0;
				for(size_t j = 0; j < features.rows(); j++)
				{
					double d = features[j][attr];
					if(d != UNKNOWN_REAL_VALUE && d > m)
					{
						if(m_rand.next(++candidates) == 0)
							*pPivot = d;
					}
				}
				if(candidates == 0)
					continue; // This attribute is worthless
			}
			else
			{
				if(features.isAttrHomogenous(attr))
					continue; // This attribute is worthless
			}
			return index;
		}
	}
	else
		WAssert(false); // unknown division algorithm
	return attrPool.size();
}

double* WDecisionTreeNode_labelVec(WMatrix& labels)
{
	size_t n = labels.cols();
	double* pVec = new double[n];
	for(size_t i = 0; i < n; i++)
	{
		pVec[i] = labels.baselineValue(i);
		if(pVec[i] == UNKNOWN_REAL_VALUE) // This can happen if there are no known values in a real column
			pVec[i] = 0.0;
	}
	return pVec;
}

double* WDecisionTreeNode_copyIfNotTheLast(size_t emptySets, std::unique_ptr<double[]>& hBaselineVec, size_t dims)
{
	if(emptySets > 1)
	{
		double* pCopy = new double[dims];
		memcpy(pCopy, hBaselineVec.get(), sizeof(double) * dims);
		return pCopy;
	}
	else
		return hBaselineVec.release();
}

class WDTAttrPoolHolder
{
protected:
	vector<size_t>& m_attrPool;
	size_t m_attr;

public:
	WDTAttrPoolHolder(vector<size_t>& attrPool)
	: m_attrPool(attrPool), m_attr(INVALID_INDEX)
	{
	}

	void temporarilyRemoveAttribute(size_t index)
	{
		m_attr = m_attrPool[index];
		WAssert(m_attr != INVALID_INDEX);
		std::swap(m_attrPool[index], m_attrPool[m_attrPool.size() - 1]);
		m_attrPool.erase(m_attrPool.end() - 1);
	}

	~WDTAttrPoolHolder()
	{
		// Put the attribute that we temporarily removed back in the pool
		if(m_attr != INVALID_INDEX)
			m_attrPool.push_back(m_attr);
	}
};

// This constructs the decision tree in a recursive depth-first manner
WDecisionTreeNode* WDecisionTree::buildBranch(WMatrix& features, WMatrix& labels, vector<size_t>& attrPool, size_t nDepth, size_t tolerance)
{
	WAssert(features.rows() == labels.rows());

	// Make a leaf if we're out of tolerance or the output is
	// homogenous or there are no attributes left or we have
	// reached the maximum number of levels in the tree
	if(tolerance <= 0 || features.rows() <= m_leafThresh || attrPool.size() == 0 || labels.isHomogenous() || (nDepth + 1 == m_maxLevels))
	{
		return new WDecisionTreeLeafNode(WDecisionTreeNode_labelVec(labels), labels.rows());
	}

	// Pick the division
	double pivot = 0.0;
	size_t bestIndex = pickDivision(features, labels, &pivot, attrPool, nDepth);

	// Make a leaf if there are no good divisions
	if(bestIndex >= attrPool.size())
	{
		return new WDecisionTreeLeafNode(WDecisionTreeNode_labelVec(labels), labels.rows());
	}
	size_t attr = attrPool[bestIndex];

	// Make sure there aren't any missing values in the decision attribute
	features.replaceMissingValuesWithBaseline(attr);

	// Split the data
	vector<WMatrix*> featureParts;
	VectorOfPointersHolder<WMatrix> hFeatureParts(featureParts);
	vector<WMatrix*> labelParts;
	VectorOfPointersHolder<WMatrix> hLabelParts(labelParts);
	size_t nonEmptyBranchCount = 0;
	WDTAttrPoolHolder hAttrPool(attrPool);
	if(m_pRelFeatures->valueCount(attr) == 0)
	{
		// Split on a continuous attribute
		WMatrix* pOtherFeatures = new WMatrix(m_pRelFeatures->clone());
		featureParts.push_back(pOtherFeatures);
		WMatrix* pOtherLabels = new WMatrix(m_pRelLabels->clone());
		labelParts.push_back(pOtherLabels);
		features.splitByPivot(pOtherFeatures, attr, pivot, &labels, pOtherLabels);
		nonEmptyBranchCount += (features.rows() > 0 ? 1 : 0) + (pOtherFeatures->rows() > 0 ? 1 : 0);
	}
	else if(m_binaryDivisions)
	{
		// Split on a nominal attribute and specific value
		WMatrix* pOtherFeatures = new WMatrix(m_pRelFeatures->clone());
		featureParts.push_back(pOtherFeatures);
		WMatrix* pOtherLabels = new WMatrix(m_pRelLabels->clone());
		labelParts.push_back(pOtherLabels);
		features.splitCategoricalKeepIfEqual(pOtherFeatures, attr, (int)pivot, &labels, pOtherLabels);
		nonEmptyBranchCount += (features.rows() > 0 ? 1 : 0) + (pOtherFeatures->rows() > 0 ? 1 : 0);
	}
	else
	{
		// Split on a nominal attribute
		int valueCount = (int)features.relation().valueCount(attr);
		for(int i = 1; i < valueCount; i++)
		{
			WMatrix* pOtherFeatures = new WMatrix(m_pRelFeatures->clone());
			featureParts.push_back(pOtherFeatures);
			WMatrix* pOtherLabels = new WMatrix(m_pRelLabels->clone());
			labelParts.push_back(pOtherLabels);
			features.splitCategoricalKeepIfNotEqual(pOtherFeatures, attr, i, &labels, pOtherLabels);
			if(pOtherFeatures->rows() > 0)
				nonEmptyBranchCount++;
		}
		if(features.rows() > 0)
			nonEmptyBranchCount++;
		hAttrPool.temporarilyRemoveAttribute(bestIndex);
	}

	// If we didn't actually separate anything
	if(nonEmptyBranchCount < 2)
	{
		size_t setCount = featureParts.size();
		for(size_t i = 0; i < setCount; i++)
		{
			features.mergeVert(featureParts[i]);
			labels.mergeVert(labelParts[i]);
		}
		if(m_eAlg == MINIMIZE_ENTROPY)
			return new WDecisionTreeLeafNode(WDecisionTreeNode_labelVec(labels), labels.rows());
		else
		{
			// Try another division
			WDecisionTreeNode* pNode = buildBranch(features, labels, attrPool, nDepth, tolerance - 1);
			return pNode;
		}
	}

	// Make an interior node
	double* pBaselineVec = nullptr;
	size_t emptySets = 0;
	for(size_t i = 0; i < featureParts.size(); i++)
	{
		if(featureParts[i]->rows() == 0)
			emptySets++;
	}
	if(features.rows() == 0)
		emptySets++;
	if(emptySets > 0)
	{
		WMatrix* pB = labelParts[0];
		for(size_t i = 1; i < labelParts.size(); i++)
		{
			if(labelParts[i]->rows() > pB->rows())
				pB = labelParts[i];
		}
		if(labels.rows() > pB->rows())
			pBaselineVec = WDecisionTreeNode_labelVec(labels);
		else
			pBaselineVec = WDecisionTreeNode_labelVec(*pB);
	}
	std::unique_ptr<double[]> hBaselineVec(pBaselineVec);
	WDecisionTreeInteriorNode* pNode = new WDecisionTreeInteriorNode(attr, pivot, featureParts.size() + 1, 0);
	std::unique_ptr<WDecisionTreeInteriorNode> hNode(pNode);
	size_t biggest = features.rows();
	if(features.rows() > 0)
	{
		pNode->m_ppChildren[0] = buildBranch(features, labels, attrPool, nDepth + 1, tolerance);
	}
	else
	{
		pNode->m_ppChildren[0] = new WDecisionTreeLeafNode(WDecisionTreeNode_copyIfNotTheLast(emptySets--, hBaselineVec, labels.cols()), 0);
	}
	for(size_t i = 0; i < featureParts.size(); i++)
	{
		if(featureParts[i]->rows() > 0)
		{
			pNode->m_ppChildren[i + 1] = buildBranch(*featureParts[i], *labelParts[i], attrPool, nDepth + 1, tolerance);
			if(featureParts[i]->rows() > biggest)
			{
				biggest = featureParts[i]->rows();
				pNode->m_defaultChild = i + 1;
			}
		}
		else
			pNode->m_ppChildren[i + 1] = new WDecisionTreeLeafNode(WDecisionTreeNode_copyIfNotTheLast(emptySets--, hBaselineVec, labels.cols()), 0);
	}
	return hNode.release();
}

WDecisionTreeLeafNode* WDecisionTree::findLeaf(const WVec& in, size_t* pDepth)
{
	if(!m_pRoot)
		throw Ex("Not trained yet");
	WDecisionTreeNode* pNode = m_pRoot;
	int nVal;
	size_t nDepth = 1;
	while(!pNode->IsLeaf())
	{
		WDecisionTreeInteriorNode* pInterior = (WDecisionTreeInteriorNode*)pNode;
		if(m_pRelFeatures->valueCount(pInterior->m_nAttribute) == 0)
		{
			if(in[pInterior->m_nAttribute] == UNKNOWN_REAL_VALUE)
				pNode = pInterior->m_ppChildren[pInterior->m_defaultChild];
			else if(in[pInterior->m_nAttribute] < pInterior->m_dPivot)
				pNode = pInterior->m_ppChildren[0];
			else
				pNode = pInterior->m_ppChildren[1];
		}
		else if(m_binaryDivisions)
		{
			nVal = (int)in[pInterior->m_nAttribute];
			if(nVal < 0)
			{
				WAssert(nVal == UNKNOWN_DISCRETE_VALUE); // out of range
				nVal = (int)pInterior->m_defaultChild;
			}
			WAssert((size_t)nVal < m_pRelFeatures->valueCount(pInterior->m_nAttribute)); // value out of range
			if(nVal == (int)pInterior->m_dPivot)
				pNode = pInterior->m_ppChildren[0];
			else
				pNode = pInterior->m_ppChildren[1];
		}
		else
		{
			nVal = (int)in[pInterior->m_nAttribute];
			if(nVal < 0)
			{
				WAssert(nVal == UNKNOWN_DISCRETE_VALUE); // out of range
				nVal = (int)pInterior->m_defaultChild;
			}
			WAssert((size_t)nVal < m_pRelFeatures->valueCount(pInterior->m_nAttribute)); // value out of range
			pNode = pInterior->m_ppChildren[nVal];
		}
		nDepth++;
	}
	*pDepth = nDepth;
	return (WDecisionTreeLeafNode*)pNode;
}

// virtual
void WDecisionTree::predict(const WVec& in, WVec& out)
{
	size_t depth;
	WDecisionTreeLeafNode* pLeaf = findLeaf(in, &depth);
	out.copy(pLeaf->m_pOutputValues, m_pRelLabels->size());
}

// virtual
void WDecisionTree::predictDistribution(const WVec& in, WPrediction* out)
{
	// Copy the output values into the row
	size_t depth;
	WDecisionTreeLeafNode* pLeaf = findLeaf(in, &depth);
	size_t n, nValues;
	size_t labelDims = m_pRelLabels->size();
	for(n = 0; n < labelDims; n++)
	{
		nValues = m_pRelLabels->valueCount(n);
		if(nValues == 0)
			out[n].makeNormal()->setMeanAndVariance(pLeaf->m_pOutputValues[n], (double)depth);
		else
			out[n].makeCategorical()->setSpike(nValues, (int)pLeaf->m_pOutputValues[n], (int)depth);
	}
}

// virtual
void WDecisionTree::clear()
{
	delete(m_pRoot);
	m_pRoot = nullptr;
}

// static
void WDecisionTree::test()
{
	{
		WDecisionTree tree;
		tree.basicTest(0.704, 0.77);
	}
	{
		WDecisionTree ml2Tree;
		ml2Tree.setMaxLevels(2);
		ml2Tree.basicTest(0.57, 0.68);
	}
	{
		WDecisionTree ml1Tree;
		ml1Tree.setMaxLevels(1);
		ml1Tree.basicTest(0.33, 0.33);
	}
}

// ----------------------------------------------------------------------

namespace WClasses
{
class WMeanMarginsTreeNode
{
public:
	WMeanMarginsTreeNode()
	{
	}

	virtual ~WMeanMarginsTreeNode()
	{
	}

	virtual bool IsLeaf() = 0;
	virtual WDomNode* serialize(WDom* pDoc, size_t nInputs, size_t nOutputs) = 0;

	static WMeanMarginsTreeNode* deserialize(const WDomNode* pNode);
};


class WMeanMarginsTreeInteriorNode : public WMeanMarginsTreeNode
{
protected:
	WVec m_pCenter;
	WVec m_pNormal;
	WMeanMarginsTreeNode* m_pLeft;
	WMeanMarginsTreeNode* m_pRight;

public:
	WMeanMarginsTreeInteriorNode(size_t featureDims, const WVec& center, const WVec& normal)
	: WMeanMarginsTreeNode()
	{
		m_pCenter.copy(center);
		m_pNormal.copy(normal);
		m_pLeft = nullptr;
		m_pRight = nullptr;
	}

	WMeanMarginsTreeInteriorNode(const WDomNode* pNode)
	: WMeanMarginsTreeNode()
	{
		m_pCenter.deserialize(pNode->get("center"));
		m_pNormal.deserialize(pNode->get("normal"));
		if(m_pNormal.size() != m_pCenter.size())
			throw Ex("mismatching sizes");
		m_pLeft = WMeanMarginsTreeNode::deserialize(pNode->get("left"));
		m_pRight = WMeanMarginsTreeNode::deserialize(pNode->get("right"));
	}

	~WMeanMarginsTreeInteriorNode() override
	{
		delete(m_pLeft);
		delete(m_pRight);
	}

	WDomNode* serialize(WDom* pDoc, size_t nInputs, size_t nOutputs) override
	{
		WDomNode* pNode = pDoc->newObj();
		pNode->add(pDoc, "center", m_pCenter.serialize(pDoc));
		pNode->add(pDoc, "normal", m_pNormal.serialize(pDoc));
		pNode->add(pDoc, "left", m_pLeft->serialize(pDoc, nInputs, nOutputs));
		pNode->add(pDoc, "right", m_pRight->serialize(pDoc, nInputs, nOutputs));
		return pNode;
	}

	bool IsLeaf() override
	{
		return false;
	}

	bool Test(const WVec& inputVector, size_t nInputs)
	{
		return (inputVector - m_pCenter).dotProductIgnoringUnknowns(m_pNormal) >= 0;
	}

	void SetLeft(WMeanMarginsTreeNode* pNode)
	{
		m_pLeft = pNode;
	}

	void SetRight(WMeanMarginsTreeNode* pNode)
	{
		m_pRight = pNode;
	}

	WMeanMarginsTreeNode* GetRight()
	{
		return m_pRight;
	}

	WMeanMarginsTreeNode* GetLeft()
	{
		return m_pLeft;
	}
};

class WMeanMarginsTreeLeafNode : public WMeanMarginsTreeNode
{
protected:
	WVec m_pOutputs;

public:
	WMeanMarginsTreeLeafNode(size_t nOutputCount, const WVec& outputs)
	: WMeanMarginsTreeNode()
	{
		m_pOutputs.copy(outputs);
	}

	WMeanMarginsTreeLeafNode(const WDomNode* pNode)
	: WMeanMarginsTreeNode()
	{
		m_pOutputs.deserialize(pNode);
	}

	~WMeanMarginsTreeLeafNode() override
	{
	}

	WDomNode* serialize(WDom* pDoc, size_t nInputs, size_t nOutputs) override
	{
		return m_pOutputs.serialize(pDoc);
	}

	bool IsLeaf() override
	{
		return true;
	}

	WVec& GetOutputs()
	{
		return m_pOutputs;
	}
};
}

// static
WMeanMarginsTreeNode* WMeanMarginsTreeNode::deserialize(const WDomNode* pNode)
{
	if(pNode->type() == WDomNode::type_list)
		return new WMeanMarginsTreeLeafNode(pNode);
	else
		return new WMeanMarginsTreeInteriorNode(pNode);
}

// ---------------------------------------------------------------

WMeanMarginsTree::WMeanMarginsTree()
: WSupervisedLearner(), m_internalFeatureDims(0), m_internalLabelDims(0), m_pRoot(nullptr)
{
}

WMeanMarginsTree::WMeanMarginsTree(const WDomNode* pNode)
: WSupervisedLearner(pNode)
{
	m_pRoot = WMeanMarginsTreeNode::deserialize(pNode->get("root"));
	m_internalFeatureDims = (size_t)pNode->getInt("ifd");
	m_internalLabelDims = (size_t)pNode->getInt("ild");
}

WMeanMarginsTree::~WMeanMarginsTree()
{
	delete(m_pRoot);
}

// virtual
WDomNode* WMeanMarginsTree::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WMeanMarginsTree");
	pNode->add(pDoc, "ifd", m_internalFeatureDims);
	pNode->add(pDoc, "ild", m_internalLabelDims);
	if(!m_pRoot)
		throw Ex("Attempted to serialize a model that has not been trained");
	pNode->add(pDoc, "root", m_pRoot->serialize(pDoc, m_internalFeatureDims, m_internalLabelDims));
	return pNode;
}

// virtual
void WMeanMarginsTree::trainInner(const WMatrix& features, const WMatrix& labels)
{
	if(!features.relation().areContinuous())
		throw Ex("WMeanMarginsTree only supports continuous features. Perhaps you should wrap it in a WAutoFilter.");
	if(!labels.relation().areContinuous())
		throw Ex("WMeanMarginsTree only supports continuous labels. Perhaps you should wrap it in a WAutoFilter.");
	clear();
	m_internalFeatureDims = features.cols();
	m_internalLabelDims = labels.cols();
	size_t* pBuf2 = new size_t[m_internalFeatureDims * 2];
	std::unique_ptr<size_t[]> hBuf2(pBuf2);
	WMatrix fTmp(features);
	WMatrix lTmp(labels);
	m_pRoot = buildNode(fTmp, lTmp, pBuf2);
}

void WMeanMarginsTree::autoTune(WMatrix& features, WMatrix& labels)
{
	// This model has no parameters to tune
}

WMeanMarginsTreeNode* WMeanMarginsTree::buildNode(WMatrix& features, WMatrix& labels, size_t* pBuf2)
{
	// Check for a leaf node
	WAssert(features.rows() == labels.rows());
	size_t nCount = features.rows();
	if(nCount < 2)
	{
		WAssert(nCount > 0); // no data left
		return new WMeanMarginsTreeLeafNode(m_internalLabelDims, labels[0]);
	}

	// Compute the centroid and principal component of the labels
	WVec pLabelCentroid;
	labels.centroid(pLabelCentroid);
	WVec pPrincipalComponent;
	labels.principalComponentIgnoreUnknowns(pPrincipalComponent, pLabelCentroid, &m_rand);

	// Compute the centroid of each feature cluster in a manner tolerant of unknown values
	WVec pFeatureCentroid1;
	pFeatureCentroid1.resize(features.cols());
	pFeatureCentroid1.fill(0.0);
	WVec pFeatureCentroid2;
	pFeatureCentroid2.resize(features.cols());
	pFeatureCentroid2.fill(0.0);
	size_t* pCounts1 = pBuf2;
	size_t* pCounts2 = pCounts1 + m_internalFeatureDims;
	memset(pCounts1, '\0', sizeof(size_t) * m_internalFeatureDims);
	memset(pCounts2, '\0', sizeof(size_t) * m_internalFeatureDims);
	for(size_t i = 0; i < nCount; i++)
	{
		const WVec& f = features[i];
		if((labels[i] - pLabelCentroid).dotProductIgnoringUnknowns(pPrincipalComponent) >= 0)
		{
			size_t* pC = pCounts2;
			for(size_t j = 0; j < m_internalFeatureDims; j++)
			{
				if(f[j] != UNKNOWN_REAL_VALUE)
				{
					pFeatureCentroid2[j] += f[j];
					(*pC)++;
				}
				pC++;
			}
		}
		else
		{
			size_t* pC = pCounts1;
			for(size_t j = 0; j < m_internalFeatureDims; j++)
			{
				if(f[j] != UNKNOWN_REAL_VALUE)
				{
					pFeatureCentroid1[j] += f[j];
					(*pC)++;
				}
				pC++;
			}
		}
	}
	size_t* pC1 = pCounts1;
	size_t* pC2 = pCounts2;
	for(size_t j = 0; j < m_internalFeatureDims; j++)
	{
		if(*pC1 == 0 || *pC2 == 0)
			return new WMeanMarginsTreeLeafNode(m_internalLabelDims, pLabelCentroid);
		pFeatureCentroid1[j] /= *(pC1++);
		pFeatureCentroid2[j] /= *(pC2++);
	}

	// Compute the feature center and normal
	pFeatureCentroid1 += pFeatureCentroid2;
	pFeatureCentroid1 *= 0.5;
	pFeatureCentroid2 -= pFeatureCentroid1;
	pFeatureCentroid2.normalize();

	// Make the interior node
	WMeanMarginsTreeInteriorNode* pNode = new WMeanMarginsTreeInteriorNode(m_internalFeatureDims, pFeatureCentroid1, pFeatureCentroid2);
	std::unique_ptr<WMeanMarginsTreeInteriorNode> hNode(pNode);

	// Divide the data
	WMatrix otherFeatures(features.relation().clone());
	WMatrix otherLabels(labels.relation().clone());
	{
		WMergeDataHolder hFeatures(features, otherFeatures);
		WMergeDataHolder hLabels(labels, otherLabels);
		otherFeatures.reserve(features.rows());
		otherLabels.reserve(labels.rows());
		for(size_t i = features.rows() - 1; i < features.rows(); i--)
		{
			if(pNode->Test(features[i], m_internalFeatureDims))
			{
				otherFeatures.takeRow(features.releaseRow(i));
				otherLabels.takeRow(labels.releaseRow(i));
			}
		}

		// If we couldn't separate anything, just return a leaf node
		if(features.rows() == 0 || otherFeatures.rows() == 0)
			return new WMeanMarginsTreeLeafNode(m_internalLabelDims, pLabelCentroid);

		// Build the child nodes
		pNode->SetLeft(buildNode(features, labels, pBuf2));
		pNode->SetRight(buildNode(otherFeatures, otherLabels, pBuf2));
	}
	WAssert(otherFeatures.rows() == 0 && otherLabels.rows() == 0);
	return hNode.release();
}

// virtual
void WMeanMarginsTree::predictDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, this model cannot predict a distribution");
}

// virtual
void WMeanMarginsTree::predict(const WVec& in, WVec& out)
{
	WMeanMarginsTreeNode* pNode = m_pRoot;
	[[maybe_unused]] size_t nDepth = 1;
	while(!pNode->IsLeaf())
	{
		if(((WMeanMarginsTreeInteriorNode*)pNode)->Test(in, m_internalFeatureDims))
			pNode = ((WMeanMarginsTreeInteriorNode*)pNode)->GetRight();
		else
			pNode = ((WMeanMarginsTreeInteriorNode*)pNode)->GetLeft();
		nDepth++;
	}
	out.copy(((WMeanMarginsTreeLeafNode*)pNode)->GetOutputs());
}

// virtual
void WMeanMarginsTree::clear()
{
	delete(m_pRoot);
	m_pRoot = nullptr;
	m_internalFeatureDims = 0;
	m_internalLabelDims = 0;
}

// static
void WMeanMarginsTree::test()
{
	WAutoFilter af(new WMeanMarginsTree());
	af.basicTest(0.70, 0.9);
}









WRandomForest::WRandomForest(size_t trees, size_t samples)
: WSupervisedLearner()
{
	m_pEnsemble = new WBag();
	for(size_t i = 0; i < trees; i++)
	{
		WDecisionTree* pTree = new WDecisionTree();
		pTree->useBinaryDivisions();
		pTree->useRandomDivisions(samples);
		m_pEnsemble->addLearner(pTree);
	}
}

WRandomForest::WRandomForest(const WDomNode* pNode, WLearnerLoader& ll)
: WSupervisedLearner(pNode)
{
	m_pEnsemble = new WBag(pNode->get("bag"), ll);
}

// virtual
WRandomForest::~WRandomForest()
{
	delete(m_pEnsemble);
}

// virtual
WDomNode* WRandomForest::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WRandomForest");
	pNode->add(pDoc, "bag", m_pEnsemble->serialize(pDoc));
	return pNode;
}

// virtual
void WRandomForest::clear()
{
	m_pEnsemble->clear();
}

void WRandomForest::print(std::ostream& stream, WArffRelation* pFeatureRel, WArffRelation* pLabelRel)
{
	std::vector<WWeightedModel*>& models = m_pEnsemble->models();
	size_t nModels = models.size();
	for(size_t i = 0; i < nModels; i++)
	{
		stream << "TREE " << i << ":" << std::endl;
		((WDecisionTree*)models[i]->m_pModel)->print(stream, pFeatureRel, pLabelRel);
	}
}

// virtual
void WRandomForest::trainInner(const WMatrix& features, const WMatrix& labels)
{
	m_pEnsemble->train(features, labels);
}

// virtual
void WRandomForest::predict(const WVec& in, WVec& out)
{
	m_pEnsemble->predict(in, out);
}

// virtual
void WRandomForest::predictDistribution(const WVec& in, WPrediction* out)
{
	m_pEnsemble->predictDistribution(in, out);
}

// static
void WRandomForest::test()
{
	WRandomForest rf(30);
	rf.basicTest(0.762, 0.925, 0.01);
}
