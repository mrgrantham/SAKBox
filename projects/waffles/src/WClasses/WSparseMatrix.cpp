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

#include "WError.h"
#include "WSparseMatrix.h"
#include <math.h>
#include "WMatrix.h"
#include "WVec.h"
#include "WFile.h"
#include "WRand.h"
#include "WHolders.h"
#include <fstream>
#include "WDom.h"
#include <cmath>
#include <set>
#include <memory>

using std::cout;

namespace WClasses
{

WSparseMatrix::WSparseMatrix(size_t rowCount, size_t colCount, double def_Value)
: m_cols(colCount), m_defaultValue(def_Value)
{
	m_rows.resize(rowCount);
}

WSparseMatrix::WSparseMatrix(const WDomNode* pNode)
{
	m_defaultValue = pNode->getDouble("def");
	m_cols = (size_t)pNode->getInt("cols");
	WDomNode* pRows = pNode->get("rows");
	WDomListIterator it1(pRows);
	size_t rowCount = it1.remaining();
	m_rows.resize(rowCount);
	for(size_t i = 0; i < rowCount; i++)
	{
		WDomNode* pElements = it1.current();
		for(WDomListIterator it2(pElements); it2.current(); it2.advance())
		{
			size_t col = (size_t)it2.currentInt();
			it2.advance();
			if(!it2.current())
				throw Ex("Expected an even number of items in the list");
			double val = it2.currentDouble();
			set(i, col, val);
		}
		it1.advance();
	}
}

WSparseMatrix::~WSparseMatrix()
{
}

WDomNode* WSparseMatrix::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "def", m_defaultValue);
	pNode->add(pDoc, "cols", m_cols);
	WDomNode* pRows = pNode->add(pDoc, "rows", pDoc->newList());
	for(size_t i = 0; i < m_rows.size(); i++)
	{
		WDomNode* pElements = pRows->add(pDoc, pDoc->newList());
		for(Iter it = rowBegin(i); it != rowEnd(i); it++)
		{
			pElements->add(pDoc, it->first);
			pElements->add(pDoc, it->second);
		}
	}
	return pNode;
}

void WSparseMatrix::fullRow(WVec& outFullRow, size_t r)
{
	outFullRow.resize(m_cols);
	outFullRow.fill(m_defaultValue);
	Iter end = rowEnd(r);
	for(Iter it = rowBegin(r); it != end; it++)
		outFullRow[it->first] = it->second;
}

double WSparseMatrix::get(size_t rowIndex, size_t colIndex) const
{
	WAssert(rowIndex < m_rows.size() && colIndex < m_cols); // out of range
	SparseVec::const_iterator it = m_rows[rowIndex].find(colIndex);
	if(it == m_rows[rowIndex].end())
		return m_defaultValue;
	return it->second;
}

void WSparseMatrix::set(size_t rowIndex, size_t colIndex, double val)
{
	WAssert(rowIndex < m_rows.size() && colIndex < m_cols); // out of range
	if(val == m_defaultValue)
		m_rows[rowIndex].erase(colIndex);
	else
		m_rows[rowIndex][colIndex] = val;
}

void WSparseMatrix::multiply(double scalar)
{
	if(m_defaultValue != 0.0)
		throw Ex("This method assumes the default value is 0");
	for(size_t r = 0; r < m_rows.size(); r++)
	{
		SparseVec::iterator end = m_rows[r].end();
		for(SparseVec::iterator it = m_rows[r].begin(); it != end; it++)
			it->second *= scalar;
	}
}

WMatrix* WSparseMatrix::multiply(WMatrix* pThat, bool transposeThat)
{
	// Transpose pThat if necessary
	WMatrix* pOther = pThat;
	std::unique_ptr<WMatrix> hOther;
	if(!transposeThat)
	{
		pOther = pThat->transpose();
		hOther.reset(pOther);
	}
	if(pOther->cols() != cols())
		throw Ex("Matrices have incompatible sizes");

	// Do the multiplying
	WMatrix* pResult = new WMatrix(rows(), pOther->rows());
	for(size_t r = 0; r < rows(); r++)
	{
		SparseVec& a = row(r);
		WVec& out = pResult->row(r);
		for(size_t c = 0; c < pOther->rows(); c++)
		{
			WVec& b = pOther->row(c);
			out[c] = WSparseVec::dotProduct(a, b);
		}
	}
	return pResult;
}

WMatrix* WSparseMatrix::project(WMatrix& components)
{
	WMatrix* pOut = new WMatrix(rows(), components.rows());
	for(size_t i = 0; i < rows(); i++)
	{
		for(size_t j = 0; j < components.rows(); j++)
		{
			(*pOut)[i][j] = WSparseVec::dotProduct(row(i), components.row(j));
		}
	}
	return pOut;
}

void WSparseMatrix::copyFrom(const WSparseMatrix* that)
{
	size_t rowCount = std::min(m_rows.size(), that->rows());
	for(size_t r = 0; r < rowCount; r++)
	{
		Iter end = that->rowEnd(r);
		size_t pos = 0;
		for(Iter it = that->rowBegin(r); it != end && pos < m_cols; it++)
		{
			set(r, it->first, it->second);
			pos++;
		}
	}
}

void WSparseMatrix::copyFrom(const WMatrix* that)
{
	size_t rowCount = std::min(m_rows.size(), that->rows());
	size_t colCount = std::min(m_cols, (size_t)that->cols());
	for(size_t r = 0; r < rowCount; r++)
	{
		const WVec& rr = that->row(r);
		for(size_t c = 0; c < colCount; c++)
			set(r, c, rr[c]);
	}
}

void WSparseMatrix::newRow()
{
	newRows(1);
}

void WSparseMatrix::newRows(size_t n)
{
	m_rows.resize(m_rows.size() + n);
}

void WSparseMatrix::copyRow(SparseVec& r)
{
	size_t n = m_rows.size();
	newRow();
	SparseVec& m = m_rows[n];
	m = r;
}

void WSparseMatrix::clear()
{
	for(size_t r = 0; r < rows(); r++)
		m_rows[r].clear();
}

WMatrix* WSparseMatrix::toFullMatrix()
{
	WMatrix* pData = new WMatrix(m_rows.size(), m_cols);
	for(size_t r = 0; r < m_rows.size(); r++)
	{
		WVec& rr = pData->row(r);
		rr.fill(m_defaultValue);
		Iter end = rowEnd(r);
		for(Iter it = rowBegin(r); it != end; it++)
			rr[it->first] = it->second;
	}
	return pData;
}

void WSparseMatrix::swapColumns(size_t a, size_t b)
{
	for(size_t r = 0; r < m_rows.size(); r++)
	{
		double aa = get(r, a);
		double bb = get(r, b);
		set(r, a, bb);
		set(r, b, aa);
	}
}

void WSparseMatrix::swapRows(size_t a, size_t b)
{
	std::swap(m_rows[a], m_rows[b]);
}

void WSparseMatrix::shuffle(WRand* pRand, WMatrix* pLabels)
{
	if(pLabels)
	{
		for(size_t n = m_rows.size(); n > 0; n--)
		{
			size_t r = (size_t)pRand->next(n);
			swapRows(r, n - 1);
			pLabels->swapRows(r, n - 1);
		}
	}
	else
	{
		for(size_t n = m_rows.size(); n > 0; n--)
			swapRows((size_t)pRand->next(n), n - 1);
	}
}

WSparseMatrix* WSparseMatrix::subMatrix(size_t r, size_t c, size_t height, size_t width)
{
	if(r + height >= m_rows.size() || c + width >= m_cols)
		throw Ex("out of range");
	WSparseMatrix* pSub = new WSparseMatrix(height, width, m_defaultValue);
	for(size_t y = 0; y < height; y++)
	{
		for(size_t x = 0; x < width; x++)
			pSub->set(y, x, get(r + y, c + x));
	}
	return pSub;
}

WSparseMatrix* WSparseMatrix::transpose()
{
	WSparseMatrix* pThat = new WSparseMatrix(cols(), rows(), m_defaultValue);
	for(size_t i = 0; i < rows(); i++)
	{
		Iter end = rowEnd(i);
		for(Iter it = rowBegin(i); it != end; it++)
			pThat->set(it->first, i, it->second);
	}
	return pThat;
}






double WSparseMatrix_pythag(double a, double b)
{
	double at = std::abs(a);
	double bt = std::abs(b);
	if(at > bt)
	{
		double ct = bt / at;
		return at * sqrt(1.0 + ct * ct);
	}
	else if(bt > 0.0)
	{
		double ct = at / bt;
		return bt * sqrt(1.0 + ct * ct);
	}
	else
		return 0.0;
}

double WSparseMatrix_takeSign(double a, double b)
{
	return (b >= 0.0 ? std::abs(a) : -std::abs(a));
}

void WSparseMatrix::singularValueDecomposition(WSparseMatrix** ppU, double** ppDiag, WSparseMatrix** ppV, bool throwIfNoConverge, size_t maxIters)
{
	if(m_defaultValue != 0.0)
		throw Ex("Expected the default value to be 0");
	if(rows() >= cols())
		singularValueDecompositionHelper(ppU, ppDiag, ppV, throwIfNoConverge, maxIters);
	else
	{
		WSparseMatrix* pTemp = transpose();
		std::unique_ptr<WSparseMatrix> hTemp(pTemp);
		pTemp->singularValueDecompositionHelper(ppV, ppDiag, ppU, throwIfNoConverge, maxIters);
		WSparseMatrix* pOldV = *ppV;
		*ppV = pOldV->transpose();
		delete(pOldV);
		WSparseMatrix* pOldU = *ppU;
		*ppU = pOldU->transpose();
		delete(pOldU);
	}
}

double WSparseMatrix_safeDivide(double n, double d)
{
	if(d == 0.0 && n == 0.0)
		return 0.0;
	else
	{
		double t = n / d;
		//WAssert(t > -1e200, "prob");
		return t;
	}
}

void WSparseMatrix::singularValueDecompositionHelper(WSparseMatrix** ppU, double** ppDiag, WSparseMatrix** ppV, bool throwIfNoConverge, size_t maxIters)
{
	int m = (int)rows();
	int n = (int)cols();
	if(m < n)
		throw Ex("Expected at least as many rows as columns");
	int j, k, q;
	int l = 0;
	double c, f, h, s, x, y, z;
	double norm = 0.0;
	double g = 0.0;
	double scale = 0.0;
	WSparseMatrix* pU = new WSparseMatrix(m, m);
	std::unique_ptr<WSparseMatrix> hU(pU);
	pU->copyFrom(this);
	double* pSigma = new double[n];
	std::unique_ptr<double[]> hSigma(pSigma);
	WSparseMatrix* pV = new WSparseMatrix(n, n);
	std::unique_ptr<WSparseMatrix> hV(pV);
	WTEMPBUF(double, temp, n);

	// Householder reduction to bidiagonal form
	WSparseMatrix* pUT = pU->transpose();
	std::unique_ptr<WSparseMatrix> hUT(pUT);
	for(int i = 0; i < n; i++)
	{
		// Left-hand reduction
		temp[i] = scale * g;
		l = i + 1;
		g = 0.0;
		s = 0.0;
		scale = 0.0;
		if(i < m)
		{
			SparseVec::iterator end = pUT->m_rows[i].end();
			SparseVec::iterator it, it2, end2;
			for(it = pUT->m_rows[i].begin(); it != end && it->first < (size_t)i; it++) {}
			for(; it != end; it++)
				scale += std::abs(it->second);
			if(scale != 0.0)
			{
				for(it = pUT->m_rows[i].begin(); it != end && it->first < (size_t)i; it++) {}
				for(; it != end; it++)
				{
					double t = WSparseMatrix_safeDivide(it->second, scale);
					it->second = t;
					pU->row(it->first)[i] = t;
					s += t * t;
				}
				f = pU->row(i)[i];
				g = -WSparseMatrix_takeSign(sqrt(s), f);
				h = f * g - s;
				pU->row(i)[i] = f - g;
				pUT->row(i)[i] = f - g;
				if(i != n - 1)
				{
					for(j = l; j < n; j++)
					{
						s = 0.0;
						end2 = pUT->m_rows[j].end();
						for(it = pUT->m_rows[i].begin(); it != end && it->first < (size_t)i; it++) {}
						for(it2 = pUT->m_rows[j].begin(); it2 != end2 && it2->first < (size_t)i; it2++) {}
						while(it != end && it2 != end2)
						{
							if(it->first < it2->first)
								it++;
							else if(it2->first < it->first)
								it2++;
							else
							{
								s += it->second * it2->second;
								it++;
								it2++;
							}
						}
						f = WSparseMatrix_safeDivide(s, h);
						for(it = pUT->m_rows[i].begin(); it != end && it->first < (size_t)i; it++) {}
						for(; it != end; it++)
						{
							double t = pU->row(it->first)[j] + f * it->second;
							pU->row(it->first)[j] = t;
							pUT->row(j)[it->first] = t;
						}
					}
				}
				for(it = pUT->m_rows[i].begin(); it != end && it->first < (size_t)i; it++) {}
				for(; it != end; it++)
				{
					double t = it->second * scale;
					it->second = t;
					pU->row(it->first)[i] = t;
				}
			}
		}
		pSigma[i] = scale * g;

		// Right-hand reduction
		g = 0.0;
		s = 0.0;
		scale = 0.0;
		if(i < m && i != n - 1)
		{
			SparseVec::iterator end = pU->m_rows[i].end();
			SparseVec::iterator it, it2, end2;
			for(it = pU->m_rows[i].begin(); it != end && it->first < (size_t)l; it++) {}
			for(; it != end; it++)
				scale += std::abs(it->second);
			if(scale != 0.0)
			{
				for(it = pU->m_rows[i].begin(); it != end && it->first < (size_t)l; it++) {}
				for(; it != end; it++)
				{
					double t = WSparseMatrix_safeDivide(it->second, scale);
					it->second = t;
					pUT->row(it->first)[i] = t;
					s += t * t;
				}
				f = pU->row(i)[l];
				g = -WSparseMatrix_takeSign(sqrt(s), f);
				h = f * g - s;
				pU->row(i)[l] = f - g;
				pUT->row(l)[i] = f - g;
				for(it = pU->m_rows[i].begin(); it != end && it->first < (size_t)l; it++) {}
				for(; it != end; it++)
					temp[it->first] = WSparseMatrix_safeDivide(it->second, h);
				if(i != m - 1)
				{
					for(j = l; j < m; j++)
					{
						s = 0.0;
						end2 = pU->m_rows[j].end();
						for(it = pU->m_rows[i].begin(); it != end && it->first < (size_t)l; it++) {}
						for(it2 = pU->m_rows[j].begin(); it2 != end2 && it2->first < (size_t)l; it2++) {}
						while(it != end && it2 != end2)
						{
							if(it->first < it2->first)
								it++;
							else if(it2->first < it->first)
								it2++;
							else
							{
								s += it->second * it2->second;
								it++;
								it2++;
							}
						}
						for(it = pU->m_rows[i].begin(); it != end && it->first < (size_t)l; it++) {}
						for(; it != end; it++)
						{
							double t = pU->get(j, it->first) + s * temp[it->first];
							pU->set(j, it->first, t);
							pUT->set(it->first, j, t);
						}
					}
				}
				for(it = pU->m_rows[i].begin(); it != end && it->first < (size_t)l; it++) {}
				for(; it != end; it++)
				{
					double t = it->second * scale;
					it->second = t;
					pUT->set(it->first, i, t);
				}
			}
		}
		norm = std::max(norm, std::abs(pSigma[i]) + std::abs(temp[i]));
	}

	// Accumulate right-hand transform
	for(int i = n - 1; i >= 0; i--)
	{
		if(i < n - 1)
		{
			if(g != 0.0)
			{
				for(j = l; j < n; j++)
					pV->row(i)[j] = WSparseMatrix_safeDivide(WSparseMatrix_safeDivide(pU->row(i)[j], pU->row(i)[l]), g); // (double-division to avoid underflow)
				for(j = l; j < n; j++)
				{
					s = 0.0;
					SparseVec::iterator endU = pU->m_rows[i].end();
					SparseVec::iterator endV = pV->m_rows[j].end();
					SparseVec::iterator itU, itV;
					for(itU = pU->m_rows[i].begin(); itU != endU && itU->first < (size_t)l; itU++) {}
					for(itV = pV->m_rows[j].begin(); itV != endV && itV->first < (size_t)l; itV++) {}
					while(itU != endU && itV != endV)
					{
						if(itU->first < itV->first)
							itU++;
						else if(itV->first < itU->first)
							itV++;
						else
						{
							s += itU->second * itV->second;
							itU++;
							itV++;
						}
					}
					endV = pV->m_rows[i].end();
					for(itV = pV->m_rows[i].begin(); itV != endV && itV->first < (size_t)l; itV++) {}
					for(; itV != endV; itV++)
						pV->set(j, itV->first, pV->get(j, itV->first) + s * itV->second);
				}
			}
			for(j = l; j < n; j++)
			{
				pV->row(i)[j] = 0.0;
				pV->row(j)[i] = 0.0;
			}
		}
		pV->row(i)[i] = 1.0;
		g = temp[i];
		l = i;
	}

	// Accumulate left-hand transform
	for(int i = n - 1; i >= 0; i--)
	{
		l = i + 1;
		g = pSigma[i];
		if(i < n - 1)
		{
			for(j = l; j < n; j++)
			{
				pU->row(i)[j] = 0.0;
				pUT->row(j)[i] = 0.0;
			}
		}
		if(g != 0.0)
		{
			g = WSparseMatrix_safeDivide(1.0, g);
			if(i != n - 1)
			{
				for(j = l; j < n; j++)
				{
					s = 0.0;
					SparseVec::iterator end = pUT->m_rows[i].end();
					SparseVec::iterator end2 = pUT->m_rows[j].end();
					SparseVec::iterator it1, it2;
					for(it1 = pUT->m_rows[i].begin(); it1 != end && it1->first < (size_t)l; it1++) {}
					for(it2 = pUT->m_rows[j].begin(); it2 != end2 && it2->first < (size_t)l; it2++) {}
					while(it1 != end && it2 != end2)
					{
						if(it1->first < it2->first)
							it1++;
						else if(it2->first < it1->first)
							it2++;
						else
						{
							s += it1->second * it2->second;
							it1++;
							it2++;
						}
					}
					f = WSparseMatrix_safeDivide(s, pU->row(i)[i]) * g;
					for(it1 = pUT->m_rows[i].begin(); it1 != end && it1->first < (size_t)i; it1++) {}
					for(; it1 != end; it1++)
					{
						double t = f * it1->second;
						pU->row(it1->first)[j] += t;
						pUT->row(j)[it1->first] += t;
					}
				}
			}
			for(j = i; j < m; j++)
			{
				pU->row(j)[i] *= g;
				pUT->row(i)[j] *= g;
			}
		}
		else
		{
			for(j = i; j < m; j++)
			{
				pU->row(j)[i] = 0.0;
				pUT->row(i)[j] = 0.0;
			}
		}
		pU->row(i)[i] += 1.0;
		pUT->row(i)[i] += 1.0;
	}

	// Diagonalize the bidiagonal matrix
	std::set<size_t> indexes;
	for(k = n - 1; k >= 0; k--) // For each singular value
	{
		for(size_t iter = 1; iter <= maxIters; iter++)
		{
			// Test for splitting
			bool flag = true;
			for(l = k; l >= 0; l--)
			{
				q = l - 1;
				if(std::abs(temp[l]) + norm == norm)
				{
					flag = false;
					break;
				}
				if(std::abs(pSigma[q]) + norm == norm)
					break;
			}

			if(flag)
			{
				c = 0.0;
				s = 1.0;
				for(int i = l; i <= k; i++)
				{
					f = s * temp[i];
					temp[i] *= c;
					if(std::abs(f) + norm == norm)
						break;
					g = pSigma[i];
					h = WSparseMatrix_pythag(f, g);
					pSigma[i] = h;
					h = WSparseMatrix_safeDivide(1.0, h);
					c = g * h;
					s = -f * h;
					indexes.clear();
					SparseVec::iterator end = pUT->m_rows[i].end();
					for(SparseVec::iterator it = pUT->m_rows[i].begin(); it != end; it++)
						indexes.insert(it->first);
					end = pUT->m_rows[q].end();
					for(SparseVec::iterator it = pUT->m_rows[q].begin(); it != end; it++)
						indexes.insert(it->first);
					for(std::set<size_t>::iterator it = indexes.begin(); it != indexes.end(); it++)
					{
						y = pU->row(*it)[q];
						z = pU->row(*it)[i];
						pU->row(*it)[q] = y * c + z * s;
						pUT->row(q)[*it] = y * c + z * s;
						pU->row(*it)[i] = z * c - y * s;
						pUT->row(i)[*it] = z * c - y * s;
					}
				}
			}

			z = pSigma[k];
			if(l == k)
			{
				// Detect convergence
				if(z < 0.0)
				{
					// Singular value should be positive
					pSigma[k] = -z;
					for(j = 0; j < n; j++)
						pV->row(k)[j] *= -1.0;
				}
				break;
			}
			if(throwIfNoConverge && iter >= maxIters)
				throw Ex("failed to converge");

			// Shift from bottom 2x2 minor
			x = pSigma[l];
			q = k - 1;
			y = pSigma[q];
			g = temp[q];
			h = temp[k];
			f = WSparseMatrix_safeDivide(((y - z) * (y + z) + (g - h) * (g + h)), (2.0 * h * y));
			g = WSparseMatrix_pythag(f, 1.0);
			f = WSparseMatrix_safeDivide(((x - z) * (x + z) + h * (WSparseMatrix_safeDivide(y, (f + WSparseMatrix_takeSign(g, f))) - h)), x);

			// QR transform
			c = 1.0;
			s = 1.0;
			for(j = l; j <= q; j++)
			{
				int i = j + 1;
				g = temp[i];
				y = pSigma[i];
				h = s * g;
				g = c * g;
				z = WSparseMatrix_pythag(f, h);
				temp[j] = z;
				c = WSparseMatrix_safeDivide(f, z);
				s = WSparseMatrix_safeDivide(h, z);
				f = x * c + g * s;
				g = g * c - x * s;
				h = y * s;
				y = y * c;
				indexes.clear();
				SparseVec::iterator end = pV->m_rows[i].end();
				for(SparseVec::iterator it = pV->m_rows[i].begin(); it != end; it++)
					indexes.insert(it->first);
				end = pV->m_rows[j].end();
				for(SparseVec::iterator it = pV->m_rows[j].begin(); it != end; it++)
					indexes.insert(it->first);
				for(std::set<size_t>::iterator it = indexes.begin(); it != indexes.end(); it++)
				{
					x = pV->row(j)[*it];
					z = pV->row(i)[*it];
					pV->row(j)[*it] = x * c + z * s;
					pV->row(i)[*it] = z * c - x * s;
				}
				z = WSparseMatrix_pythag(f, h);
				pSigma[j] = z;
				if(z != 0.0)
				{
					z = WSparseMatrix_safeDivide(1.0, z);
					c = f * z;
					s = h * z;
				}
				f = c * g + s * y;
				x = c * y - s * g;
				indexes.clear();
				end = pUT->m_rows[i].end();
				for(SparseVec::iterator it = pUT->m_rows[i].begin(); it != end; it++)
					indexes.insert(it->first);
				end = pUT->m_rows[j].end();
				for(SparseVec::iterator it = pUT->m_rows[j].begin(); it != end; it++)
					indexes.insert(it->first);
				for(std::set<size_t>::iterator it = indexes.begin(); it != indexes.end(); it++)
				{
					y = pU->row(*it)[j];
					z = pU->row(*it)[i];
					pU->row(*it)[j] = y * c + z * s;
					pUT->row(j)[*it] = y * c + z * s;
					pU->row(*it)[i] = z * c - y * s;
					pUT->row(i)[*it] = z * c - y * s;
				}
			}
			temp[l] = 0.0;
			temp[k] = f;
			pSigma[k] = x;
		}
	}

	// Sort the singular values from largest to smallest
	for(int i = 1; i < n; i++)
	{
		for(j = i; j > 0; j--)
		{
			if(pSigma[j - 1] >= pSigma[j])
				break;
			pU->swapColumns(j - 1, j);
			pV->swapRows(j - 1, j);
			std::swap(pSigma[j - 1], pSigma[j]);
		}
	}

	// Return results
	*ppU = hU.release();
	*ppDiag = hSigma.release();
	*ppV = hV.release();
}

void WSparseMatrix::principalComponentAboutOrigin(WVec& outVector, WRand* pRand)
{
	if(m_defaultValue != 0.0)
		throw Ex("Expected the default value to be 0");

	// Initialize the out-vector to a random direction
	size_t dims = cols();
	outVector.resize(dims);
	outVector.fillSphericalShell(*pRand);

	// Iterate
	size_t nCount = rows();
	WVec pAccumulator(dims);
	double d;
	double mag = 0;
	for(size_t iters = 0; iters < 200; iters++)
	{
		pAccumulator.fill(0.0);
		for(size_t n = 0; n < nCount; n++)
		{
			Iter itEnd = rowEnd(n);
			double dd = 0.0;
			for(Iter it = rowBegin(n); it != itEnd; it++)
				dd += outVector[it->first] * it->second;
			for(Iter it = rowBegin(n); it != itEnd; it++)
				pAccumulator[it->first] += dd * it->second;
		}
		outVector.copy(pAccumulator);
		outVector.normalize();
		d = pAccumulator.squaredMagnitude();
		if(iters < 6 || d - mag > 1e-8)
			mag = d;
		else
			break;
	}
}

void WSparseMatrix::deleteLastRow()
{
	m_rows.pop_back();
}

void WSparseMatrix::resize(size_t rows, size_t cols)
{
	clear();
	m_cols = cols;
	m_rows.resize(rows);
}

bool WSparseMatrix_testHelper(WSparseMatrix& sm)
{
	WMatrix* fm = sm.toFullMatrix();
	std::unique_ptr<WMatrix> hFM(fm);

	// Do it with the full matrix
	WMatrix* pU;
	double* pDiag;
	WMatrix* pV;
	fm->singularValueDecomposition(&pU, &pDiag, &pV);
	std::unique_ptr<WMatrix> hU(pU);
	std::unique_ptr<double[]> hDiag(pDiag);
	std::unique_ptr<WMatrix> hV(pV);

	// Do it with the sparse matrix
	WSparseMatrix* pSU;
	double* pSDiag;
	WSparseMatrix* pSV;
	sm.singularValueDecomposition(&pSU, &pSDiag, &pSV);
	std::unique_ptr<WSparseMatrix> hSU(pSU);
	std::unique_ptr<double[]> hSDiag(pSDiag);
	std::unique_ptr<WSparseMatrix> hSV(pSV);

	// Check the results
	WMatrix* pV2 = pSV->toFullMatrix();
	std::unique_ptr<WMatrix> hV2(pV2);
	double err = pV2->sumSquaredDifference(*pV, false);
	if(err > 1e-6)
		return false;
	return true;
}

// static
void WSparseMatrix::test()
{
	WRand prng(0);
	size_t failures = 0;
	for(size_t i = 0; i < 100; i++)
	{
		size_t w = (size_t)prng.next(20) + 1;
		size_t h = (size_t)prng.next(20) + 1;
		WSparseMatrix m(h, w);
		for(size_t j = 0; j < 60; j++)
			m.set((size_t)prng.next(h), (size_t)prng.next(w), prng.normal());
		if(!WSparseMatrix_testHelper(m))
			failures++;
	}
	size_t tolerance = 0;
	if(sizeof(size_t) == 4)
		tolerance += 6; // on 32-bit machines there seem to be a small number of failures due to rounding error (I think)
	if(failures > tolerance)
		throw Ex("failed");
}





// static
size_t WSparseVec::indexOfMaxMagnitude(SparseVec& sparse)
{
	size_t i = 0;
	double mag = -1.0;
	for(SparseVec::iterator it = sparse.begin(); it != sparse.end(); it++)
	{
		double m = it->second * it->second;
		if(m > mag)
		{
			mag = m;
			i = it->first;
		}
	}
	return i;
}

// static
double WSparseVec::dotProduct(SparseVec& sparse, WVec& dense)
{
	double d = 0.0;
	for(SparseVec::iterator it = sparse.begin(); it != sparse.end(); it++)
		d += dense[it->first] * it->second;
	return d;
}

// static
double WSparseVec::dotProduct(SparseVec& a, SparseVec& b)
{
	double d = 0.0;
	SparseVec::iterator itA = a.begin();
	if(itA == a.end())
		return d;
	SparseVec::iterator itB = b.begin();
	if(itB == b.end())
		return d;
	while(true)
	{
		if(itA->first < itB->first)
		{
			itA++;
			if(itA == a.end())
				break;
		}
		else if(itB->first < itA->first)
		{
			itB++;
			if(itB == b.end())
				break;
		}
		else
		{
			d += itA->second * itB->second;
			itA++;
			itB++;
		}
	}
	return d;
}

// static
size_t WSparseVec::count_matching_elements(SparseVec& a, SparseVec& b)
{
	size_t count = 0;
	SparseVec::iterator itA = a.begin();
	SparseVec::iterator itB = b.begin();
	while(itA != a.end() && itB != b.end())
	{
		if(itA->first < itB->first)
			itA++;
		else if(itB->first < itA->first)
			itB++;
		else
		{
			itA++;
			itB++;
			count++;
		}
	}
	return count;
}



} // namespace WClasses

