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

#include "WVec.h"
#include "WRand.h"
#include "WError.h"
#include "WMatrix.h"
#include "WBits.h"
#include "WDom.h"
#include "WMath.h"
#include "WImage.h"
#include "WBitTable.h"
#include "WHolders.h"
#include "WTokenizer.h"
#include "WTime.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <queue>

namespace WClasses
{

using std::vector;
using std::string;

WVec::WVec(size_t n)
: m_size(n)
{
	if(n == 0)
		m_data = nullptr;
	else
		m_data = new double[n];
}

WVec::WVec(int n)
: m_size(n)
{
	if(n == 0)
		m_data = nullptr;
	else
		m_data = new double[n];
}

WVec::WVec(const std::initializer_list<double>& list)
: m_size(list.size())
{
	if(list.size() == 0)
		m_data = nullptr;
	else
		m_data = new double[list.size()];
	size_t i = 0;
	for(const double* it = list.begin(); it != list.end(); ++it)
	{
		m_data[i++] = *it;
	}
}

WVec::WVec(WDomNode* pNode)
: m_data(nullptr), m_size(0)
{
	deserialize(pNode);
}

WVec::~WVec()
{
	delete[] m_data;
}

void WVec::resize(size_t n)
{
	if(m_size == n)
		return;
	delete[] m_data;
	m_data = nullptr;
	m_size = 0;
	resize_implicit(n);
}

void WVec::resize_implicit(size_t n)
{
	if(m_size == n)
		return;
	if(m_size != 0)
		throw Ex("Implicit resizing from non-empty vector is not allowed. Call resize to make it explicit.");
	m_size = n;
	if(n == 0)
		m_data = nullptr;
	else
		m_data = new double[n];
}

void WVec::resizePreserve(size_t n)
{
	if(m_size == n)
		return;
	size_t overlap = std::min(m_size, n);
	double* oldData = m_data;
	m_size = n;
	if(n == 0)
		m_data = nullptr;
	else
		m_data = new double[n];
	for(size_t i = 0; i < overlap; i++)
		m_data[i] = oldData[i];
	delete[] oldData;
}

void WVec::fill(const double val, size_t startPos, size_t elements)
{
	size_t endPos = std::min(startPos + elements, m_size);
	for(size_t i = startPos; i < endPos; i++)
		m_data[i] = val;
}

WVec WVec::operator+(const WVec& that) const
{
	WAssert(size() == that.size());
	WVec v(m_size);
	for(size_t i = 0; i < m_size; i++)
		v[i] = (*this)[i] + that[i];
	return v;
}

WVec& WVec::operator+=(const WVec& that)
{
	WAssert(size() == that.size());
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] += that[i];
	return *this;
}

WVec& WVec::operator+=(const double scalar)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] += scalar;
	return *this;
}

WVec WVec::operator-(const WVec& that) const
{
	WAssert(size() == that.size());
	WVec v(m_size);
	for(size_t i = 0; i < m_size; i++)
		v[i] = (*this)[i] - that[i];
	return v;
}

WVec& WVec::operator-=(const WVec& that)
{
	WAssert(size() == that.size());
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] -= that[i];
	return *this;
}

WVec WVec::operator*(double scalar) const
{
	WVec v(m_size);
	for(size_t i = 0; i < m_size; i++)
		v[i] = (*this)[i] * scalar;
	return v;
}

WVec& WVec::operator*=(double scalar)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] *= scalar;
	return *this;
}

WVec& WVec::operator*=(const WVec& that)
{
	WAssert(size() == that.size());
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] *= that[i];
	return *this;
}

WVec& WVec::operator/=(double scalar)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] /= scalar;
	return *this;
}

void WVec::copy(const WVec& orig, size_t start, size_t size)
{
	WAssert(start <= orig.size());
	size = std::min(size, orig.size() - start);
	resize_implicit(size);
	for(size_t i = 0; i < size; i++)
		m_data[i] = orig[start + i];
}

void WVec::copy(const double* pSource, size_t n)
{
	resize_implicit(n);
	for(size_t i = 0; i < n; i++)
		(*this)[i] = *(pSource++);
}

void WVec::copy(size_t pos, const WVec& that, size_t start, size_t length)
{
	if(length == (size_t)-1)
		length = that.size() - start;
	else if(start + length > that.size())
		throw Ex("Input out of range. that size=", WClasses::to_str(that.size()), ", start=", WClasses::to_str(start), ", length=", WClasses::to_str(length));
	if(pos + length > m_size || start + length > that.m_size)
		throw Ex("Out of range. this size=", WClasses::to_str(m_size), ", pos=", WClasses::to_str(pos), ", that size=", WClasses::to_str(that.m_size));
	for(size_t i = 0; i < length; i++)
		(*this)[pos + i] = that[start + i];
}

double WVec::mean() const
{
	double m = 0.0;
	for(size_t i = 0; i < m_size; i++)
	{
		m += (*this)[i];
	}
	if(m_size > 0) // make sure there are elements.
		return m / m_size;
	else
		return 0.0;
}

double WVec::squaredMagnitude() const
{
	double s = 0.0;
	for(size_t i = 0; i < m_size; i++)
	{
		double d = (*this)[i];
		s += (d * d);
	}
	return s;
}

void WVec::normalize()
{
	double mag = std::sqrt(squaredMagnitude());
	if(mag < 1e-16)
		fill(std::sqrt(1.0 / m_size));
	else
		(*this) *= (1.0 / mag);
}

void WVec::sumToOne()
{
	double s = sum();
	if(s < 1e-16)
		fill(1.0 / m_size);
	else
		(*this) *= (1.0 / s);
}

double WVec::squaredDistance(const WVec& that) const
{
	WAssert(size() == that.size());
	double s = 0.0;
	for(size_t i = 0; i < m_size; i++)
	{
		double d = (*this)[i] - that[i];
		s += (d * d);
	}
	return s;
}

void WVec::fillUniform(WRand& rand, double min, double max)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] = rand.uniform() * (max - min) + min;
}

void WVec::fillNormal(WRand& rand, double deviation)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] = rand.normal() * deviation;
}

void WVec::perturbNormal(WRand& rand, double deviation)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] += rand.normal() * deviation;
}

void WVec::fillSphericalShell(WRand& rand, double radius)
{
	fillNormal(rand);
	normalize();
	if(radius != 1.0)
		(*this) *= radius;
}

void WVec::fillSphericalVolume(WRand& rand)
{
	fillSphericalShell(rand);
	(*this) *= std::pow(rand.uniform(), 1.0 / m_size);
}

void WVec::fillSimplex(WRand& rand)
{
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] = rand.exponential();
	(*this) *= (1.0 / sum());
}

void WVec::print(std::ostream& stream, char separator, size_t max_elements_per_line) const
{
	std::streamsize oldPrecision = stream.precision(14);
	if(m_size > 0)
		stream << (*this)[0];
	for(size_t i = 1; i < m_size; i++)
	{
		stream << separator;
		if(i % max_elements_per_line == 0)
			stream << "\n";
		stream << (*this)[i];
	}
	stream.precision(oldPrecision);
}

std::string WVec::to_str(char separator, size_t max_elements_per_line) const
{
	std::ostringstream ss;
	print(ss, separator, max_elements_per_line);
	return ss.str();
}

double WVec::sum() const
{
	double s = 0.0;
	for(size_t i = 0; i < m_size; i++)
		s += (*this)[i];
	return s;
}

size_t WVec::indexOfMax(size_t startPos, size_t endPos) const
{
	endPos = std::min(m_size, endPos);
	size_t maxIndex = startPos;
	double maxValue = -1e300;
	for(size_t i = startPos; i < endPos; i++)
	{
		if((*this)[i] > maxValue)
		{
			maxIndex = i;
			maxValue = (*this)[i];
		}
	}
	return maxIndex;
}

size_t WVec::indexOfMin(size_t startPos, size_t endPos) const
{
	endPos = std::min(m_size, endPos);
	size_t minIndex = startPos;
	double minValue = 1e300;
	for(size_t i = startPos; i < endPos; i++)
	{
		if((*this)[i] < minValue)
		{
			minIndex = i;
			minValue = (*this)[i];
		}
	}
	return minIndex;
}

double WVec::max(size_t startPos, size_t endPos) const
{
	endPos = std::min(m_size, endPos);
	double maxValue = -1e300;
	for(size_t i = startPos; i < endPos; i++)
	{
		if((*this)[i] > maxValue)
		{
			maxValue = (*this)[i];
		}
	}
	return maxValue;
}

double WVec::min(size_t startPos, size_t endPos) const
{
	endPos = std::min(m_size, endPos);
	double minValue = 1e300;
	for(size_t i = startPos; i < endPos; i++)
	{
		if((*this)[i] < minValue)
		{
			minValue = (*this)[i];
		}
	}
	return minValue;
}

WDomNode* WVec::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newList();
	for(size_t i = 0; i < m_size; i++)
		pNode->add(pDoc, (*this)[i]);
	return pNode;
}

void WVec::deserialize(const WDomNode* pNode)
{
	WDomListIterator it(pNode);
	resize_implicit(it.remaining());
	for(size_t i = 0; it.current(); i++)
	{
		(*this)[i] = it.currentDouble();
		it.advance();
	}
}

double WVec::dotProduct(const WVec& that) const
{
	WAssert(size() == that.size());
	double s = 0.0;
	for(size_t i = 0; i < m_size; i++)
		s += ((*this)[i] * that[i]);
	return s;
}

double WVec::dotProductIgnoringUnknowns(const WVec& that) const
{
	WAssert(size() == that.size());
	double s = 0.0;
	for(size_t i = 0; i < m_size; i++)
	{
		if((*this)[i] != UNKNOWN_REAL_VALUE && that[i] != UNKNOWN_REAL_VALUE)
			s += ((*this)[i] * that[i]);
	}
	return s;
}

double WVec::dotProductIgnoringUnknowns(const WVec& from, const WVec& to) const
{
	WAssert(size() == from.size());
	WAssert(size() == to.size());
	double s = 0.0;
	for(size_t i = 0; i < m_size; i++)
	{
		if((*this)[i] != UNKNOWN_REAL_VALUE && from[i] != UNKNOWN_REAL_VALUE && to[i] != UNKNOWN_REAL_VALUE)
			s += ((*this)[i] * (to[i] - from[i]));
	}
	return s;
}

double WVec::estimateSquaredDistanceWithUnknowns(const WVec& that) const
{
	WAssert(size() == that.size());
	double dist = 0;
	double d;
	size_t nMissing = 0;
	for(size_t n = 0; n < m_size; n++)
	{
		if((*this)[n] == UNKNOWN_REAL_VALUE || that[n] == UNKNOWN_REAL_VALUE)
			nMissing++;
		else
		{
			d = (*this)[n] - that[n];
			dist += (d * d);
		}
	}
	if(nMissing >= m_size)
		return 1e50; // we have no info, so let's make a wild guess
	else
		return dist * m_size / (m_size - nMissing);
}

void WVec::addScaled(double scalar, const WVec& that, size_t start, size_t size)
{
	WAssert(start + size <= that.size() || size == (size_t)-1);
	size = std::min(size, that.size() - start);
	WAssert(this->size() >= size);
	for(size_t i = 0; i < size; i++)
		(*this)[i] += (scalar * that[start + i]);
}

void WVec::addScaled(size_t startPos, double scalar, const WVec& that, size_t start, size_t size)
{
	WAssert(start + size <= that.size() || size == (size_t)-1);
	size = std::min(size, that.size() - start);
	WAssert(this->size() >= size + startPos);
	for(size_t i = 0; i < size; i++)
		(*this)[startPos + i] += (scalar * that[start + i]);
}

void WVec::regularize(double amount)
{
	regularizeL2(amount);
	regularizeL1(0.2 * amount);
}

void WVec::regularizeL1(double amount)
{
	for(size_t i = 0; i < m_size; i++)
	{
		if((*this)[i] < 0.0)
			(*this)[i] = std::min(0.0, (*this)[i] + amount);
		else
			(*this)[i] = std::max(0.0, (*this)[i] - amount);
	}
}

void WVec::regularizeL2(double amount)
{
	(*this) *= (1.0 - amount);
}

void WVec::erase(size_t start, size_t count)
{
	if(start + count > m_size)
		throw Ex("out of range");
	size_t end = m_size - count;
	for(size_t i = start; i < end; i++)
		(*this)[i] = (*this)[i + count];
	m_size -= count;
	if(m_size == 0)
	{
		delete(m_data);
		m_data = nullptr;
	}
}

double WVec::correlation(const WVec& that) const
{
	double d = this->dotProduct(that);
	if(d == 0.0)
		return 0.0;
	return d / (sqrt(this->squaredMagnitude() * that.squaredMagnitude()));
}

void WVec::clip(double min, double max)
{
	WAssert(max >= min);
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] = std::max(min, std::min(max, (*this)[i]));
}

void WVec::subtractComponent(const WVec& component)
{
	WAssert(size() == component.size());
	double comp = dotProduct(component);
	for(size_t i = 0; i < m_size; i++)
		(*this)[i] -= component[i] * comp;
}

void WVec::toImage(WImage* pImage, int width, int height, int channels, double range) const
{
	if(size() != (size_t)width * (size_t)height * (size_t)channels)
		throw Ex("Size mismatch");
	pImage->setSize(width, height);
	unsigned int* pix = pImage->pixels();
	if(channels == 3)
	{
		size_t pos = 0;
		for(int y = 0; y < height; y++)
		{
			for(int x = 0; x < width; x++)
			{
				int r = ClipChan((int)((*this)[pos++] * 256 / range));
				int g = ClipChan((int)((*this)[pos++] * 256 / range));
				int b = ClipChan((int)((*this)[pos++] * 256 / range));
				*(pix++) = gARGB(0xff, r, g, b);
			}
		}
	}
	else if(channels == 1)
	{
		size_t pos = 0;
		for(int y = 0; y < height; y++)
		{
			for(int x = 0; x < width; x++)
			{
				int v = ClipChan((int)((*this)[pos++] * 256 / range));
				*(pix++) = gARGB(0xff, v, v, v);
			}
		}
	}
	else
		throw Ex("unsupported value for channels");
}

void WVec::fromImage(WImage* pImage, int width, int height, int channels, double range)
{
	resize_implicit(width * height * channels);
	unsigned int* pix = pImage->pixels();
	if(channels == 3)
	{
		size_t pos = 0;
		for(int y = 0; y < height; y++)
		{
			for(int x = 0; x < width; x++)
			{
				(*this)[pos++] = gRed(*pix) * range / 255;
				(*this)[pos++] = gGreen(*pix) * range / 255;
				(*this)[pos++] = gBlue(*pix) * range / 255;
				pix++;
			}
		}
	}
	else if(channels == 1)
	{
		size_t pos = 0;
		for(int y = 0; y < height; y++)
		{
			for(int x = 0; x < width; x++)
			{
				(*this)[pos++] = gGray(*pix) * range / MAX_GRAY_VALUE;
				pix++;
			}
		}
	}
	else
		throw Ex("unsupported value for channels");
}

void WVec::swapContents(WVec& that)
{
	std::swap(m_data, that.m_data);
	std::swap(m_size, that.m_size);
}


// static
void WVec::test()
{
	{
		// Test some static methods
		WRand prng(0);
		WVec v1(100);
		WVec v2(100);
		for(int i = 0; i < 10; i++)
		{
			v1.fillSphericalShell(prng);
			v2.fillSphericalShell(prng);
			v2.subtractComponent(v1);
			v2.normalize();
			if(std::abs(v1.correlation(v2)) > 1e-4)
				throw Ex("Failed");
			if(std::abs(v1.squaredMagnitude() - 1) > 1e-4)
				throw Ex("Failed");
			if(std::abs(v2.squaredMagnitude() - 1) > 1e-4)
				throw Ex("Failed");
		}
	}

	// Test the basic operations of the WVec object
	WVec v1(2);
	v1[0] = 2.0;
	v1[1] = 7.0;
	WVec v2;
	v2.copy(v1);
	if(v2.size() != 2)
		throw Ex("failed");
	if(v1.squaredDistance(v2) != 0.0)
		throw Ex("failed");
	std::swap(v1[0], v1[1]);
	if(v1.squaredDistance(v2) != 50.0)
		throw Ex("failed");
	v2.fill(3.0);
	v1.copy(v2);
	if(v1.squaredMagnitude() != 18.0)
		throw Ex("failed");
	if(v1.data()[0] != 3.0 || v1.data()[1] != 3.0)
		throw Ex("failed");

	// Test overloaded operators
	v1[0] = 1.0;
	v1[1] = 2.0;
	v2[0] = 3.0;
	v2[1] = 4.0;
	WVec v3 = v1 + v2;
	if(v3.squaredMagnitude() != 52.0)
		throw Ex("failed");
	v3 += v1;
	if(v3.squaredMagnitude() != 89.0)
		throw Ex("failed");
	v1 *= 2.0;
	if(v1.squaredMagnitude() != 20.0)
		throw Ex("failed");
	v1 -= v2;
	if(v1[0] != -1.0)
		throw Ex("failed");
	if(v1[1] != 0.0)
		throw Ex("failed");

	// Test some other methods:
	WVec v4(10);
	WVec v5(10);
	for(size_t i = 0; i < 10; i++)
	{
		v4[i] = (double)i;
		v5[i] = (double)(20 - i);
	}
	double max = v4.max();
	if(max != 9)
		throw Ex("failed");
	max = v5.max();
	if(max != 20)
		throw Ex("failed");
	v4 += 2;
	if(v4[0] != 2 || v4[1] != 3 || v4[2] != 4)
		throw Ex("failed");
	double mean = v4.mean();
	if(mean != 6.5)
		throw Ex("failed");
	WVec v6(2);
	v6[0] = 10;
	v6[1] = 10;
	v6 /= 2;
	if(v6[0] != 5 || v6[1] != 5)
		throw Ex("failed");
}

std::string to_str(const WVec& v)
{
	return v.to_str();
}






WIndexVec::WIndexVec(size_t n)
: m_size(n)
{
	if(n == 0)
		m_data = nullptr;
	else
		m_data = new size_t[n];
}

WIndexVec::WIndexVec(const std::initializer_list<size_t>& list)
: m_size(list.size())
{
	if(list.size() == 0)
		m_data = nullptr;
	else
		m_data = new size_t[list.size()];
	size_t i = 0;
	for(const size_t* it = list.begin(); it != list.end(); ++it)
	{
		m_data[i++] = *it;
	}
}

WIndexVec::WIndexVec(const WIndexVec& copyMe)
{
	if(copyMe.size() == 0)
		m_data = nullptr;
	else
		m_data = new size_t[copyMe.m_size];
	m_size = copyMe.m_size;
	memcpy(m_data, copyMe.m_data, sizeof(size_t) * m_size);
}

WIndexVec::WIndexVec(WDomNode* pNode)
: m_data(nullptr), m_size(0)
{
	WDomListIterator it(pNode);
	resize(it.remaining());
	for(size_t i = 0; it.current(); i++)
	{
		(*this)[i] = it.currentInt();
		it.advance();
	}
}

WIndexVec::~WIndexVec()
{
	delete[] m_data;
}

WDomNode* WIndexVec::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newList();
	for(size_t i = 0; i < m_size; i++)
		pNode->add(pDoc, (*this)[i]);
	return pNode;
}

void WIndexVec::resize(size_t n)
{
	delete[] m_data;
	m_size = n;
	if(n == 0)
		m_data = nullptr;
	else
		m_data = new size_t[n];
}

void WIndexVec::fill(size_t val)
{
	for(size_t i = 0; i < m_size; i++)
		m_data[i] = val;
}

void WIndexVec::fillIndexes()
{
	for(size_t i = 0; i < m_size; i++)
		m_data[i] = i;
}

size_t WIndexVec::popRandom(WRand& rand)
{
	if(m_size == 0)
		throw Ex("Nothing to pop");
	size_t r = rand.next(m_size);
	size_t v = m_data[r];
	m_data[r] = m_data[m_size - 1];
	if(--m_size == 0)
	{
		delete[] m_data;
		m_data = nullptr;
	}
	return v;
}

void WIndexVec::erase(size_t start, size_t count)
{
	if(start + count > m_size)
		throw Ex("out of range");
	size_t end = m_size - count;
	for(size_t i = start; i < end; i++)
		(*this)[i] = (*this)[i + count];
	m_size -= count;
	if(m_size == 0)
	{
		delete(m_data);
		m_data = nullptr;
	}
}

void WIndexVec::append(size_t count)
{
	size_t* pNewData = new size_t[m_size + count];
	memcpy(pNewData, m_data, sizeof(size_t) * m_size);
	m_size += count;
	delete[] m_data;
	m_data = pNewData;
}





// static
void WIndexVec::makeIndexVec(size_t* pVec, size_t size)
{
	for(size_t i = 0; i < size; i++)
	{
		*pVec = i;
		pVec++;
	}
}

// static
void WIndexVec::shuffle(size_t* pVec, size_t size, WRand* pRand)
{
	for(size_t i = size; i > 1; i--)
	{
		size_t r = (size_t)pRand->next(i);
		size_t t = pVec[i - 1];
		pVec[i - 1] = pVec[r];
		pVec[r] = t;
	}
}

// static
void WIndexVec::setAll(size_t* pVec, size_t value, size_t size)
{
	while(size > 0)
	{
		*pVec = value;
		pVec++;
		size--;
	}
}

// static
void WIndexVec::copy(size_t* pDest, const size_t* pSource, size_t nDims)
{
	memcpy(pDest, pSource, sizeof(size_t) * nDims);
}

// static
size_t WIndexVec::maxValue(size_t* pVec, size_t size)
{
	size_t m = *(pVec++);
	size--;
	while(size > 0)
	{
		m = std::max(m, *(pVec++));
		size--;
	}
	return m;
}

// static
size_t WIndexVec::indexOfMax(size_t* pVec, size_t size)
{
	size_t index = 0;
	size_t m = *(pVec++);
	size--;
	size_t i = 1;
	while(size > 0)
	{
		if(*pVec > m)
		{
			m = *pVec;
			index = i;
		}
		pVec++;
		size--;
		i++;
	}
	return index;
}

// static
WDomNode* WIndexVec::serialize(WDom* pDoc, const size_t* pVec, size_t dims)
{
	WDomNode* pNode = pDoc->newList();
	for(size_t i = 0; i < dims; i++)
		pNode->add(pDoc, *(pVec++));
	return pNode;
}

// static
void WIndexVec::deserialize(size_t* pVec, WDomListIterator& it)
{
	while(it.current())
	{
		*(pVec++) = size_t(it.currentInt());
		it.advance();
	}
}

// static
void WIndexVec::print(std::ostream& stream, size_t* pVec, size_t dims)
{
	if(dims == 0)
		return;
	stream << *pVec;
	pVec++;
	for(size_t i = 1; i < dims; i++)
	{
		stream << ", ";
		stream << *pVec;
		pVec++;
	}
}




WRandomIndexIterator::WRandomIndexIterator(size_t len, WRand& rnd)
: m_length(len), m_rand(rnd)
{
	m_pIndexes = new size_t[len];
	size_t* pInd = m_pIndexes;
	for(size_t i = 0; i < len; i++)
		*(pInd++) = i;
	m_pEnd = m_pIndexes + len;
	m_pCur = m_pEnd;
}

WRandomIndexIterator::~WRandomIndexIterator()
{
	delete[] m_pIndexes;
}

void WRandomIndexIterator::reset()
{
	for(size_t i = m_length; i > 1; i--)
		std::swap(m_pIndexes[i - 1], m_pIndexes[m_rand.next(i)]);
	m_pCur = m_pIndexes;
}

void WRandomIndexIterator::resetPart(size_t size)
{
	for(size_t i = size; i > 1; i--)
		std::swap(m_pIndexes[i - 1], m_pIndexes[m_rand.next(i)]);
	m_pCur = m_pIndexes;
}

bool WRandomIndexIterator::next(size_t& outIndex)
{
	if(m_pCur == m_pEnd)
		return false;
	outIndex = *(m_pCur++);
	return true;
}





WCoordVectorIterator::WCoordVectorIterator(size_t dimCount, size_t* pRanges)
{
	m_pCoords = nullptr;
	reset(dimCount, pRanges);
}

WCoordVectorIterator::WCoordVectorIterator(vector<size_t>& range)
{
	m_pCoords = nullptr;
	reset(range);
}

WCoordVectorIterator::~WCoordVectorIterator()
{
	delete[] m_pCoords;
}

void WCoordVectorIterator::reset()
{
	memset(m_pCoords, '\0', sizeof(size_t) * m_dims);
	m_sampleShift = INVALID_INDEX;
}

void WCoordVectorIterator::reset(size_t dimCount, size_t* pRanges)
{
	m_dims = dimCount;
	delete[] m_pCoords;
	if(dimCount > 0)
	{
		m_pCoords = new size_t[2 * dimCount];
		m_pRanges = m_pCoords + dimCount;
		if(pRanges)
			memcpy(m_pRanges, pRanges, sizeof(size_t) * dimCount);
		else
		{
			for(size_t i = 0; i < dimCount; i++)
				m_pRanges[i] = 1;
		}
	}
	else
	{
		m_pCoords = nullptr;
		m_pRanges = nullptr;
	}
	reset();
}

void WCoordVectorIterator::reset(vector<size_t>& range)
{
	m_dims = range.size();
	delete[] m_pCoords;
	if(m_dims > 0)
	{
		m_pCoords = new size_t[2 * m_dims];
		m_pRanges = m_pCoords + m_dims;
		for(size_t i = 0; i < m_dims; i++)
			m_pRanges[i] = range[i];
	}
	else
	{
		m_pCoords = nullptr;
		m_pRanges = nullptr;
	}
	reset();
}

size_t WCoordVectorIterator::coordCount()
{
	size_t n = 1;
	size_t* pR = m_pRanges;
	for(size_t i = 0; i < m_dims; i++)
		n *= (*(pR++));
	return n;
}

bool WCoordVectorIterator::advance()
{
	size_t j;
	for(j = 0; j < m_dims; j++)
	{
		if(++m_pCoords[j] >= m_pRanges[j])
			m_pCoords[j] = 0;
		else
			break;
	}

	// Test if we're done
	if(j >= m_dims)
		return false;
	return true;
}

bool WCoordVectorIterator::advance(size_t steps)
{
	size_t j;
	for(j = 0; j < m_dims; j++)
	{
		size_t t = m_pCoords[j] + steps;
		m_pCoords[j] = t % m_pRanges[j];
		steps = t / m_pRanges[j];
		if(t == 0)
			break;
	}

	// Test if we're done
	if(j >= m_dims)
		return false;
	return true;
}

bool WCoordVectorIterator::advanceSampling()
{
	if(m_sampleShift == INVALID_INDEX) // if we have not yet computed the step size
	{
		size_t r = m_pRanges[0];
		for(size_t i = 1; i < m_dims; i++)
			r = std::max(r, m_pRanges[i]);
		m_sampleShift = WBits::boundingShift(r);
		m_sampleMask = 0;
	}

	m_pCoords[0] += ((size_t)1 << (m_sampleShift + (m_sampleMask ? 0 : 1)));
	if(m_pCoords[0] >= m_pRanges[0])
	{
		m_pCoords[0] = 0;
		size_t j = 1;
		for(; j < m_dims; j++)
		{
			m_pCoords[j] += ((size_t)1 << m_sampleShift);
			m_sampleMask ^= ((size_t)1 << j);
			if(m_pCoords[j] < m_pRanges[j])
				break;
			m_pCoords[j] = 0;
			m_sampleMask &= ~((size_t)1 << j);
		}
		if(j >= m_dims)
		{
			if(--m_sampleShift == INVALID_INDEX) // if we're all done
				return false;
		}
		if(m_sampleMask == 0)
		{
			m_pCoords[0] -= ((size_t)1 << m_sampleShift);
			return advanceSampling();
		}
	}
	return true;
}

size_t* WCoordVectorIterator::current()
{
	return m_pCoords;
}

void WCoordVectorIterator::currentNormalized(double* pCoords)
{
	for(size_t i = 0; i < m_dims; i++)
	{
		*pCoords = ((double)m_pCoords[i] + 0.5) / m_pRanges[i];
		pCoords++;
	}
}

size_t WCoordVectorIterator::currentIndex()
{
	size_t index = 0;
	size_t n = 1;
	for(size_t i = 0; i < m_dims; i++)
	{
		index += n * m_pCoords[i];
		n *= m_pRanges[i];
	}
	return index;
}

void WCoordVectorIterator::setRandom(WRand* pRand)
{
	for(size_t i = 0; i < m_dims; i++)
		m_pCoords[i] = (size_t)pRand->next(m_pRanges[i]);
}

#define TEST_DIMS 4
// static
void WCoordVectorIterator::test()
{
	size_t r = 11;
	size_t size = 1;
	for(size_t i = 0; i < TEST_DIMS; i++)
		size *= r;
	WBitTable bt(size);
	size_t ranges[TEST_DIMS];
	for(size_t i = 0; i < TEST_DIMS; i++)
		ranges[i] = r;
	WCoordVectorIterator cvi(TEST_DIMS, ranges);
	size_t count = 0;
	while(true)
	{
		size_t index = cvi.currentIndex();
		if(bt.bit(index))
			throw Ex("already got this one");
		bt.set(index);
		count++;
		if(!cvi.advanceSampling())
			break;
	}
	if(count != size)
		throw Ex("didn't get them all");
}






size_t WTensor_countTensorSize(const WIndexVec& shape)
{
	size_t n = 1;
	for(size_t i = 0; i < shape.size(); i++)
		n *= shape[i];
	return n;
}

WTensor::WTensor(const std::initializer_list<size_t>& list, bool ownBuffer, WVec* pBuffer)
: shape(list.size()), pHolder(nullptr)
{
	// Populate the shape
	size_t i = 0;
	size_t tot = 1;
	for(const size_t* it = list.begin(); it != list.end(); ++it)
	{
		shape[i++] = *it;
		tot *= *it;
	}

	// Arrange for the buffer
	if(pBuffer && tot != pBuffer->size())
		throw Ex("Mismatching sizes. pBuffer has ", WClasses::to_str(pBuffer->size()), ", but the shape requires ", WClasses::to_str(tot));
	if(ownBuffer)
	{
		if(pBuffer)
			pHolder = pBuffer;
		else
			pHolder = new WVec(tot);
		setData(*pHolder);
	}
	else
	{
		if(pBuffer)
			setData(*pBuffer);
	}
}

WTensor::WTensor(const WTensor& copyMe)
: WVecWrapper(copyMe),
  shape(copyMe.shape), pHolder(nullptr)
{
}

WTensor::WTensor(WDomNode* pNode)
: WVecWrapper(nullptr, 0),
  shape(pNode), pHolder(nullptr)
{
}

WTensor::~WTensor()
{
	delete(pHolder);
}

void WTensor::get(WVecWrapper& out, size_t index)
{
	size_t len = 1;
	for(size_t i = 1; i < shape.size(); i++)
		len *= shape[i];
	out.setData(*this, index * shape[0], len);
}


class WTensorAttr
{
public:
	std::string m_name;
	std::vector<std::string> m_values;

	WDomNode* serialize(WDom* pDoc, size_t valCount) const;
};

class WTensorMeta
{
public:
	std::vector<WTensorAttr*> m_attrs;


	~WTensorMeta()
	{
		for(size_t i = 0; i < m_attrs.size(); i++)
			delete(m_attrs[i]);
	}

	// Adds a column named name, or attr_<index> if the name is empty (as WArffRelation does)
	WTensorAttr* addAttr(const string& name)
	{
		WTensorAttr* pAttr = new WTensorAttr();
		pAttr->m_name = name.length() > 0 ? name : "attr_" + to_str(m_attrs.size());
		m_attrs.push_back(pAttr);
		return pAttr;
	}

	// The column's name and number, for error messages
	string describe(size_t nAttr) const
	{
		return m_attrs[nAttr]->m_name + " (" + to_str(nAttr) + ")";
	}

	void parseAttribute(WTokenizer& tok, WCharSet& cs_whitespace, WCharSet& cs_spaces, WCharSet& cs_argEnd, WCharSet& cs_valEnd, WCharSet& cs_newline)
	{
		tok.skipWhile(cs_spaces);
		string dataname = tok.readUntil_escaped_quoted(cs_argEnd);
		tok.skipWhile(cs_spaces);
		char c = tok.peek();
		if(c == '{')
		{
			tok.skip(1);
			WTensorAttr* pAttr = addAttr(dataname);
			while(true)
			{
				tok.readUntil_escaped_quoted(cs_valEnd);
				char* szVal = tok.trim(cs_whitespace);
				if(*szVal == '\0')
					throw Ex("Empty value specified on line ", to_str(tok.line()));
				pAttr->m_values.push_back(szVal);
				char c2 = tok.peek();
				if(c2 == ',')
					tok.skip(1);
				else if(c2 == '}')
					break;
				else if(c2 == '\n')
					throw Ex("Expected a '}' but got new-line on line ", to_str(tok.line()));
				else
					throw Ex("inconsistency");
			}
		}
		else
		{
			const char* szType = tok.readUntil(cs_whitespace);
			if(_stricmp(szType, "CONTINUOUS") == 0 || _stricmp(szType, "REAL") == 0 || _stricmp(szType, "NUMERIC") == 0 || _stricmp(szType, "INTEGER") == 0)
			{
				addAttr(dataname);
			}
			else
				throw Ex("Unsupported attribute type: (", szType, "), at line ", to_str(tok.line()));
		}
		tok.skipUntil(cs_newline);
		tok.skip(1);
	}

	static string stripQuotes(string& s)
	{
		if(s.length() < 2)
			return s;
		if(s[0] == '"' && s[s.length() - 1] == '"')
			return s.substr(1, s.length() - 2);
		if(s[0] == '\'' && s[s.length() - 1] == '\'')
			return s.substr(1, s.length() - 2);
		return s;
	}

	int findEnumeratedValue(size_t nAttr, const char* szValue) const
	{
		size_t nValueCount = m_attrs[nAttr]->m_values.size();
		size_t i;
		bool quotedCand = false;
		for(i = 0; i < nValueCount; i++)
		{
			const char* szCand = m_attrs[nAttr]->m_values[i].c_str();
			if(_stricmp(szCand, szValue) == 0)
				return (int)i;
			if(*szCand == '"' || *szCand == '\'')
				quotedCand = true;
		}
		if(quotedCand || *szValue == '"' || *szValue == '\'')
		{
			string sValue = szValue;
			if(sValue.length() > 0 && (sValue[0] == '"' || sValue[0] == '\''))
				sValue = stripQuotes(sValue);
			for(i = 0; i < nValueCount; i++)
			{
				string sCand = m_attrs[nAttr]->m_values[i].c_str();
				if(sCand.length() > 0 && (sCand[0] == '"' || sCand[0] == '\''))
					sCand = stripQuotes(sCand);
				if(sCand.compare(sValue) == 0)
					return (int)i;
			}
		}
		return UNKNOWN_DISCRETE_VALUE;
	}
};


double WTensor_parseValue(WTensorMeta& relation, size_t col, const char* szVal, WTokenizer& tok)
{
	size_t vals = relation.m_attrs[col]->m_values.size();
	if(vals == 0) // Continuous
	{
		// Continuous attribute
		if(*szVal == '\0' || (*szVal == '?' && szVal[1] == '\0'))
			return UNKNOWN_REAL_VALUE;
		else
		{
			double d;
			if(!parseNumber(szVal, &d)) // SAKBox: was isRealValue, which checked only the start ("1.5abc" read as 1.5)
				throw Ex("Expected a numeric value for attribute ", relation.describe(col), " at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			return d;
		}
	}
	else if(vals < (size_t)-10) // Nominal
	{
		// Nominal attribute
		if(*szVal == '\0' || (*szVal == '?' && szVal[1] == '\0'))
			return UNKNOWN_DISCRETE_VALUE;
		else
		{
			int nVal = relation.findEnumeratedValue(col, szVal);
			if(nVal == UNKNOWN_DISCRETE_VALUE)
				throw Ex("Unrecognized enumeration value '", szVal, "' for attribute ", relation.describe(col), " at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			return (double)nVal;
		}
	}
	else
		throw Ex("Unexpected attribute type, ", to_str(vals));
}

void WTensor::loadArff(const char* szFilename)
{
	WTokenizer tok(szFilename);
	parseArff(tok);
}

void WTensor::parseArff(WTokenizer& tok)
{
	delete(pHolder);
	pHolder = nullptr;
	WCharSet cs_whitespace("\t\n\r ");
	WCharSet cs_spaces(" \t");
	WCharSet cs_space(" ");
	WCharSet cs_valEnd(",}\n");
	WCharSet cs_valEnder(" ,\t}\n");
	WCharSet cs_valHardEnder(",}\t\n");
	WCharSet cs_argEnd(" \t\n{\r");
	WCharSet cs_newline("\n");
	WCharSet cs_commaNewlineTab(",\n\t");

	// Parse the meta data
	WTensorMeta meta;
	while(true)
	{
		tok.skipWhile(cs_whitespace);
		char c = tok.peek();
		if(c == '\0')
			throw Ex("Invalid ARFF file--contains no data");
		else if(c == '%')
		{
			tok.skip(1);
			tok.skipUntil(cs_newline);
		}
		else if(c == '@')
		{
			tok.skip(1);
			const char* szTok = tok.readUntil(cs_whitespace);
			if(_stricmp(szTok, "ATTRIBUTE") == 0)
				meta.parseAttribute(tok, cs_whitespace, cs_spaces, cs_argEnd, cs_valEnd, cs_newline);
			else if(_stricmp(szTok, "RELATION") == 0)
			{
				tok.skipWhile(cs_spaces);
				tok.readUntil_escaped_quoted(cs_whitespace);
				tok.skip(1);
			}
			else if(_stricmp(szTok, "DATA") == 0)
			{
				tok.skipUntil(cs_newline);
				tok.skip(1);
				break;
			}
		}
		else
			throw Ex("Expected a '%' or a '@' at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
	}

	// Parse the data
	std::queue<double> data_buf;
	size_t colCount = meta.m_attrs.size();
	if(colCount == 0)
		throw Ex("Invalid ARFF file--it has no attributes");
	while(true)
	{
		tok.skipWhile(cs_whitespace);
		char c = tok.peek();
		if(c == '\0')
			break;
		else if(c == '%')
		{
			tok.skip(1);
			tok.skipUntil(cs_newline);
		}
		else if(c == '{')
		{
			// Parse ARFF sparse data format
			tok.skip(1);
			while(true)
			{
				tok.skipWhile(cs_space);
				char c2 = tok.peek();
				if(c2 >= '0' && c2 <= '9')
				{
					const char* szTok = tok.readUntil(cs_valEnder);
#ifdef WINDOWS
					size_t column = (size_t)_strtoui64(szTok, (char**)NULL, 10);
#else
					size_t column = strtoull(szTok, (char**)nullptr, 10);
#endif
					if(column >= colCount)
						throw Ex("Column index out of range at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
					tok.skipWhile(cs_spaces);
					const char* szVal = tok.readUntil_escaped_quoted(cs_valEnder);
					data_buf.push(WTensor_parseValue(meta, column, szVal, tok));
					tok.skipUntil(cs_valHardEnder);
					c2 = tok.peek();
					if(c2 == ',' || c2 == '\t')
						tok.skip(1);
				}
				else if(c2 == '}')
				{
					tok.skip(1);
					break;
				}
				else if(c2 == '\n' || c2 == '\0')
					throw Ex("Expected a matching '}' at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
				else
					throw Ex("Unexpected token at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			}
		}
		else
		{
			// Parse ARFF dense data format
			size_t column = 0;
			while(true)
			{
				if(column >= colCount)
					throw Ex("Too many values on line ", to_str(tok.line()), ", col ", to_str(tok.col()));
				tok.readUntil_escaped_quoted(cs_commaNewlineTab);
				const char* szVal = tok.trim(cs_whitespace);
				data_buf.push(WTensor_parseValue(meta, column, szVal, tok));
				column++;
				char c2 = tok.peek();
				while(c2 == '\t' || c2 == ' ')
				{
					tok.skip(1);
					c2 = tok.peek();
				}
				if(c2 == ',')
					tok.skip(1);
				else if(c2 == '\n' || c2 == '\0')
					break;
				else if(c2 == '%')
				{
					tok.skip(1);
					tok.skipUntil(cs_newline);
					break;
				}
			}
			if(column < colCount)
				throw Ex("Not enough values on line ", to_str(tok.line()), ", col ", to_str(tok.col()));
		}
	}

	// Fill the tensor with data
	size_t rowCount = data_buf.size() / colCount;
	if(rowCount * colCount != data_buf.size())
		throw Ex("Unexpected number of elements");
	shape.resize(2);
	shape[0] = rowCount;
	shape[1] = colCount;
	pHolder = new WVec(data_buf.size());
	setData(*pHolder);
	for(size_t i = 0; !data_buf.empty(); i++)
	{
		(*this)[i] = data_buf.front();
		data_buf.pop();
	}
}

void WTensor::convolve(const WTensor& in, const WTensor& filter, WTensor& out, bool flipFilter, size_t stride)
{
	// Precompute some values
	size_t dc = in.shape.size();
	WAssert(dc == filter.shape.size());
	WAssert(dc == out.shape.size());
	size_t* kinner = (size_t*)alloca(sizeof(size_t) * 5 * dc);
	size_t* kouter = kinner + dc;
	size_t* stepInner = kouter + dc;
	size_t* stepFilter = stepInner + dc;
	size_t* stepOuter = stepFilter + dc;

	// Compute step sizes
	stepInner[0] = 1;
	stepFilter[0] = 1;
	stepOuter[0] = 1;
	for(size_t i = 1; i < dc; i++)
	{
		stepInner[i] = stepInner[i - 1] * in.shape[i - 1];
		stepFilter[i] = stepFilter[i - 1] * filter.shape[i - 1];
		stepOuter[i] = stepOuter[i - 1] * out.shape[i - 1];
	}
	size_t filterTail = stepFilter[dc - 1] * filter.shape[dc - 1] - 1;

	// Do convolution
	size_t op = 0;
	size_t ip = 0;
	size_t fp = 0;
	for(size_t i = 0; i < dc; i++)
	{
		kouter[i] = 0;
		kinner[i] = 0;
		ssize_t padding = (stride * (out.shape[i] - 1) + filter.shape[i] - in.shape[i]) / 2;
		ssize_t adj = (padding - std::min(padding, (ssize_t)kouter[i])) - kinner[i];
		kinner[i] += adj;
		fp += adj * stepFilter[i];
	}
	while(true) // kouter
	{
		double val = 0.0;

		// Fix up the initial kinner positions
		for(size_t i = 0; i < dc; i++)
		{
			ssize_t padding = (stride * (out.shape[i] - 1) + filter.shape[i] - in.shape[i]) / 2;
			ssize_t adj = (padding - std::min(padding, (ssize_t)kouter[i])) - kinner[i];
			kinner[i] += adj;
			fp += adj * stepFilter[i];
			ip += adj * stepInner[i];
		}
		while(true) // kinner
		{
			val += (in[ip] * filter[flipFilter ? filterTail - fp : fp]);

			// increment the kinner position
			size_t i;
			for(i = 0; i < dc; i++)
			{
				kinner[i]++;
				ip += stepInner[i];
				fp += stepFilter[i];
				ssize_t padding = (stride * (out.shape[i] - 1) + filter.shape[i] - in.shape[i]) / 2;
				if(kinner[i] < filter.shape[i] && kouter[i] + kinner[i] - padding < in.shape[i])
					break;
				ssize_t adj = (padding - std::min(padding, (ssize_t)kouter[i])) - kinner[i];
				kinner[i] += adj;
				fp += adj * stepFilter[i];
				ip += adj * stepInner[i];
			}
			if(i >= dc)
				break;
		}
		out[op] += val;

		// increment the kouter position
		size_t i;
		for(i = 0; i < dc; i++)
		{
			kouter[i]++;
			op += stepOuter[i];
			ip += stride * stepInner[i];
			if(kouter[i] < out.shape[i])
				break;
			op -= kouter[i] * stepOuter[i];
			ip -= kouter[i] * stride * stepInner[i];
			kouter[i] = 0;
		}
		if(i >= dc)
			break;
	}
}

// static
void WTensor::test()
{
	{
		// 1D test
		WVec in({2, 3, 1, 0, 1});
		WTensor tin({5}, false, &in);

		WVec k({1, 0, 2});
		WTensor tk({3}, false, &k);

		WVec out(7);
		out.fill(0.0);
		WTensor tout({7}, false, &out);

		WTensor::convolve(tin, tk, tout, true, 1);

		//     2 3 1 0 1
		// 2 0 1 --->
		WVec expected({2, 3, 5, 6, 3, 0, 2});
		if(std::sqrt(out.squaredDistance(expected)) > 1e-10)
			throw Ex("wrong");
	}

	{
		// 2D test
		WVec in(
		  {1, 2, 3, 4, 5, 6, 7, 8, 9});
		WTensor tin({3, 3}, false, &in);

		WVec k(
		  {1, 2, 1, 0, 0, 0, -1, -2, -1});
		WTensor tk({3, 3}, false, &k);

		WVec out(9);
		out.fill(0.0);
		WTensor tout({3, 3}, false, &out);

		WTensor::convolve(tin, tk, tout, false, 1);

		WVec expected(
		  {-13, -20, -17, -18, -24, -18, 13, 20, 17});
		if(std::sqrt(out.squaredDistance(expected)) > 1e-10)
			throw Ex("wrong");
	}
}



} // namespace WClasses
