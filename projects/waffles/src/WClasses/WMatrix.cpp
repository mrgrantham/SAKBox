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

#include "WMatrix.h"
#include "WError.h"
#include "WAssignment.h"
#include "WMath.h"
#include "WDistribution.h"
#include "WFile.h"
#include "WHashTable.h"
#include "WBits.h"
#include "WNeighborFinder.h"
#include "WDistance.h"
#include "WVec.h"
#include "WHeap.h"
#include "WDom.h"
#include <math.h>
#include "WLearner.h"
#include "WRand.h"
#include "WTokenizer.h"
#include "WTime.h"
#include <algorithm>
#include <functional>
#include <fstream>
#include <sstream>
#include <cmath>
#include <set>
#include <errno.h>
#include <map>
#include <memory>
#include <iostream>

using std::vector;
using std::string;
using std::ostream;
using std::ostringstream;

namespace WClasses
{

// static
WUniformRelation g_emptyRelation(0, 0);

// static
WRelation* WRelation::deserialize(const WDomNode* pNode)
{
	if(pNode->getIfExists("name"))
		return new WArffRelation(pNode);
	else if(pNode->getIfExists("attrs"))
		return new WUniformRelation(pNode);
	else
		return new WMixedRelation(pNode);
}

void WRelation::print(ostream& stream) const
{
	// Write the relation title
	stream << "@RELATION ";
	if(type() == ARFF)
	{
		std::string name = ((WArffRelation*)this)->name();
		if(name.find(' ') != string::npos)
			stream << "'" << name << "'";
		else
			stream << name;
	}
	else
		stream << "Untitled";
	stream << "\n\n";

	// Write the attributes
	for(size_t i = 0; i < size(); i++)
	{
		stream << "@ATTRIBUTE ";
		printAttrName(stream, i);
		stream << " ";
		size_t vals = valueCount(i);
		if(vals == 0 || vals == (size_t)-2) // continuous, or a date (stored, and saved, as a number)
			stream << "real";
		else if(vals < (size_t)-10) // nominal
		{
			stream << "{";
			for(size_t j = 0; j < valueCount(i); j++)
			{
				if(j > 0)
					stream << ",";
				printAttrValue(stream, i, (double)j);
			}
			stream << "}";
		}
		else if(vals == (size_t)-1) // string
			stream << "string";
		stream << "\n";
	}

	// Write the data
	stream << "\n@DATA\n";
}

// virtual
void WRelation::printAttrName(std::ostream& stream, size_t column) const
{
	stream << "attr_" << column;
}

// virtual
void WRelation::printAttrValue(ostream& stream, size_t column, double value, const char* missing) const
{
	size_t valCount = valueCount(column);
	if(valCount == 0) // continuous
	{
		if(value == UNKNOWN_REAL_VALUE)
			stream << "?";
		else
			stream << value;
	}
	else if(valCount < (size_t)-10) // nominal
	{
		int val = (int)value;
		if(val < 0)
			stream << "?";
		else if(val >= (int)valCount)
			throw Ex("value out of range");
		else if(val < 26)
		{
			char tmp[2];
			tmp[0] = 'a' + val;
			tmp[1] = '\0';
			stream << tmp;
		}
		else
			stream << "_" << val;
	}
	else if(valCount == (size_t)-1) // string
	{
		stream << "<string>";
	}
	else if(valCount == (size_t)-2) // date
		stream << value;
	else
		throw Ex("Unexpected attribute type");
}

// virtual
bool WRelation::isCompatible(const WRelation& that) const
{
	if(this == &that)
		return true;
	if(size() != that.size())
		return false;
	if(type() == UNIFORM && that.type() == UNIFORM)
	{
		if(valueCount(0) != that.valueCount(0))
			return false;
	}
	else
	{
		for(size_t i = 0; i < size(); i++)
		{
			if(valueCount(i) != that.valueCount(i))
				return false;
		}
	}
	return true;
}

void WRelation::printRow(ostream& stream, const double* pRow, char separator, const char* missing) const
{
	size_t j = 0;
	if(j < size())
	{
		printAttrValue(stream, j, *pRow, missing);
		pRow++;
		j++;
	}
	for(; j < size(); j++)
	{
		stream << separator;
		printAttrValue(stream, j, *pRow, missing);
		pRow++;
	}
	stream << "\n";
}

void WRelation::save(const WMatrix* pData, const char* szFilename) const
{
	std::ofstream stream;
	stream.exceptions(std::ios::badbit | std::ios::failbit);
	try
	{
		stream.open(szFilename, std::ios::binary);
	}
	catch(const std::exception&)
	{
		throw Ex("Error while trying to create the file, ", szFilename, ". ", strerror(errno));
	}
	pData->print(stream);
}

//static
void WRelation::test()
{
}










WUniformRelation::WUniformRelation(const WDomNode* pNode)
{
	m_attrCount = (size_t)pNode->getInt("attrs");
	m_valueCount = (size_t)pNode->getInt("vals");
}

// virtual
WDomNode* WUniformRelation::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "attrs", m_attrCount);
	pNode->add(pDoc, "vals", m_valueCount);
	return pNode;
}

// virtual
void WUniformRelation::deleteAttributes(size_t index, size_t count)
{
	if(index + count > m_attrCount)
		throw Ex("Index out of range");
	m_attrCount -= count;
}

// virtual
bool WUniformRelation::isCompatible(const WRelation& that) const
{
	if(that.type() == WRelation::UNIFORM)
	{
		if(m_attrCount == ((WUniformRelation*)&that)->m_attrCount && m_valueCount == ((WUniformRelation*)&that)->m_valueCount)
			return true;
		else
			return false;
	}
	else
		return WRelation::isCompatible(that);
}




WMixedRelation::WMixedRelation()
{
}

WMixedRelation::WMixedRelation(vector<size_t>& attrValues)
{
	m_valueCounts.reserve(attrValues.size());
	for(vector<size_t>::iterator it = attrValues.begin(); it != attrValues.end(); it++)
		addAttr(*it);
}

WMixedRelation::WMixedRelation(const WDomNode* pNode)
{
	m_valueCounts.clear();
	WDomNode* pValueCounts = pNode->get("vals");
	WDomListIterator it(pValueCounts);
	m_valueCounts.reserve(it.remaining());
	for(; it.current(); it.advance())
		m_valueCounts.push_back((size_t)it.currentInt());
}

WMixedRelation::WMixedRelation(const WRelation* pCopyMe)
{
	copy(pCopyMe);
}

WMixedRelation::WMixedRelation(const WRelation* pCopyMe, size_t firstAttr, size_t attrCount)
{
	addAttrs(*pCopyMe, firstAttr, attrCount);
}

// virtual
WMixedRelation::~WMixedRelation()
{
}

// virtual
WDomNode* WMixedRelation::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	WDomNode* pValueCounts = pNode->add(pDoc, "vals", pDoc->newList());
	for(size_t i = 0; i < m_valueCounts.size(); i++)
		pValueCounts->add(pDoc, m_valueCounts[i]);
	return pNode;
}

// virtual
WRelation* WMixedRelation::clone() const
{
	WMixedRelation* pNewRelation = new WMixedRelation();
	pNewRelation->addAttrs(*this, 0, size());
	return pNewRelation;
}

// virtual
WRelation* WMixedRelation::cloneMinimal() const
{
	if(m_valueCounts.size() == 0)
		return new WUniformRelation(0, 0);
	bool allSame = true;
	size_t vals = m_valueCounts[0];
	for(vector<size_t>::const_iterator it = m_valueCounts.begin(); it != m_valueCounts.end(); it++)
	{
		if(*it != vals)
		{
			allSame = false;
			break;
		}
	}
	if(allSame)
		return new WUniformRelation(m_valueCounts.size(), vals);
	WMixedRelation* pNewRelation = new WMixedRelation();
	pNewRelation->addAttrs(*this, 0, size());
	return pNewRelation;
}

// virtual
WRelation* WMixedRelation::cloneSub(size_t start, size_t count) const
{
	WMixedRelation* pNewRelation = new WMixedRelation();
	pNewRelation->addAttrs(*this, start, count);
	return pNewRelation;
}

void WMixedRelation::addAttrs(const WRelation& copyMe, size_t firstAttr, size_t attrCount)
{
	if(firstAttr + attrCount > copyMe.size())
	{
		if(attrCount == INVALID_INDEX)
			attrCount = copyMe.size() - firstAttr;
		else
			throw Ex("out of range");
	}
	for(size_t i = 0; i < attrCount; i++)
		copyAttr(&copyMe, firstAttr + i);
}

void WMixedRelation::addAttrs(size_t attrCount, size_t vals)
{
	for(size_t i = 0; i < attrCount; i++)
		addAttr(vals);
}

void WMixedRelation::copy(const WRelation* pCopyMe)
{
	flush();
	if(pCopyMe)
		addAttrs(*pCopyMe);
}

// virtual
void WMixedRelation::flush()
{
	m_valueCounts.clear();
}

void WMixedRelation::addAttr(size_t nValues)
{
	if(type() == ARFF)
	{
		ostringstream oss;
		oss << "attr_";
		oss << size();
		string s = oss.str();
		((WArffRelation*)this)->addAttribute(s.c_str(), nValues, nullptr);
	}
	else
		m_valueCounts.push_back(nValues);
}

// virtual
void WMixedRelation::copyAttr(const WRelation* pThat, size_t nAttr)
{
	if(nAttr >= pThat->size())
		throw Ex("attribute index out of range");
	addAttr(pThat->valueCount(nAttr));
}

// virtual
bool WMixedRelation::areContinuous(size_t first, size_t count) const
{
	for(size_t i = 0; i < count && i + first < size(); i++)
	{
		if(valueCount(i + first) != 0)
			return false;
	}
	return true;
}

// virtual
bool WMixedRelation::areNominal(size_t first, size_t count) const
{
	for(size_t i = 0; i < count && i + first < size(); i++)
	{
		if(valueCount(i + first) == 0)
			return false;
	}
	return true;
}

// virtual
void WMixedRelation::swapAttributes(size_t nAttr1, size_t nAttr2)
{
	std::swap(m_valueCounts[nAttr1], m_valueCounts[nAttr2]);
}

// virtual
void WMixedRelation::deleteAttributes(size_t nAttr, size_t count)
{
	m_valueCounts.erase(m_valueCounts.begin() + nAttr, m_valueCounts.begin() + (nAttr + count));
}

// virtual
void WMixedRelation::setAttrValueCount(size_t nAttr, size_t nValues)
{
	m_valueCounts[nAttr] = nValues;
}

// ------------------------------------------------------------------

WDomNode* WArffAttribute::serialize(WDom* pDoc, size_t valCount) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "name", m_name.c_str());
	if(valCount > 0 && m_values.size() >= valCount)
	{
		WDomNode* pVals = pNode->add(pDoc, "vals", pDoc->newList());
		for(size_t i = 0; i < valCount; i++)
			pVals->add(pDoc, m_values[i].c_str());
	}
	else
		pNode->add(pDoc, "vc", valCount);
	return pNode;
}

class WArffTokenizer : public WTokenizer
{
public:
	WCharSet m_whitespace, m_spaces, m_space, m_valEnd, m_valEnder, m_valHardEnder, m_argEnd, m_newline, m_commaNewlineTab;

	WArffTokenizer(const char* szFilename) : WTokenizer(szFilename),
	                                         m_whitespace("\t\n\r "), m_spaces(" \t"), m_space(" "), m_valEnd(",}\n"), m_valEnder(" ,\t}\n"), m_valHardEnder(",}\t\n"), m_argEnd(" \t\n{\r"), m_newline("\n"), m_commaNewlineTab(",\n\t") {}
	WArffTokenizer(const char* pFile, size_t len) : WTokenizer(pFile, len),
	                                                m_whitespace("\t\n\r "), m_spaces(" \t"), m_space(" "), m_valEnd(",}\n"), m_valEnder(" ,\t}\n"), m_valHardEnder(",}\t\n"), m_argEnd(" \t\n{\r"), m_newline("\n"), m_commaNewlineTab(",\n\t") {}
	~WArffTokenizer() override {}
};

WArffRelation::WArffRelation()
: m_name("Untitled")
{
}

WArffRelation::WArffRelation(const WDomNode* pNode)
{
	m_name = pNode->getString("name");
	WDomListIterator it(pNode->get("attrs"));
	while(it.current())
	{
		WDomNode* pVals = it.current()->getIfExists("vals");
		const char* valName = it.current()->getString("name");
		if(pVals)
		{
			vector<const char*> valNames;
			WDomListIterator it2(pVals);
			while(it2.current())
			{
				valNames.push_back(it2.currentString());
				it2.advance();
			}
			addAttribute(valName, valNames.size(), &valNames);
		}
		else
			addAttribute(valName, (size_t)it.current()->getInt("vc"), nullptr);
		it.advance();
	}
}

WArffRelation::~WArffRelation()
{
}

// virtual
WDomNode* WArffRelation::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "name", m_name.c_str());
	WDomNode* pAttrs = pNode->add(pDoc, "attrs", pDoc->newList());
	for(size_t i = 0; i < m_attrs.size(); i++)
		pAttrs->add(pDoc, m_attrs[i].serialize(pDoc, m_valueCounts[i]));
	return pNode;
}

// virtual
void WArffRelation::flush()
{
	WAssert(m_attrs.size() == m_valueCounts.size());
	m_attrs.clear();
	WMixedRelation::flush();
}

// virtual
WRelation* WArffRelation::clone() const
{
	WArffRelation* pNewRelation = new WArffRelation();
	pNewRelation->addAttrs(*this);
	pNewRelation->setName(name());
	return pNewRelation;
}

// virtual
WRelation* WArffRelation::cloneMinimal() const
{
	return WMixedRelation::cloneMinimal();
}

// virtual
WRelation* WArffRelation::cloneSub(size_t start, size_t count) const
{
	WArffRelation* pNewRelation = new WArffRelation();
	pNewRelation->addAttrs(*this, start, count);
	pNewRelation->setName(name());
	return pNewRelation;
}

void WArffRelation::addAttribute(const char* szName, size_t nValues, vector<const char*>* pValues)
{
	WAssert(m_attrs.size() == m_valueCounts.size());
	size_t index = m_valueCounts.size();
	m_valueCounts.push_back(nValues);
	m_attrs.resize(index + 1);
	if(szName)
	{
		m_attrs[index].m_name = szName;
	}
	else
	{
		m_attrs[index].m_name = "attr_";
		std::ostringstream oss;
		oss << index;
		m_attrs[index].m_name += oss.str();
	}
	if(pValues)
	{
		if(nValues != pValues->size())
			throw Ex("mismatching value counts");
		for(size_t i = 0; i < nValues; i++)
		{
			WAssert((*pValues)[i][0] > ' ');
			m_attrs[index].m_values.push_back((*pValues)[i]);
		}
	}
}

// virtual
void WArffRelation::copyAttr(const WRelation* pThat, size_t nAttr)
{
	if(nAttr >= pThat->size())
		throw Ex("attribute index out of range");
	if(pThat->type() == ARFF)
	{
		size_t index = m_valueCounts.size();
		WArffRelation* pOther = (WArffRelation*)pThat;
		addAttribute(pOther->m_attrs[nAttr].m_name.c_str(), pOther->m_valueCounts[nAttr], nullptr);
		for(size_t i = 0; i < pOther->m_attrs[nAttr].m_values.size(); i++)
			m_attrs[index].m_values.push_back(pOther->m_attrs[nAttr].m_values[i]);
	}
	else
		addAttribute(nullptr, pThat->valueCount(nAttr), nullptr);
}

void WArffRelation::setName(const char* szName)
{
	m_name = szName;
}

void WArffRelation::parseAttribute(WArffTokenizer& tok)
{
	tok.skipWhile(tok.m_spaces);
	string dataname = tok.readUntil_escaped_quoted(tok.m_argEnd);
	//std::cerr << "Attr:" << dataname << "\n"; //DEBUG
	tok.skipWhile(tok.m_spaces);
	char c = tok.peek();
	if(c == '{')
	{
		tok.skip(1);
		WAssert(m_attrs.size() == m_valueCounts.size());
		size_t index = m_valueCounts.size();
		m_attrs.resize(index + 1);
		while(true)
		{
			tok.readUntil_escaped_quoted(tok.m_valEnd);
			char* szVal = tok.trim(tok.m_whitespace);
			if(*szVal == '\0')
				throw Ex("Empty value specified on line ", to_str(tok.line()));
			m_attrs[index].m_values.push_back(szVal);
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
		m_valueCounts.push_back(m_attrs[index].m_values.size());
		if(dataname.length() > 0)
			m_attrs[index].m_name = dataname;
		else
		{
			m_attrs[index].m_name = "attr_";
			std::ostringstream oss;
			oss << index;
			m_attrs[index].m_name += oss.str();
		}
	}
	else
	{
		const char* szType = tok.readUntil(tok.m_whitespace);
		if(_stricmp(szType, "CONTINUOUS") == 0 || _stricmp(szType, "REAL") == 0 || _stricmp(szType, "NUMERIC") == 0 || _stricmp(szType, "INTEGER") == 0)
		{
			addAttribute(dataname.c_str(), 0, nullptr);
		}
		else if(_stricmp(szType, "STRING") == 0)
			addAttribute(dataname.c_str(), (size_t)-1, nullptr);
		else if(_stricmp(szType, "DATE") == 0)
		{
			dataname += ':';
			tok.skipWhile(tok.m_spaces);
			while(true)
			{
				char c2 = tok.peek();
				if(c2 == '\n' || c2 == '\r')
					break;
				dataname += c2;
				tok.skip(1);
			}
			addAttribute(dataname.c_str(), (size_t)-2, nullptr);
		}
		else
			throw Ex("Unsupported attribute type: (", szType, "), at line ", to_str(tok.line()));
	}
	tok.skipUntil(tok.m_newline);
	tok.skip(1);
}

// virtual
void WArffRelation::printAttrName(std::ostream& stream, size_t column) const
{
	std::string s = attrName(column);
	if(s.find(' ') != std::string::npos)
		stream << "'" << s << "'";
	else
		stream << s;
}

// virtual
void WArffRelation::printAttrValue(ostream& stream, size_t column, double value, const char* missing) const
{
	size_t valCount = valueCount(column);
	if(valCount == 0) // continuous
	{
		if(value == UNKNOWN_REAL_VALUE)
			stream << missing;
		else
			stream << value;
	}
	else if(valCount < (size_t)-10) // nominal
	{
		int val = (int)value;
		if(val < 0)
			stream << missing;
		else if(val >= (int)valCount)
			throw Ex("value out of range");
		else if(m_attrs[column].m_values.size() > 0)
		{
			WAssert(m_attrs[column].m_values[val][0] > ' ');
			stream << m_attrs[column].m_values[val];
		}
		else if(val < 26)
		{
			char tmp[2];
			tmp[0] = 'a' + val;
			tmp[1] = '\0';
			stream << tmp;
		}
		else
			stream << "_" << val;
	}
	else if(valCount == (size_t)-1) // string
	{
		stream << "<string>";
	}
	else if(valCount == (size_t)-2) // date
		stream << value;
	else
		throw Ex("Unexpected attribute type");
}

// virtual
bool WArffRelation::isCompatible(const WRelation& that) const
{
	if(that.type() == WRelation::ARFF)
	{
		if(this == &that)
			return true;
		if(!WRelation::isCompatible(that))
			return false;
		for(size_t i = 0; i < size(); i++)
		{
			if(((WArffRelation*)this)->attrName(i)[0] != '\0' && ((WArffRelation*)&that)->attrName(i)[0] != '\0' && strcmp(((WArffRelation*)this)->attrName(i), ((WArffRelation*)&that)->attrName(i)) != 0)
				return false;
			size_t vals = valueCount(i);
			if(vals != 0)
			{
				const WArffAttribute& attrThis = m_attrs[i];
				const WArffAttribute& attrThat = ((const WArffRelation*)&that)->m_attrs[i];
				size_t named = std::min(attrThis.m_values.size(), attrThat.m_values.size());
				for(size_t j = 0; j < named; j++)
				{
					if(attrThis.m_values[j].length() != 0 && attrThat.m_values[j].length() != 0 && strcmp(attrThis.m_values[j].c_str(), attrThat.m_values[j].c_str()) != 0)
						return false;
				}
			}
		}
		return true;
	}
	else
		return WRelation::isCompatible(that);
}

string stripQuotes(string& s)
{
	if(s.length() < 2)
		return s;
	if(s[0] == '"' && s[s.length() - 1] == '"')
		return s.substr(1, s.length() - 2);
	if(s[0] == '\'' && s[s.length() - 1] == '\'')
		return s.substr(1, s.length() - 2);
	return s;
}

int WArffRelation::findEnumeratedValue(size_t nAttr, const char* szValue) const
{
	size_t nValueCount = valueCount(nAttr);
	size_t actualValCount = m_attrs[nAttr].m_values.size();
	if(nValueCount > actualValCount)
		throw Ex("some values have no names");
	size_t i;
	bool quotedCand = false;
	for(i = 0; i < nValueCount; i++)
	{
		const char* szCand = m_attrs[nAttr].m_values[i].c_str();
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
			string sCand = m_attrs[nAttr].m_values[i].c_str();
			if(sCand.length() > 0 && (sCand[0] == '"' || sCand[0] == '\''))
				sCand = stripQuotes(sCand);
			if(sCand.compare(sValue) == 0)
				return (int)i;
		}
	}
	return UNKNOWN_DISCRETE_VALUE;
}

const char* WArffRelation::attrName(size_t nAttr) const
{
	return m_attrs[nAttr].m_name.c_str();
}

void WArffRelation::setAttrName(size_t attr, const char* szNewName)
{
	m_attrs[attr].m_name = szNewName;
}

int WArffRelation::addAttrValue(size_t nAttr, const char* szValue)
{
	int val = (int)m_valueCounts[nAttr]++;
	WAssert(m_attrs[nAttr].m_values.size() == (size_t)val);
	m_attrs[nAttr].m_values.push_back(szValue);
	return val;
}

// virtual
void WArffRelation::setAttrValueCount(size_t nAttr, size_t nValues)
{
	m_attrs[nAttr].m_values.clear();
	WMixedRelation::setAttrValueCount(nAttr, nValues);
}

// virtual
void WArffRelation::swapAttributes(size_t nAttr1, size_t nAttr2)
{
	WMixedRelation::swapAttributes(nAttr1, nAttr2);
	std::swap(m_attrs[nAttr1], m_attrs[nAttr2]);
}

// virtual
void WArffRelation::deleteAttributes(size_t nAttr, size_t count)
{
	m_attrs.erase(m_attrs.begin() + nAttr, m_attrs.begin() + (nAttr + count));
	WMixedRelation::deleteAttributes(nAttr, count);
}

double WArffRelation::parseValue(size_t attr, const char* val)
{
	size_t values = valueCount(attr);
	if(values == 0)
	{
		if(strcmp(val, "?") == 0)
			return UNKNOWN_REAL_VALUE;
		else
		{
			double d;
			if(!parseNumber(val, &d)) // SAKBox: was checked by its first character only
				throw Ex("Invalid real value, ", val, ". Expected a number.");
			return d;
		}
	}
	else
	{
		if(strcmp(val, "?") == 0)
			return UNKNOWN_DISCRETE_VALUE;
		else
		{
			size_t v = INVALID_INDEX;
			for(size_t j = 0; j < values; j++)
			{
				if(_stricmp(val, m_attrs[attr].m_values[j].c_str()) == 0)
				{
					v = j;
					break;
				}
			}
			if(v == INVALID_INDEX)
			{
				if(*val >= '0' && *val <= '9')
				{
					// SAKBox: a value's position, which wasn't checked to be in range
					if(!parseNumber(val, &v) || v >= values)
						throw Ex("Invalid categorical value, ", val, ". Expected a value's name, or a number below ", to_str(values));
				}
				else
				{
					string sChoices;
					for(size_t j = 0; j < values; j++)
					{
						if(j != 0)
							sChoices += ',';
						sChoices += m_attrs[attr].m_values[j].c_str();
					}
					throw Ex("Invalid categorical value, ", val, ". Expected one of {", sChoices, "}");
				}
			}
			return double(v);
		}
	}
}

void WArffRelation::dropValue(size_t attr, int val)
{
	size_t valCount = valueCount(attr);
	if((size_t)val >= valCount)
		throw Ex("out of range");
	WArffAttribute& at = m_attrs[attr];
	if(at.m_values.size() == valCount)
	{
		std::swap(at.m_values[val], at.m_values[valCount - 1]);
		at.m_values.erase(at.m_values.end() - 1);
	}
	WMixedRelation::setAttrValueCount(attr, valCount - 1);
}

// ------------------------------------------------------------------

WMatrix::WMatrix()
: m_pRelation(&g_emptyRelation)
{
}

WMatrix::WMatrix(WRelation* pRelation)
: m_pRelation(pRelation)
{
}

WMatrix::WMatrix(size_t rowCount, size_t colCount)
{
	m_pRelation = new WUniformRelation(colCount, 0);
	newRows(rowCount);
}

WMatrix::WMatrix(vector<size_t>& attrValues)
{
	m_pRelation = new WMixedRelation(attrValues);
}

WMatrix::WMatrix(const WMatrix& orig, size_t rowStart, size_t colStart, size_t rowCount, size_t colCount)
: m_pRelation(nullptr)
{
	copy(orig, rowStart, colStart, rowCount, colCount);
}

WMatrix& WMatrix::operator=(const WMatrix& orig)
{
	// SAKBox: copy() empties this matrix first, so m = m lost its data (copy
	// only asserts this != &that, in debug builds)
	if(this == &orig)
		return *this;
	copy(orig);
	return *this;
}

WMatrix::WMatrix(const WDomNode* pNode)
{
	m_pRelation = WRelation::deserialize(pNode->get("rel"));
	WDomNode* pRows = pNode->get("vals");
	WDomListIterator it(pRows);
	reserve(it.remaining());
	size_t dims = (size_t)m_pRelation->size();
	for(size_t i = 0; it.current(); it.advance())
	{
		WDomNode* pRow = it.current();
		WDomListIterator it2(pRow);
		if(it2.remaining() != dims)
			throw Ex("Row ", to_str(i), " has an unexpected number of values");
		WVec& pat = newRow();
		for(size_t j = 0; it2.current(); it2.advance())
			pat[j++] = it2.currentDouble();
		i++;
	}
}

WMatrix::~WMatrix()
{
	flush();
	setRelation(nullptr);
}

bool WMatrix::operator==(const WMatrix& other) const
{
	//Check if relation is compatible
	if(!relation().isCompatible(other.relation()))
	{
		return false;
	}
	//Check if same size
	if(!(rows() == other.rows() && cols() == other.cols()))
	{
		return false;
	}
	//Check if have same entries
	const std::size_t c = cols();
	for(size_t i = 0; i < rows(); i++)
	{
		const WVec& a = row(i);
		const WVec& b = other.row(i);
		for(size_t j = 0; j < c; j++)
		{
			if(a[j] != b[j])
				return false;
		}
	}
	return true;
}

void WMatrix::setRelation(WRelation* pRelation)
{
	if(pRelation && rows() > 0 && m_pRelation && pRelation->size() != m_pRelation->size())
		throw Ex("Existing data incompatible with new relation");
	if(m_pRelation != pRelation)
	{
		if(m_pRelation != &g_emptyRelation)
			delete(m_pRelation);
		m_pRelation = pRelation;
	}
}

void WMatrix::resize(size_t rowCount, size_t colCount)
{
	flush();
	setRelation(new WUniformRelation(colCount, 0));
	newRows(rowCount);
}

void WMatrix::flush()
{
	for(size_t i = 0; i < rows(); i++)
		delete(m_rows[i]);
	m_rows.clear();
}

// SAKBox: while an ARFF file loads, the texts seen so far in each string
// column, in order, and each one's index (keyed without surrounding quotes)
struct WArffStringValues
{
	std::vector<std::string> m_texts; // as first written, quotes and all
	std::map<std::string, size_t> m_index;
};

static std::string WArffStringValues_key(const char* szVal)
{
	size_t len = strlen(szVal);
	if(len >= 2 && (szVal[0] == '\'' || szVal[0] == '"') && szVal[len - 1] == szVal[0])
		return std::string(szVal + 1, len - 2);
	return szVal;
}

double WMatrix_parseValue(WArffRelation* pRelation, size_t col, const char* szVal, WTokenizer& tok, std::map<size_t, WArffStringValues>* pStrings = nullptr)
{
	size_t vals = pRelation->valueCount(col);
	if(vals == 0) // Continuous
	{
		// Continuous attribute
		if(*szVal == '\0' || (*szVal == '?' && szVal[1] == '\0'))
			return UNKNOWN_REAL_VALUE;
		else
		{
			double d;
			if(!parseNumber(szVal, &d)) // SAKBox: was IsRealValue, which checked only the start ("1.5abc" read as 1.5)
				throw Ex("Expected a numeric value at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
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
			int nVal = pRelation->findEnumeratedValue(col, szVal);
			if(nVal == UNKNOWN_DISCRETE_VALUE)
				throw Ex("Unrecognized enumeration value '", szVal, "' for attribute ", to_str(col), " at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			return (double)nVal;
		}
	}
	else if(vals == (size_t)-1) // String
	{
		// SAKBox: was 0.0, losing the text. Each distinct text is now a value of
		// a category (the column becomes one once the file is loaded).
		if(!pStrings)
			throw Ex("String values aren't supported here");
		if(*szVal == '\0' || (*szVal == '?' && szVal[1] == '\0'))
			return UNKNOWN_DISCRETE_VALUE;
		WArffStringValues& strings = (*pStrings)[col];
		std::string key = WArffStringValues_key(szVal);
		std::map<std::string, size_t>::iterator it = strings.m_index.find(key);
		if(it != strings.m_index.end())
			return (double)it->second;
		size_t index = strings.m_texts.size();
		strings.m_texts.push_back(szVal);
		strings.m_index[key] = index;
		return (double)index;
	}
	else if(vals == (size_t)-2) // Date
	{
		const char* szFormat = pRelation->attrName(col);
		while(*szFormat != ':' && *szFormat != '\0')
			szFormat++;
		if(*szFormat == '\0')
			throw Ex("Invalid date format string");
		szFormat++;
		double t;
		if(!WTime::fromString(&t, szVal, szFormat))
			throw Ex("The string, ", szVal, " does not fit the specified date format, ", szFormat);
		return t;
	}
	else
		throw Ex("Unexpected attribute type, ", to_str(vals));
}

void WMatrix::parseArff(WArffTokenizer& tok, size_t maxRows)
{
	parseArff(tok, WArffStrings::asCategories, maxRows);
}

void WMatrix::parseArff(WArffTokenizer& tok, WArffStrings stringColumns, size_t maxRows)
{
	// Parse the meta data. SAKBox: the relation is owned here until the
	// matrix takes it, so it isn't leaked when the file is refused.
	std::unique_ptr<WArffRelation> relation(new WArffRelation());
	WArffRelation* pRelation = relation.get();
	while(true)
	{
		tok.skipWhile(tok.m_whitespace);
		char c = tok.peek();
		if(c == '\0')
			throw Ex("Invalid ARFF file--contains no data");
		else if(c == '%')
		{
			tok.skip(1);
			tok.skipUntil(tok.m_newline);
		}
		else if(c == '@')
		{
			tok.skip(1);
			const char* szTok = tok.readUntil(tok.m_whitespace);
			if(_stricmp(szTok, "ATTRIBUTE") == 0)
				pRelation->parseAttribute(tok);
			else if(_stricmp(szTok, "RELATION") == 0)
			{
				tok.skipWhile(tok.m_spaces);
				pRelation->setName(tok.readUntil_escaped_quoted(tok.m_whitespace));
				tok.skip(1);
			}
			else if(_stricmp(szTok, "DATA") == 0)
			{
				tok.skipUntil(tok.m_newline);
				tok.skip(1);
				break;
			}
		}
		else
			throw Ex("Expected a '%' or a '@' at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
	}

	// SAKBox: refused before the matrix is changed
	if(stringColumns == WArffStrings::refuse)
	{
		for(size_t i = 0; i < pRelation->size(); i++)
		{
			if(pRelation->valueCount(i) == INVALID_INDEX)
				throw Ex("Column '", pRelation->attrName(i), "' is an ARFF string column, which isn't loaded (WArffStrings::refuse): Waffles can only keep text as a category");
		}
	}

	flush();
	setRelation(relation.release());
	size_t colCount = pRelation->size();
	std::map<size_t, WArffStringValues> strings; // SAKBox: see WMatrix_parseValue
	while(true)
	{
		if(rows() >= maxRows)
			break;
		tok.skipWhile(tok.m_whitespace);
		char c = tok.peek();
		if(c == '\0')
			break;
		else if(c == '%')
		{
			tok.skip(1);
			tok.skipUntil(tok.m_newline);
		}
		else if(c == '{')
		{
			// Parse ARFF sparse data format
			tok.skip(1);
			WVec& r = newRow();
			r.fill(0.0);
			while(true)
			{
				tok.skipWhile(tok.m_space);
				char c2 = tok.peek();
				if(c2 >= '0' && c2 <= '9')
				{
					const char* szTok = tok.readUntil(tok.m_valEnder);
#ifdef WINDOWS
					size_t column = (size_t)_strtoui64(szTok, (char**)NULL, 10);
#else
					size_t column = strtoull(szTok, (char**)nullptr, 10);
#endif
					if(column >= colCount)
						throw Ex("Column index out of range at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
					tok.skipWhile(tok.m_spaces);
					const char* szVal = tok.readUntil_escaped_quoted(tok.m_valEnder);
					r[column] = WMatrix_parseValue(pRelation, column, szVal, tok, &strings);
					tok.skipUntil(tok.m_valHardEnder);
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
			WVec& r = newRow();
			size_t column = 0;
			while(true)
			{
				if(column >= colCount)
					throw Ex("Too many values on line ", to_str(tok.line()), ", col ", to_str(tok.col()));
				tok.readUntil_escaped_quoted(tok.m_commaNewlineTab);
				const char* szVal = tok.trim(tok.m_whitespace);
				r[column] = WMatrix_parseValue(pRelation, column, szVal, tok, &strings);
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
					tok.skipUntil(tok.m_newline);
					break;
				}
			}
			if(column < colCount)
				throw Ex("Not enough values on line ", to_str(tok.line()), ", col ", to_str(tok.col()));
		}
	}
	// SAKBox: a string column becomes a category of the texts it held (it used
	// to become a number column of zeros). Most values being different
	// suggests free text, which a learner would take as arbitrary numbers.
	for(size_t i = 0; i < colCount; i++)
	{
		if(pRelation->valueCount(i) != INVALID_INDEX)
			continue;
		pRelation->setAttrValueCount(i, 0);
		const std::vector<std::string>& texts = strings[i].m_texts;
		for(const std::string& text : texts)
			pRelation->addAttrValue(i, text.c_str());
		if(texts.size() >= 20 && texts.size() * 2 > rows())
			std::cerr << "warning: string column '" << pRelation->attrName(i) << "' has " << texts.size()
			          << " different values in " << rows() << " rows; loaded as a category, which a learner may treat as meaningless numbers\n";
	}
}

void WMatrix::loadArff(const char* szFilename, size_t maxRows)
{
	loadArff(szFilename, WArffStrings::asCategories, maxRows);
}

void WMatrix::loadArff(const char* szFilename, WArffStrings stringColumns, size_t maxRows)
{
	WArffTokenizer tok(szFilename);
	parseArff(tok, stringColumns, maxRows);
}

void WMatrix::loadRaw(const char* szFilename)
{
	size_t r, c;
	std::ifstream fin(szFilename, std::ios::in | std::ios::binary);
	if(fin.fail())
		throw Ex("Error while trying to open the file, ", szFilename, ". ", strerror(errno));
	fin.read((char*)&r, sizeof(size_t));
	fin.read((char*)&c, sizeof(size_t));
	resize(r, c);
	for(size_t i = 0; i < r; i++)
		fin.read((char*)m_rows[i]->data(), sizeof(double) * c);
	fin.close();
}

void WMatrix::load(const char* szFilename)
{
	const char* extPos = strrchr(szFilename, '.');
	if(extPos)
	{
		string ext(extPos + 1);
		for(size_t i = 0; i < ext.size(); ++i)
			ext[i] = (char)tolower((unsigned char)ext[i]); // SAKBox: a negative char is undefined behaviour in tolower
		if(ext == "arff")
			loadArff(szFilename);
		else if(ext == "raw")
			loadRaw(szFilename);
		else
			throw Ex("File type could not be determined.");
	}
	else
		throw Ex("File type could not be determined.");
}

void WMatrix::saveArff(const char* szFilename)
{
	m_pRelation->save(this, szFilename);
}

void WMatrix::saveRaw(const char* szFilename)
{
	std::ofstream fout(szFilename, std::ios::out | std::ios::binary);
	size_t r = rows();
	size_t c = cols();
	fout.write((char*)&r, sizeof(size_t));
	fout.write((char*)&c, sizeof(size_t));
	for(size_t i = 0; i < r; i++)
		fout.write((char*)m_rows[i]->data(), sizeof(double) * c);
	fout.close();
}

// static
void WMatrix::parseArff(const char* szFile, size_t nLen, size_t maxRows)
{
	parseArff(szFile, nLen, WArffStrings::asCategories, maxRows);
}

void WMatrix::parseArff(const char* szFile, size_t nLen, WArffStrings stringColumns, size_t maxRows)
{
	WArffTokenizer tok(szFile, nLen);
	parseArff(tok, stringColumns, maxRows);
}

size_t WMatrix::countUniqueValues(size_t column, size_t maxCount) const
{
	size_t unique = 0;
	std::set<double> seen;
	for(size_t i = 0; i < m_rows.size(); i++)
	{
		if(seen.find((*this)[i][column]) == seen.end())
		{
			seen.insert((*this)[i][column]);
			if(++unique >= maxCount)
				return maxCount;
		}
	}
	return unique;
}

WDomNode* WMatrix::serialize(WDom* pDoc) const
{
	WDomNode* pData = pDoc->newObj();
	size_t attrCount = m_pRelation->size();
	pData->add(pDoc, "rel", m_pRelation->serialize(pDoc));
	WDomNode* pPats = pData->add(pDoc, "vals", pDoc->newList());
	WDomNode* pRow;
	for(size_t i = 0; i < rows(); i++)
	{
		const WVec& pat = row(i);
		pRow = pPats->add(pDoc, pDoc->newList());
		for(size_t j = 0; j < attrCount; j++)
			pRow->add(pDoc, pat[j]);
	}
	return pData;
}


void WMatrix::col(size_t index, double* pOutVector)
{
	for(size_t i = 0; i < rows(); i++)
		*(pOutVector++) = row(i)[index];
}

void WMatrix::setCol(size_t index, const double* pVector)
{
	for(size_t i = 0; i < rows(); i++)
		row(i)[index] = *(pVector++);
}

void WMatrix::add(const WMatrix* pThat, bool transposeThat, double scalar)
{
	if(transposeThat)
	{
		size_t c = (size_t)cols();
		if(rows() != (size_t)pThat->cols() || c != pThat->rows())
			throw Ex("expected matrices of same size");
		for(size_t i = 0; i < rows(); i++)
		{
			WVec& r = row(i);
			for(size_t j = 0; j < c; j++)
				r[j] += scalar * pThat->row(j)[i];
		}
	}
	else
	{
		size_t c = cols();
		if(rows() != pThat->rows() || c != pThat->cols())
			throw Ex("expected matrices of same size");
		for(size_t i = 0; i < rows(); i++)
			row(i).addScaled(scalar, pThat->row(i));
	}
}

void WMatrix::dropValue(size_t attr, int val)
{
	if(attr >= cols())
		throw Ex("out of range");
	size_t lastVal = relation().valueCount(attr);
	if((size_t)val >= lastVal)
		throw Ex("out of range");
	lastVal--;

	// Adjust the relation
	if(relation().type() == WRelation::ARFF)
		((WArffRelation*)m_pRelation)->dropValue(attr, val);
	else if(relation().type() == WRelation::MIXED)
		((WMixedRelation*)m_pRelation)->setAttrValueCount(attr, lastVal);
	else
		throw Ex("Sorry, not supported for uniform relations");

	// Adjust the data
	for(size_t i = 0; i < m_rows.size(); i++)
	{
		WVec& r = row(i);
		if(r[attr] == lastVal)
			r[attr] = val;
		else if(r[attr] == val)
			r[attr] = UNKNOWN_DISCRETE_VALUE;
	}
}

void WMatrix::subtract(const WMatrix* pThat, bool transposeThat)
{
	if(transposeThat)
	{
		size_t c = (size_t)cols();
		if(rows() != (size_t)pThat->cols() || c != pThat->rows())
			throw Ex("expected matrices of same size");
		for(size_t i = 0; i < rows(); i++)
		{
			WVec& r = row(i);
			for(size_t j = 0; j < c; j++)
				r[j] -= pThat->row(j)[i];
		}
	}
	else
	{
		size_t c = cols();
		if(rows() != pThat->rows() || c != pThat->cols())
			throw Ex("expected matrices of same size");
		for(size_t i = 0; i < rows(); i++)
			row(i) -= pThat->row(i);
	}
}

void WMatrix::multiply(double scalar)
{
	for(size_t i = 0; i < rows(); i++)
		row(i) *= scalar;
}

void WMatrix::multiply(const WVec& vectorIn, WVec& vectorOut, bool transposeFirst) const
{
	size_t rowCount = rows();
	if(transposeFirst)
	{
		vectorOut.fill(0.0);
		for(size_t i = 0; i < rowCount; i++)
			vectorOut.addScaled(vectorIn[i], row(i));
	}
	else
	{
		for(size_t i = 0; i < rowCount; i++)
			vectorOut[i] = row(i).dotProduct(vectorIn);
	}
}

// static
WMatrix* WMatrix::multiply(const WMatrix& a, const WMatrix& b, bool transposeA, bool transposeB)
{
	if(transposeA)
	{
		if(transposeB)
		{
			size_t dims = a.rows();
			if((size_t)b.cols() != dims)
				throw Ex("dimension mismatch");
			size_t w = b.rows();
			size_t h = a.cols();
			WMatrix* pOut = new WMatrix(h, w);
			for(size_t y = 0; y < h; y++)
			{
				WVec& r = pOut->row(y);
				for(size_t x = 0; x < w; x++)
				{
					const WVec& pB = b[x];
					double sum = 0;
					for(size_t i = 0; i < dims; i++)
						sum += a[i][y] * pB[i];
					r[x] = sum;
				}
			}
			return pOut;
		}
		else
		{
			size_t dims = a.rows();
			if(b.rows() != dims)
				throw Ex("dimension mismatch");
			size_t w = b.cols();
			size_t h = a.cols();
			WMatrix* pOut = new WMatrix(h, w);
			for(size_t y = 0; y < h; y++)
			{
				WVec& r = pOut->row(y);
				for(size_t x = 0; x < w; x++)
				{
					double sum = 0;
					for(size_t i = 0; i < dims; i++)
						sum += a[i][y] * b[i][x];
					r[x] = sum;
				}
			}
			return pOut;
		}
	}
	else
	{
		if(transposeB)
		{
			size_t dims = (size_t)a.cols();
			if((size_t)b.cols() != dims)
				throw Ex("dimension mismatch");
			size_t w = b.rows();
			size_t h = a.rows();
			WMatrix* pOut = new WMatrix(h, w);
			for(size_t y = 0; y < h; y++)
			{
				WVec& r = pOut->row(y);
				const WVec& pA = a[y];
				for(size_t x = 0; x < w; x++)
					r[x] = pA.dotProduct(b[x]);
			}
			return pOut;
		}
		else
		{
			size_t dims = (size_t)a.cols();
			if(b.rows() != dims)
				throw Ex("dimension mismatch");
			size_t w = b.cols();
			size_t h = a.rows();
			WMatrix* pOut = new WMatrix(h, w);
			for(size_t y = 0; y < h; y++)
			{
				WVec& r = pOut->row(y);
				const WVec& pA = a[y];
				for(size_t x = 0; x < w; x++)
				{
					double sum = 0;
					for(size_t i = 0; i < dims; i++)
						sum += pA[i] * b[i][x];
					r[x] = sum;
				}
			}
			return pOut;
		}
	}
}

WMatrix* WMatrix::transpose()
{
	size_t r = rows();
	size_t c = (size_t)cols();
	WMatrix* pTarget = new WMatrix(c, r);
	for(size_t i = 0; i < c; i++)
	{
		WVec& pRow = pTarget->row(i);
		for(size_t j = 0; j < r; j++)
			pRow[j] = row(j)[i];
	}
	return pTarget;
}

double WMatrix::trace()
{
	size_t min = std::min((size_t)cols(), rows());
	double sum = 0;
	for(size_t n = 0; n < min; n++)
		sum += row(n)[n];
	return sum;
}

size_t WMatrix::toReducedRowEchelonForm()
{
	size_t nLead = 0;
	size_t rowCount = rows();
	size_t colCount = cols();
	for(size_t nRow = 0; nRow < rowCount; nRow++)
	{
		// Find the next pivot (swapping rows as necessary)
		size_t i = nRow;
		while(std::abs(row(i)[nLead]) < 1e-9)
		{
			if(++i >= rowCount)
			{
				i = nRow;
				if(++nLead >= colCount)
					return nRow;
			}
		}
		if(i > nRow)
			swapRows(i, nRow);

		// Scale the pivot to 1
		WVec& pRow = row(nRow);
		double d = 1.0 / pRow[nLead];
		for(i = nLead; i < colCount; i++)
			pRow[i] *= d;

		// Elliminate all values above and below the pivot
		for(i = 0; i < rowCount; i++)
		{
			if(i != nRow)
			{
				double* pR2 = row(i).data();
				double t = pR2[nLead];
				for(size_t j = nLead; j < colCount; j++)
					pR2[j] -= t * pRow[j];
			}
		}

		nLead++;
	}
	return rowCount;
}

bool WMatrix::gaussianElimination(double* pVector)
{
	if(rows() != (size_t)cols())
		throw Ex("Expected a square matrix");
	double d;
	size_t rowCount = rows();
	size_t colCount = cols();
	for(size_t nRow = 0; nRow < rowCount; nRow++)
	{
		size_t i;
		for(i = nRow; i < rowCount && std::abs(row(i)[nRow]) < 1e-4; i++)
		{
		}
		if(i >= rowCount)
			continue;
		if(i > nRow)
		{
			swapRows(i, nRow);
			d = pVector[i];
			pVector[i] = pVector[nRow];
			pVector[nRow] = d;
		}

		// Scale the pivot to 1
		WVec& pRow = row(nRow);
		d = 1.0 / pRow[nRow];
		for(i = nRow; i < colCount; i++)
			pRow[i] *= d;
		pVector[nRow] *= d;

		// Elliminate all values above and below the pivot
		for(i = 0; i < rowCount; i++)
		{
			if(i != nRow)
			{
				d = -row(i)[nRow];
				double* pR2 = row(i).data();
				for(size_t j = nRow; j < colCount; j++)
					pR2[j] += d * pRow[j];
				pVector[i] += d * pVector[nRow];
			}
		}
	}

	// Arbitrarily assign null-space values to 1
	for(size_t nRow = 0; nRow < rowCount; nRow++)
	{
		if(row(nRow)[nRow] < 0.5)
		{
			if(std::abs(pVector[nRow]) >= 1e-4)
				return false;
			for(size_t i = 0; i < rowCount; i++)
			{
				if(i == nRow)
				{
					pVector[nRow] = 1;
					row(nRow)[nRow] = 1;
				}
				else
				{
					pVector[i] -= row(i)[nRow];
					row(i)[nRow] = 0;
				}
			}
		}
	}
	return true;
}

WMatrix* WMatrix::cholesky(bool tolerant)
{
	size_t rowCount = rows();
	size_t colCount = (size_t)cols();
	WMatrix* pOut = new WMatrix(m_pRelation->cloneMinimal());
	pOut->newRows(rowCount);
	double d;
	for(size_t j = 0; j < rowCount; j++)
	{
		size_t i;
		for(i = 0; i < j; i++)
		{
			d = 0;
			for(size_t k = 0; k < i; k++)
				d += (pOut->row(i)[k] * pOut->row(j)[k]);
			if(std::abs(pOut->row(i)[i]) < 1e-12)
				pOut->row(i)[i] = 1e-10;
			pOut->row(j)[i] = (1.0 / pOut->row(i)[i]) * (row(i)[j] - d);
		}
		d = 0;
		for(size_t k = 0; k < i; k++)
			d += (pOut->row(i)[k] * pOut->row(j)[k]);
		d = row(j)[i] - d;
		if(d < 0)
		{
			if(d > -1e-12)
				d = 0; // it's probably just rounding error
			else if(tolerant)
				d = -d;
			else
				throw Ex("not positive definite");
		}
		pOut->row(j)[i] = sqrt(d);
		for(i++; i < colCount; i++)
			pOut->row(j)[i] = 0;
	}
	return pOut;
}

void WMatrix::LUDecomposition()
{
	size_t colCount = cols();
	WVec& r = row(0);
	for(size_t i = 1; i < colCount; i++)
		r[i] /= r[0];
	for(size_t i = 1; i < colCount; i++)
	{
		for(size_t j = i; j < colCount; j++)
		{ // do a column of L
			double sum = 0.0;
			for(size_t k = 0; k < i; k++)
				sum += row(j)[k] * row(k)[i];
			row(j)[i] -= sum;
		}
		if(i == colCount - 1)
			continue;
		for(size_t j = i + 1; j < colCount; j++)
		{ // do a row of U
			double sum = 0.0;
			for(size_t k = 0; k < i; k++)
				sum += row(i)[k] * row(k)[j];
			row(i)[j] = (row(i)[j] - sum) / row(i)[i];
		}
	}
}

/*
void WMatrix::invert()
{
	if(rows() != (size_t)cols())
		throw Ex("only square matrices supported");
	if(rows() == 1)
	{
		row(0)[0] = 1.0 / row(0)[0];
		return;
	}

	// Do LU decomposition (I think this is the Doolittle algorithm)
	int colCount = cols();
	double* pRow = row(0);
	for(int i = 1; i < colCount; i++)
		pRow[i] /= pRow[0];
	for(int i = 1; i < colCount; i++)
	{
		for(int j = i; j < colCount; j++)
		{ // do a column of L
			double sum = 0.0;
			for(int k = 0; k < i; k++)
				sum += row(j)[k] * row(k)[i];
			row(j)[i] -= sum;
		}
		if(i == colCount - 1)
			continue;
		for(int j = i + 1; j < colCount; j++)
		{ // do a row of U
			double sum = 0.0;
			for(int k = 0; k < i; k++)
				sum += row(i)[k] * row(k)[j];
			row(i)[j] = (row(i)[j] - sum) / row(i)[i];
		}
	}

	// Invert L
	for(int i = 0; i < colCount; i++)
	{
		for(int j = i; j < colCount; j++ )
		{
			double x = 1.0;
			if ( i != j )
			{
				x = 0.0;
				for(int k = i; k < j; k++ )
					x -= row(j)[k] * row(k)[i];
			}
			row(j)[i] = x / row(j)[j];
		}
	}

	// Invert U
	for(int i = 0; i < colCount; i++)
	{
		for(int j = i; j < colCount; j++ )
		{
			if( i == j )
				continue;
			double sum = 0.0;
			for (int k = i; k < j; k++ )
				sum += row(k)[j] * ((i == k) ? 1.0 : row(i)[k]);
			row(i)[j] = -sum;
		}
	}

	// A^-1 = U^-1 x L^-1
	for(int i = 0; i < colCount; i++ )
	{
		for(int j = 0; j < colCount; j++ )
		{
			double sum = 0.0;
			for(int k = ((i > j) ? i : j); k < colCount; k++)
				sum += ((j == k) ? 1.0 : row(j)[k]) * row(k)[i];
			row(j)[i] = sum;
		}
	}
}
*/
void WMatrix::inPlaceSquareTranspose()
{
	size_t size = rows();
	if(size != (size_t)cols())
		throw Ex("Expected a square matrix");
	for(size_t a = 0; a < size; a++)
	{
		for(size_t b = a + 1; b < size; b++)
			std::swap(row(a)[b], row(b)[a]);
	}
}

double WMatrix_pythag(double a, double b)
{
	WAssert(a < 1e300);
	WAssert(b < 1e300);
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

double WMatrix_takeSign(double a, double b)
{
	return (b >= 0.0 ? std::abs(a) : -std::abs(a));
}

void WMatrix::singularValueDecomposition(WMatrix** ppU, double** ppDiag, WMatrix** ppV, bool throwIfNoConverge, size_t maxIters)
{
	if(rows() >= (size_t)cols())
		singularValueDecompositionHelper(ppU, ppDiag, ppV, throwIfNoConverge, maxIters);
	else
	{
		WMatrix* pTemp = transpose();
		std::unique_ptr<WMatrix> hTemp(pTemp);
		pTemp->singularValueDecompositionHelper(ppV, ppDiag, ppU, throwIfNoConverge, maxIters);
		(*ppV)->inPlaceSquareTranspose();
		(*ppU)->inPlaceSquareTranspose();
	}
}

double WMatrix_safeDivide(double n, double d)
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

void WMatrix::fixNans()
{
	size_t colCount = cols();
	for(size_t i = 0; i < rows(); i++)
	{
		WVec& r = row(i);
		for(size_t j = 0; j < colCount; j++)
		{
			if(r[j] >= -1e308 && r[j] < 1e308)
			{
			}
			else
				r[j] = (i == (size_t)j ? 1.0 : 0.0);
		}
	}
}

void WMatrix::singularValueDecompositionHelper(WMatrix** ppU, double** ppDiag, WMatrix** ppV, bool throwIfNoConverge, size_t maxIters)
{
	int m = (int)rows();
	int n = (int)cols();
	if(m < n)
		throw Ex("Expected at least as many rows as columns");
	int j, k;
	int l = 0;
	int p, q;
	double c, f, h, s, x, y, z;
	double norm = 0.0;
	double g = 0.0;
	double scale = 0.0;
	WMatrix* pU = new WMatrix(m, m);
	std::unique_ptr<WMatrix> hU(pU);
	pU->fill(0.0);
	WAssert((*this)[this->rows() - 1][this->cols() - 1] != UNKNOWN_REAL_VALUE);
	pU->copyBlock(*this, 0, 0, m, n, 0, 0, false);
	double* pSigma = new double[n];
	std::unique_ptr<double[]> hSigma(pSigma);
	WMatrix* pV = new WMatrix(n, n);
	std::unique_ptr<WMatrix> hV(pV);
	pV->fill(0.0);
	WTEMPBUF(double, temp, n);

	// Householder reduction to bidiagonal form
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
			for(k = i; k < m; k++)
				scale += std::abs(pU->row(k)[i]);
			if(scale != 0.0)
			{
				for(k = i; k < m; k++)
				{
					pU->row(k)[i] = WMatrix_safeDivide(pU->row(k)[i], scale);
					double t = pU->row(k)[i];
					s += t * t;
				}
				f = pU->row(i)[i];
				g = -WMatrix_takeSign(sqrt(s), f);
				h = f * g - s;
				pU->row(i)[i] = f - g;
				if(i != n - 1)
				{
					for(j = l; j < n; j++)
					{
						s = 0.0;
						for(k = i; k < m; k++)
							s += pU->row(k)[i] * pU->row(k)[j];
						f = WMatrix_safeDivide(s, h);
						for(k = i; k < m; k++)
							pU->row(k)[j] += f * pU->row(k)[i];
					}
				}
				for(k = i; k < m; k++)
					pU->row(k)[i] *= scale;
			}
		}
		pSigma[i] = scale * g;

		// Right-hand reduction
		g = 0.0;
		s = 0.0;
		scale = 0.0;
		if(i < m && i != n - 1)
		{
			for(k = l; k < n; k++)
				scale += std::abs(pU->row(i)[k]);
			if(scale != 0.0)
			{
				for(k = l; k < n; k++)
				{
					pU->row(i)[k] = WMatrix_safeDivide(pU->row(i)[k], scale);
					double t = pU->row(i)[k];
					s += t * t;
				}
				f = pU->row(i)[l];
				g = -WMatrix_takeSign(sqrt(s), f);
				h = f * g - s;
				pU->row(i)[l] = f - g;
				for(k = l; k < n; k++)
					temp[k] = WMatrix_safeDivide(pU->row(i)[k], h);
				if(i != m - 1)
				{
					for(j = l; j < m; j++)
					{
						s = 0.0;
						for(k = l; k < n; k++)
							s += pU->row(j)[k] * pU->row(i)[k];
						for(k = l; k < n; k++)
							pU->row(j)[k] += s * temp[k];
					}
				}
				for(k = l; k < n; k++)
					pU->row(i)[k] *= scale;
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
					pV->row(i)[j] = WMatrix_safeDivide(WMatrix_safeDivide(pU->row(i)[j], pU->row(i)[l]), g); // (double-division to avoid underflow)
				for(j = l; j < n; j++)
				{
					s = 0.0;
					for(k = l; k < n; k++)
						s += pU->row(i)[k] * pV->row(j)[k];
					for(k = l; k < n; k++)
						pV->row(j)[k] += s * pV->row(i)[k];
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
				pU->row(i)[j] = 0.0;
		}
		if(g != 0.0)
		{
			g = WMatrix_safeDivide(1.0, g);
			if(i != n - 1)
			{
				for(j = l; j < n; j++)
				{
					s = 0.0;
					for(k = l; k < m; k++)
						s += pU->row(k)[i] * pU->row(k)[j];
					f = WMatrix_safeDivide(s, pU->row(i)[i]) * g;
					for(k = i; k < m; k++)
						pU->row(k)[j] += f * pU->row(k)[i];
				}
			}
			for(j = i; j < m; j++)
				pU->row(j)[i] *= g;
		}
		else
		{
			for(j = i; j < m; j++)
				pU->row(j)[i] = 0.0;
		}
		pU->row(i)[i] += 1.0;
	}

	// Diagonalize the bidiagonal matrix
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
					h = WMatrix_pythag(f, g);
					pSigma[i] = h;
					h = WMatrix_safeDivide(1.0, h);
					c = g * h;
					s = -f * h;
					for(j = 0; j < m; j++)
					{
						y = pU->row(j)[q];
						z = pU->row(j)[i];
						pU->row(j)[q] = y * c + z * s;
						pU->row(j)[i] = z * c - y * s;
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
			f = WMatrix_safeDivide(((y - z) * (y + z) + (g - h) * (g + h)), (2.0 * h * y));
			g = WMatrix_pythag(f, 1.0);
			f = WMatrix_safeDivide(((x - z) * (x + z) + h * (WMatrix_safeDivide(y, (f + WMatrix_takeSign(g, f))) - h)), x);

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
				z = WMatrix_pythag(f, h);
				temp[j] = z;
				c = WMatrix_safeDivide(f, z);
				s = WMatrix_safeDivide(h, z);
				f = x * c + g * s;
				g = g * c - x * s;
				h = y * s;
				y = y * c;
				for(p = 0; p < n; p++)
				{
					x = pV->row(j)[p];
					z = pV->row(i)[p];
					pV->row(j)[p] = x * c + z * s;
					pV->row(i)[p] = z * c - x * s;
				}
				z = WMatrix_pythag(f, h);
				pSigma[j] = z;
				if(z != 0.0)
				{
					z = WMatrix_safeDivide(1.0, z);
					c = f * z;
					s = h * z;
				}
				f = c * g + s * y;
				x = c * y - s * g;
				for(p = 0; p < m; p++)
				{
					y = pU->row(p)[j];
					z = pU->row(p)[i];
					pU->row(p)[j] = y * c + z * s;
					pU->row(p)[i] = z * c - y * s;
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
	pU->fixNans();
	pV->fixNans();
	*ppU = hU.release();
	*ppDiag = hSigma.release();
	*ppV = hV.release();
}

WMatrix* WMatrix::pseudoInverse()
{
	WMatrix* pU;
	double* pDiag;
	WMatrix* pV;
	size_t colCount = cols();
	size_t rowCount = rows();
	if(rowCount < (size_t)colCount)
	{
		WMatrix* pTranspose = transpose();
		std::unique_ptr<WMatrix> hTranspose(pTranspose);
		pTranspose->singularValueDecompositionHelper(&pU, &pDiag, &pV, false, 80);
	}
	else
		singularValueDecompositionHelper(&pU, &pDiag, &pV, false, 80);
	std::unique_ptr<WMatrix> hU(pU);
	std::unique_ptr<double[]> hDiag(pDiag);
	std::unique_ptr<WMatrix> hV(pV);
	WMatrix sigma(rowCount < (size_t)colCount ? colCount : rowCount, rowCount < (size_t)colCount ? rowCount : colCount);
	sigma.fill(0.0);
	size_t m = std::min(rowCount, colCount);
	for(size_t i = 0; i < m; i++)
	{
		if(std::abs(pDiag[i]) > 1e-9)
			sigma[i][i] = WMatrix_safeDivide(1.0, pDiag[i]);
		else
			sigma[i][i] = 0.0;
	}
	WMatrix* pT = WMatrix::multiply(*pU, sigma, false, false);
	std::unique_ptr<WMatrix> hT(pT);
	if(rowCount < (size_t)colCount)
		return WMatrix::multiply(*pT, *pV, false, false);
	else
		return WMatrix::multiply(*pV, *pT, true, true);
}

// static
WMatrix* WMatrix::kabsch(WMatrix* pA, WMatrix* pB)
{
	WAssert((*pA)[pA->rows() - 1][pA->cols() - 1] != UNKNOWN_REAL_VALUE);
	WAssert((*pB)[pB->rows() - 1][pB->cols() - 1] != UNKNOWN_REAL_VALUE);
	WMatrix* pCovariance = WMatrix::multiply(*pA, *pB, true, false);
	std::unique_ptr<WMatrix> hCov(pCovariance);
	WMatrix* pU;
	double* pDiag;
	WMatrix* pV;
	pCovariance->singularValueDecomposition(&pU, &pDiag, &pV);
	std::unique_ptr<WMatrix> hU(pU);
	delete[] pDiag;
	std::unique_ptr<WMatrix> hV(pV);
	WMatrix* pK = WMatrix::multiply(*pV, *pU, true, true);
	return pK;
}

// static
WMatrix* WMatrix::align(WMatrix* pA, WMatrix* pB)
{
	size_t columns = pA->cols();
	WVec mean(columns);
	pA->centroid(mean);
	WMatrix aa(*pA);
	aa.centerMeanAtOrigin();
	WMatrix bb(*pB);
	bb.centerMeanAtOrigin();
	WMatrix* pK = WMatrix::kabsch(&bb, &aa);
	std::unique_ptr<WMatrix> hK(pK);
	WMatrix* pAligned = WMatrix::multiply(bb, *pK, false, true);
	std::unique_ptr<WMatrix> hAligned(pAligned);
	for(size_t i = 0; i < pAligned->rows(); i++)
		pAligned->row(i) += mean;
	return hAligned.release();
}

double WMatrix::determinant()
{
	// Check size
	size_t n = rows();
	if(n != cols())
		throw Ex("Only square matrices are supported");

	// Convert to a triangular matrix
	double epsilon = 1e-10;
	WMatrix C;
	C.copy(*this);
	WTEMPBUF(size_t, Kp, 2 * n);
	size_t* Lp = Kp + n;
	size_t l, ko, lo;
	double po, t0;
	bool nonSingular = true;
	size_t k = 0;
	while(nonSingular && k < n)
	{
		po = C[k][k];
		lo = k;
		ko = k;
		for(size_t i = k; i < n; i++)
			for(size_t j = k; j < n; j++)
				if(std::abs(C[i][j]) > std::abs(po))
				{
					po = C[i][j];
					lo = i;
					ko = j;
				}
		Lp[k] = lo;
		Kp[k] = ko;
		if(std::abs(po) < epsilon)
		{
			nonSingular = false;
			//throw Ex("Failed to compute determinant. Pivot too small.");
		}
		else
		{
			if(lo != k)
			{
				for(size_t j = k; j < n; j++)
				{
					t0 = C[k][j];
					C[k][j] = C[lo][j];
					C[lo][j] = t0;
				}
			}
			if(ko != k)
			{
				for(size_t i = 0; i < n; i++)
				{
					t0 = C[i][k];
					C[i][k] = C[i][ko];
					C[i][ko] = t0;
				}
			}
			for(size_t i = k + 1; i < n; i++)
			{
				C[i][k] /= po;
				for(size_t j = k + 1; j < n; j++)
					C[i][j] -= C[i][k] * C[k][j];
			}
			k++;
		}
	}
	if(nonSingular && std::abs(C[n - 1][n - 1]) < epsilon)
		nonSingular = false;

	// Compute determinant
	if(!nonSingular)
		return 0.0;
	else
	{
		double det = 1.0;
		for(k = 0; k < n; k++)
			det *= C[k][k];
		l = 0;
		for(k = 0; k < n - 1; k++)
		{
			if(Lp[k] != k)
				l++;
			if(Kp[k] != k)
				l++;
		}
		if((l % 2) != 0)
			det = -det;
		return det;
	}
}

void WMatrix::makeIdentity()
{
	size_t rowCount = rows();
	size_t colCount = cols();
	for(size_t nRow = 0; nRow < rowCount; nRow++)
		row(nRow).fill(0.0);
	size_t nMin = std::min((size_t)colCount, rowCount);
	for(size_t i = 0; i < nMin; i++)
		row(i)[i] = 1.0;
}

void WMatrix::mirrorTriangle(bool upperToLower)
{
	size_t n = std::min(rows(), (size_t)cols());
	if(upperToLower)
	{
		for(size_t i = 0; i < n; i++)
		{
			for(size_t j = i + 1; j < n; j++)
				row(j)[i] = row(i)[j];
		}
	}
	else
	{
		for(size_t i = 0; i < n; i++)
		{
			for(size_t j = i + 1; j < n; j++)
				row(i)[j] = row(j)[i];
		}
	}
}

double WMatrix::eigenValue(const WVec& pEigenVector)
{
	// Find the element with the largest magnitude
	size_t nEl = 0;
	size_t colCount = cols();
	for(size_t i = 1; i < colCount; i++)
	{
		if(std::abs(pEigenVector[i]) > std::abs(pEigenVector[nEl]))
			nEl = i;
	}
	return row(nEl).dotProduct(pEigenVector) / pEigenVector[nEl];
}

void WMatrix::eigenVector(double eigenvalue, WVec& outVector)
{
	WAssert(rows() == (size_t)cols()); // Expected a square matrix
	size_t rowCount = rows();
	for(size_t i = 0; i < rowCount; i++)
		row(i)[i] = row(i)[i] - eigenvalue;
	outVector.resize(rowCount);
	outVector.fill(0.0);
	if(!gaussianElimination(outVector.data()))
		throw Ex("no solution");
	outVector.normalize();
}

WMatrix* WMatrix::eigs(size_t nCount, WVec& eigenVals, WRand* pRand, bool mostSignificant)
{
	eigenVals.resize(nCount);
	size_t dims = cols();
	if(nCount > dims)
		throw Ex("Can't have more eigenvectors than columns");
	if(rows() != (size_t)dims)
		throw Ex("expected a square matrix");

	/*
	// The principle components of the Cholesky (square-root) matrix are the same as
	// the eigenvectors of this matrix.
	WMatrix* pDeviation = cholesky();
	std::unique_ptr<WMatrix> hDeviation(pDeviation);
	WMatrix* pData = pDeviation->transpose();
	std::unique_ptr<WMatrix> hData(pData);
	size_t s = pData->rows();
	for(size_t i = 0; i < s; i++)
	{
		double* pRow = pData->newRow();
		WVec::copy(pRow, pData->row(i), dims);
		WVec::multiply(pRow, -1, dims);
	}

	// Extract the principle components
	WMatrix* pOut = new WMatrix(m_pRelation->cloneMinimal());
	pOut->newRows(nCount);
	for(size_t i = 0; i < nCount; i++)
	{
		pData->principalComponentAboutOrigin(pOut->row(i), dims, pRand);
		pData->removeComponentAboutOrigin(pOut->row(i), dims);
	}
*/

	// Use the power method to compute the first few eigenvectors. todo: we really should use the Lanczos method instead
	WMatrix* pOut = new WMatrix(m_pRelation->cloneMinimal());
	pOut->newRows(nCount);
	WMatrix* pA;
	if(mostSignificant)
	{
		pA = new WMatrix();
		pA->copy(*this);
	}
	else
		pA = pseudoInverse();
	std::unique_ptr<WMatrix> hA(pA);
	WVec pTemp(dims);
	for(size_t i = 0; i < nCount; i++)
	{
		// Use the power method to compute the next eigenvector
		WVec& x = pOut->row(i);
		x.fillSphericalShell(*pRand);
		for(size_t j = 0; j < 100; j++) // todo: is there a better way to detect convergence?
		{
			pA->multiply(x, pTemp);
			x.copy(pTemp);
			x.normalize();
		}

		// Compute the corresponding eigenvalue
		double lambda = pA->eigenValue(x);
		eigenVals[i] = lambda;

		// Deflate (subtract out the eigenvector)
		for(size_t j = 0; j < dims; j++)
		{
			WVec& r = pA->row(j);
			for(size_t k = 0; k < dims; k++)
				r[k] = r[k] - lambda * x[j] * x[k];
		}
	}

	return pOut;
}
/*
WMatrix* WMatrix::leastSignificantEigenVectors(size_t nCount, WRand* pRand)
{
	WMatrix* pInv = cloneMinimal();
	std::unique_ptr<WMatrix> hInv(pInv);
	pInv->invert();
	WMatrix* pOut = pInv->mostSignificantEigenVectors(nCount, pRand);
	double eigenvalue;
	for(size_t i = 0; i < nCount; i++)
	{
		eigenvalue = 1.0 / pInv->eigenValue(pOut->row(i));
		WMatrix* cp = cloneMinimal();
		std::unique_ptr<WMatrix> hCp(cp);
		cp->eigenVector(eigenvalue, pOut->row(i));
	}
	return pOut;
}
*/
WVec& WMatrix::newRow()
{
	WVec* pNewVec = new WVec(m_pRelation->size());
	m_rows.push_back(pNewVec);
	return *pNewVec;
}

void WMatrix::newColumns(size_t n)
{
	size_t oldSize = m_pRelation->size();
	if(m_pRelation->type() == WRelation::UNIFORM)
	{
		size_t newSize = m_pRelation->size() + n;
		size_t vals = m_pRelation->valueCount(0);
		setRelation(nullptr);
		setRelation(new WUniformRelation(newSize, vals));
	}
	else
	{
		for(size_t i = 0; i < n; i++)
			((WMixedRelation*)m_pRelation)->addAttr(0);
	}
	for(size_t i = 0; i < rows(); i++)
		m_rows[i]->resizePreserve(oldSize + n);
}

void WMatrix::takeRow(WVec* pRow, size_t pos)
{
	if(pRow->size() != cols())
		throw Ex("Mismatching size");
	if(pos < m_rows.size())
		m_rows.insert(m_rows.begin() + pos, pRow);
	else
		m_rows.push_back(pRow);
}

void WMatrix::newRows(size_t nRows)
{
	reserve(m_rows.size() + nRows);
	for(size_t i = 0; i < nRows; i++)
		newRow();
}

void WMatrix::fromVector(const double* pVec, size_t nRows)
{
	if(rows() < nRows)
		newRows(nRows - rows());
	else
	{
		while(rows() > nRows)
			deleteRow(0);
	}
	size_t nCols = m_pRelation->size();
	for(size_t r = 0; r < nRows; r++)
	{
		WVec& pRow = row(r);
		pRow.copy(pVec, nCols);
		pVec += nCols;
	}
}

void WMatrix::toVector(double* pVec) const
{
	size_t nCols = cols();
	for(size_t i = 0; i < rows(); i++)
	{
		memcpy(pVec, row(i).data(), nCols * sizeof(double));
		pVec += nCols;
	}
}

void WMatrix::fill(double val, size_t colStart, size_t colCount)
{
	size_t count = std::min(cols() - colStart, colCount);
	for(size_t i = 0; i < rows(); i++)
		row(i).fill(val, colStart, count);
}

void WMatrix::fillUniform(WRand& rand, double min, double max)
{
	for(size_t i = 0; i < rows(); i++)
		row(i).fillUniform(rand, min, max);
}

void WMatrix::fillNormal(WRand& rand, double deviation)
{
	for(size_t i = 0; i < rows(); i++)
		row(i).fillNormal(rand, deviation);
}

void WMatrix::copy(const WMatrix& that, size_t rowStart, size_t colStart, size_t rowCount, size_t colCount)
{
	WAssert(this != &that);
	flush();
	setRelation(that.m_pRelation->cloneSub(colStart, std::min(that.cols() - colStart, colCount)));
	newRows(std::min(that.rows() - rowStart, rowCount));
	copyBlock(that, rowStart, colStart, rowCount, colCount, 0, 0, false);
}

void WMatrix::copyTranspose(WMatrix& that)
{
	resize(that.cols(), that.rows());
	for(size_t i = 0; i < that.rows(); i++)
	{
		for(size_t j = 0; j < that.cols(); j++)
			(*this)[j][i] = that[i][j];
	}
}

void WMatrix::copyBlock(const WMatrix& source, size_t srcRow, size_t srcCol, size_t hgt, size_t wid, size_t destRow, size_t destCol, bool checkMetaData)
{
	wid = std::min(wid, std::max((size_t)0, source.cols() - srcCol));
	hgt = std::min(hgt, std::max((size_t)0, source.rows() - srcRow));
	if(destRow + hgt > rows())
		throw Ex("Destination matrix has insufficient rows for this operation");
	if(destCol + wid > cols())
		throw Ex("Destination matrix has insufficient cols for this operation");
	if(checkMetaData)
	{
		if(relation().type() == WRelation::UNIFORM && source.relation().type() == WRelation::UNIFORM)
		{
			if(relation().valueCount(0) != source.relation().valueCount(0))
				throw Ex("Incompatible metadata");
		}
		else
		{
			for(size_t i = 0; i < wid; i++)
			{
				const WRelation& rs = source.relation();
				const WRelation& rd = relation();
				if(rs.valueCount(srcCol + i) != rd.valueCount(destCol + i))
					throw Ex("Incompatible metadata");
			}
		}
	}
	for(size_t i = 0; i < hgt; i++)
		memcpy(row(destRow + i).data() + destCol, source[srcRow + i].data() + srcCol, wid * sizeof(double));
}

void WMatrix::copyCols(const WMatrix& that, size_t firstCol, size_t colCount)
{
	if(that.cols() < firstCol + colCount)
		throw Ex("columns out of range");
	flush();
	setRelation(that.relation().cloneSub(firstCol, colCount));
	newRows(that.rows());
	copyBlock(that, 0, firstCol, that.rows(), colCount, 0, 0, false);
}

void WMatrix::swapRows(size_t a, size_t b)
{
	std::swap(m_rows[a], m_rows[b]);
}

void WMatrix::swapColumns(size_t nAttr1, size_t nAttr2)
{
	if(nAttr1 == nAttr2)
		return;
	m_pRelation->swapAttributes(nAttr1, nAttr2);
	size_t nCount = rows();
	for(size_t i = 0; i < nCount; i++)
	{
		WVec& r = row(i);
		std::swap(r[nAttr1], r[nAttr2]);
	}
}

void WMatrix::deleteColumns(size_t index, size_t count)
{
	m_pRelation->deleteAttributes(index, count);
	size_t rowCount = rows();
	for(size_t i = 0; i < rowCount; i++)
	{
		WVec& r = row(i);
		r.erase(index, count);
	}
}

WVec* WMatrix::releaseRow(size_t index)
{
	size_t last = m_rows.size() - 1;
	WVec* pRow = m_rows[index];
	m_rows[index] = m_rows[last];
	m_rows.pop_back();
	return pRow;
}

void WMatrix::deleteRow(size_t index)
{
	delete(releaseRow(index));
}

WVec* WMatrix::releaseRowPreserveOrder(size_t index)
{
	WVec* pRow = m_rows[index];
	m_rows.erase(m_rows.begin() + index);
	return pRow;
}

void WMatrix::deleteRowPreserveOrder(size_t index)
{
	delete(releaseRowPreserveOrder(index));
}

void WMatrix::releaseAllRows()
{
	m_rows.clear();
}

// static
WMatrix* WMatrix::mergeHoriz(const WMatrix* pSetA, const WMatrix* pSetB)
{
	if(pSetA->rows() != pSetB->rows())
		throw Ex("Expected same number of rows");
	WArffRelation* pRel = new WArffRelation();
	size_t nSetADims = pSetA->cols();
	size_t nSetBDims = pSetB->cols();
	pRel->addAttrs(pSetA->relation());
	pRel->addAttrs(pSetB->relation());
	WMatrix* pNewSet = new WMatrix(pRel);
	std::unique_ptr<WMatrix> hNewSet(pNewSet);
	pNewSet->reserve(pSetA->rows());
	for(size_t i = 0; i < pSetA->rows(); i++)
	{
		WVec& newRow = pNewSet->newRow();
		memcpy(newRow.data(), pSetA->row(i).data(), nSetADims * sizeof(double));
		memcpy(newRow.data() + nSetADims, pSetB->row(i).data(), nSetBDims * sizeof(double));
	}
	return hNewSet.release();
}

void WMatrix::shuffle(WRand& rand, WMatrix* pExtension)
{
	if(pExtension)
	{
		if(pExtension->rows() != rows())
			throw Ex("Expected pExtension to have the same number of rows");
		for(size_t n = m_rows.size(); n > 0; n--)
		{
			size_t r = (size_t)rand.next(n);
			std::swap(m_rows[r], m_rows[n - 1]);
			std::swap(pExtension->m_rows[r], pExtension->m_rows[n - 1]);
		}
	}
	else
	{
		for(size_t n = m_rows.size(); n > 0; n--)
			std::swap(m_rows[(size_t)rand.next(n)], m_rows[n - 1]);
	}
}

void WMatrix::shuffle2(WRand& rand, WMatrix& other)
{
	for(size_t n = m_rows.size(); n > 0; n--)
	{
		size_t r = (size_t)rand.next(n);
		std::swap(m_rows[r], m_rows[n - 1]);
		std::swap(other.m_rows[r], other.m_rows[n - 1]);
	}
}

void WMatrix::shuffleLikeCards()
{
	for(size_t i = 0; i < rows(); i++)
	{
		size_t n = i;
		while(n & 1)
			n = (n >> 1);
		n = (n >> 1) + rows() / 2;
		std::swap(m_rows[i], m_rows[n]);
	}
}

double WMatrix::entropy(size_t nColumn) const
{
	// Count the number of occurrences of each value
	WAssert(m_pRelation->valueCount(nColumn) > 0); // continuous attributes are not supported
	size_t nPossibleValues = m_pRelation->valueCount(nColumn);
	WTEMPBUF(size_t, pnCounts, nPossibleValues);
	size_t nTotalCount = 0;
	memset(pnCounts, '\0', m_pRelation->valueCount(nColumn) * sizeof(size_t));
	size_t nRows = rows();
	for(size_t n = 0; n < nRows; n++)
	{
		int nValue = (int)row(n)[nColumn];
		if(nValue < 0)
		{
			WAssert(nValue == UNKNOWN_DISCRETE_VALUE);
			continue;
		}
		WAssert(nValue < (int)nPossibleValues);
		pnCounts[nValue]++;
		nTotalCount++;
	}
	if(nTotalCount == 0)
		return 0;

	// Total up the entropy
	double dEntropy = 0;
	double dRatio;
	for(size_t n = 0; n < nPossibleValues; n++)
	{
		if(pnCounts[n] > 0)
		{
			dRatio = (double)pnCounts[n] / nTotalCount;
			dEntropy -= dRatio * log(dRatio);
		}
	}
	return M_LOG2E * dEntropy;
}

void WMatrix::splitByPivot(WMatrix* pGreaterOrEqual, size_t nAttribute, double dPivot, WMatrix* pExtensionA, WMatrix* pExtensionB)
{
	if(pExtensionA && pExtensionA->rows() != rows())
		throw Ex("Expected pExtensionA to have the same number of rows as this dataset");
	size_t nUnknowns = 0;
	size_t n;
	for(n = rows() - 1; n >= nUnknowns && n < rows(); n--)
	{
		WVec& r = row(n);
		if(r[nAttribute] == UNKNOWN_REAL_VALUE)
		{
			std::swap(m_rows[nUnknowns], m_rows[n]);
			if(pExtensionA)
				std::swap(pExtensionA->m_rows[nUnknowns], pExtensionA->m_rows[n]);
			nUnknowns++;
			n++;
		}
		else if(r[nAttribute] >= dPivot)
		{
			pGreaterOrEqual->takeRow(releaseRow(n));
			if(pExtensionA)
				pExtensionB->takeRow(pExtensionA->releaseRow(n));
		}
	}

	// Send all the unknowns to the side with more rows
	if(pGreaterOrEqual->rows() > rows() - nUnknowns)
	{
		for(; n < rows(); n--)
		{
			pGreaterOrEqual->takeRow(releaseRow(n));
			if(pExtensionA)
				pExtensionB->takeRow(pExtensionA->releaseRow(n));
		}
	}
}

void WMatrix::splitCategoricalKeepIfNotEqual(WMatrix* pSingleClass, size_t nAttr, int nValue, WMatrix* pExtensionA, WMatrix* pExtensionB)
{
	for(size_t i = rows() - 1; i < rows(); i--)
	{
		WVec& vec = row(i);
		if((int)vec[nAttr] == nValue)
		{
			pSingleClass->takeRow(releaseRow(i));
			if(pExtensionA)
				pExtensionB->takeRow(pExtensionA->releaseRow(i));
		}
	}
}

void WMatrix::splitCategoricalKeepIfEqual(WMatrix* pOtherValues, size_t nAttr, int nValue, WMatrix* pExtensionA, WMatrix* pExtensionB)
{
	for(size_t i = rows() - 1; i < rows(); i--)
	{
		WVec& vec = row(i);
		if((int)vec[nAttr] != nValue)
		{
			pOtherValues->takeRow(releaseRow(i));
			if(pExtensionA)
				pExtensionB->takeRow(pExtensionA->releaseRow(i));
		}
	}
}

void WMatrix::splitBySize(WMatrix& other, size_t nOtherRows)
{
	if(nOtherRows > rows())
		throw Ex("row count out of range");
	size_t a = other.rows();
	size_t targetSize = a + nOtherRows;
	other.reserve(targetSize);
	while(other.rows() < targetSize)
		other.takeRow(releaseRow(rows() - 1));

	// Restore the original order to the other data
	if(other.rows() == 0)
		return;
	size_t b = other.rows() - 1;
	while(b > a)
	{
		other.swapRows(a, b);
		a++;
		b--;
	}
}

void WMatrix::mergeVert(WMatrix* pData, bool ignoreMismatchingName)
{
	if(relation().type() == WRelation::ARFF && pData->relation().type() == WRelation::ARFF)
	{
		// Make an value mapping for pData
		const WArffRelation& a = (WArffRelation&)relation();
		const WArffRelation& b = (WArffRelation&)pData->relation();
		if(a.size() != b.size())
			throw Ex("Mismatching number of columns");
		vector< vector<size_t> > valueMap;
		valueMap.resize(a.size());
		for(size_t i = 0; i < a.size(); i++)
		{
			if(!ignoreMismatchingName && strcmp(a.attrName(i), b.attrName(i)) != 0)
				throw Ex("The name of attribute ", to_str(i), " does not match");
			if(a.valueCount(i) == 0 && b.valueCount(i) != 0)
				throw Ex("Attribute ", to_str(i), " is continuous in one matrix and nominal in the other");
			if(a.valueCount(i) != 0 && b.valueCount(i) == 0)
				throw Ex("Attribute ", to_str(i), " is continuous in one matrix and nominal in the other");
			vector<size_t>& vm = valueMap[i];
			const WArffAttribute& attrThis = a.m_attrs[i];
			const WArffAttribute& attrThat = b.m_attrs[i];
			for(size_t j = 0; j < b.valueCount(i); j++)
			{
				if(attrThis.m_values.size() >= a.valueCount(i) && attrThat.m_values.size() >= j && attrThat.m_values[j].length() > 0)
				{
					int newVal = a.findEnumeratedValue(i, attrThat.m_values[j].c_str());
					if(newVal == UNKNOWN_DISCRETE_VALUE)
						newVal = ((WArffRelation*)m_pRelation)->addAttrValue(i, attrThat.m_values[j].c_str());
					vm.push_back(newVal);
				}
				else
					vm.push_back(j);
			}
		}

		// Merge the data and map the values in pData to match those in this Matrix with the same name
		for(size_t j = 0; j < pData->rows(); j++)
		{
			WVec* pRow = &pData->row(j);
			takeRow(pRow);
			for(size_t i = 0; i < a.size(); i++)
			{
				if(a.valueCount(i) != 0 && (*pRow)[i] != UNKNOWN_DISCRETE_VALUE)
				{
					vector<size_t>& vm = valueMap[i];
					int oldVal = (int)(*pRow)[i];
					WAssert(oldVal >= 0 && (size_t)oldVal < vm.size());
					(*pRow)[i] = (double)vm[oldVal];
				}
			}
		}
		pData->releaseAllRows();
	}
	else
	{
		if(!relation().isCompatible(pData->relation()))
			throw Ex("The two matrices have incompatible relations");
		for(size_t i = 0; i < pData->rows(); i++)
			takeRow(&pData->row(i));
		pData->releaseAllRows();
	}
}

double WMatrix::columnSum(size_t col) const
{
	double sum = 0;
	for(size_t i = 0; i < rows(); i++)
	{
		if((*this)[i][col] != UNKNOWN_REAL_VALUE)
			sum += (*this)[i][col];
	}
	return sum;
}

double WMatrix::columnMean(size_t nAttribute, const WVec* pWeights, bool throwIfEmpty) const
{
	if(nAttribute >= cols())
		throw Ex("attribute index out of range");
	if(pWeights)
	{
		double sum = 0.0;
		double sumWeight = 0.0;
		for(size_t i = 0; i < rows(); i++)
		{
			if((*this)[i][nAttribute] != UNKNOWN_REAL_VALUE)
			{
				sum += (*pWeights)[i] * (*this)[i][nAttribute];
				sumWeight += (*pWeights)[i];
			}
		}
		if(sumWeight > 0.0)
			return sum / sumWeight;
		else
		{
			if(throwIfEmpty)
				throw Ex("No values have any weight while computing mean");
			else
				return UNKNOWN_REAL_VALUE;
		}
	}
	else
	{
		double sum = 0;
		size_t missing = 0;
		for(size_t i = 0; i < rows(); i++)
		{
			if((*this)[i][nAttribute] == UNKNOWN_REAL_VALUE)
				missing++;
			else
				sum += (*this)[i][nAttribute];
		}
		size_t count = m_rows.size() - missing;
		if(count > 0)
			return sum / count;
		else
		{
			if(throwIfEmpty)
				throw Ex("At least one value is required to compute a mean");
			else
				return UNKNOWN_REAL_VALUE;
		}
	}
}

double WMatrix::columnMedian(size_t nAttribute, bool throwIfEmpty) const
{
	if(nAttribute >= cols())
		throw Ex("attribute index out of range");
	vector<double> vals;
	vals.reserve(rows());
	for(size_t i = 0; i < rows(); i++)
	{
		double d = (*this)[i][nAttribute];
		if(d != UNKNOWN_REAL_VALUE)
			vals.push_back(d);
	}
	if(vals.size() < 1)
	{
		if(throwIfEmpty)
			throw Ex("at least one value is required to compute a median");
		else
			return UNKNOWN_REAL_VALUE;
	}
	if(vals.size() & 1)
	{
		vector<double>::iterator med = vals.begin() + (vals.size() / 2);
		std::nth_element(vals.begin(), med, vals.end());
		return *med;
	}
	else
	{
		vector<double>::iterator a = vals.begin() + (vals.size() / 2 - 1);
		std::nth_element(vals.begin(), a, vals.end());
		vector<double>::iterator b = std::min_element(a + 1, vals.end());
		return 0.5 * (*a + *b);
	}
}

void WMatrix::centroid(WVec& outCentroid, const WVec* pWeights) const
{
	size_t c = cols();
	outCentroid.resize(c);
	for(size_t n = 0; n < c; n++)
		outCentroid[n] = columnMean(n, pWeights);
}

double WMatrix::columnSquaredMagnitude(size_t column) const
{
	double dSum = 0;
	for(size_t i = 0; i < rows(); i++)
	{
		if((*this)[i][column] == UNKNOWN_REAL_VALUE)
			continue;
		double d = (*this)[i][column];
		dSum += (d * d);
	}
	return dSum;
}

double WMatrix::columnVariance(size_t nAttr, double mean) const
{
	double d;
	double dSum = 0;
	size_t nMissing = 0;
	for(size_t i = 0; i < rows(); i++)
	{
		if((*this)[i][nAttr] == UNKNOWN_REAL_VALUE)
		{
			nMissing++;
			continue;
		}
		d = (*this)[i][nAttr] - mean;
		dSum += (d * d);
	}
	size_t nCount = m_rows.size() - nMissing;
	if(nCount > 1)
		return dSum / (nCount - 1);
	else
		return 0; // todo: wouldn't UNKNOWN_REAL_VALUE be better here?
}

double WMatrix::columnMin(size_t nAttribute) const
{
	double d = 1e300;
	for(size_t i = 0; i < rows(); i++)
	{
		if((*this)[i][nAttribute] == UNKNOWN_REAL_VALUE)
			continue;
		if((*this)[i][nAttribute] < d)
			d = (*this)[i][nAttribute];
	}
	if(d == 1e300)
		return UNKNOWN_REAL_VALUE;
	else
		return d;
}

double WMatrix::columnMax(size_t nAttribute) const
{
	double d = -1e300;
	for(size_t i = 0; i < rows(); i++)
	{
		if((*this)[i][nAttribute] == UNKNOWN_REAL_VALUE)
			continue;
		if((*this)[i][nAttribute] > d)
			d = (*this)[i][nAttribute];
	}
	if(d == -1e300)
		return UNKNOWN_REAL_VALUE;
	else
		return d;
}

void WMatrix::scaleColumn(size_t col, double scalar)
{
	for(size_t i = 0; i < rows(); i++)
		(*this)[i][col] *= scalar;
}

void WMatrix::normalizeColumn(size_t column, double dInMin, double dInMax, double dOutMin, double dOutMax)
{
	WAssert(dInMax > dInMin);
	double dScale = (dOutMax - dOutMin) / (dInMax - dInMin);
	for(size_t i = 0; i < rows(); i++)
	{
		if((*this)[i][column] != UNKNOWN_REAL_VALUE)
		{
			(*this)[i][column] -= dInMin;
			(*this)[i][column] *= dScale;
			(*this)[i][column] += dOutMin;
		}
	}
}

void WMatrix::clipColumn(size_t column, double dMin, double dMax)
{
	WAssert(dMax > dMin);
	for(size_t i = 0; i < rows(); i++)
		(*this)[i][column] = std::max(dMin, std::min(dMax, (*this)[i][column]));
}

/*static*/ double WMatrix::normalizeValue(double dVal, double dInMin, double dInMax, double dOutMin, double dOutMax)
{
	WAssert(dInMax > dInMin);
	dVal -= dInMin;
	dVal /= (dInMax - dInMin);
	dVal *= (dOutMax - dOutMin);
	dVal += dOutMin;
	return dVal;
}

double WMatrix::baselineValue(size_t nAttribute) const
{
	if(m_pRelation->valueCount(nAttribute) == 0)
		return columnMean(nAttribute, nullptr, false);
	int j;
	int val;
	int nValues = (int)m_pRelation->valueCount(nAttribute);
	WTEMPBUF(size_t, counts, nValues + 1); // We add 1 here so that UNKNOWN_DISCRETE_VALUE, which is -1, will be counted as a unique value, so we don't have to test for it
	memset(counts, '\0', sizeof(size_t) * (nValues + 1));
	for(size_t i = 0; i < rows(); i++)
	{
		val = (int)(*this)[i][nAttribute] + 1;
		WAssert(val >= 0 && val <= nValues);
		counts[val]++;
	}
	val = 1; // We ignore element 0 because we don't care whether UNKNOWN_DISCRETE_VALUE is the most common value
	for(j = 2; j <= nValues; j++)
	{
		if(counts[j] > counts[val])
			val = j;
	}
	return (double)(val - 1);
}

bool WMatrix::isAttrHomogenous(size_t column) const
{
	if(m_pRelation->valueCount(column) > 0)
	{
		int d;
		size_t i;
		for(i = 0; i < rows(); i++)
		{
			d = (int)(*this)[i][column];
			if(d != UNKNOWN_DISCRETE_VALUE)
			{
				i++;
				break;
			}
		}
		for(; i < rows(); i++)
		{
			int t = (int)(*this)[i][column];
			if(t != d && t != UNKNOWN_DISCRETE_VALUE)
				return false;
		}
	}
	else
	{
		double d;
		size_t i;
		for(i = 0; i < rows(); i++)
		{
			d = (*this)[i][column];
			if(d != UNKNOWN_REAL_VALUE)
			{
				i++;
				break;
			}
		}
		for(; i < rows(); i++)
		{
			double t = (*this)[i][column];
			if(t != d && t != UNKNOWN_REAL_VALUE)
				return false;
		}
	}
	return true;
}

bool WMatrix::isHomogenous() const
{
	for(size_t i = 0; i < cols(); i++)
	{
		if(!isAttrHomogenous(i))
			return false;
	}
	return true;
}

void WMatrix::replaceMissingValuesWithBaseline(size_t nAttr)
{
	double bl = baselineValue(nAttr);
	size_t count = rows();
	if(m_pRelation->valueCount(nAttr) == 0)
	{
		for(size_t i = 0; i < count; i++)
		{
			if(row(i)[nAttr] == UNKNOWN_REAL_VALUE)
				row(i)[nAttr] = bl;
		}
	}
	else
	{
		for(size_t i = 0; i < count; i++)
		{
			if(row(i)[nAttr] == UNKNOWN_DISCRETE_VALUE)
				row(i)[nAttr] = bl;
		}
	}
}

void WMatrix::replaceMissingValuesRandomly(size_t nAttr, WRand* pRand)
{
	WTEMPBUF(size_t, indexes, rows());

	// Find the rows that are not missing values in this attribute
	size_t* pCur = indexes;
	double dOk = m_pRelation->valueCount(nAttr) == 0 ? -1e300 : 0;
	for(size_t i = 0; i < rows(); i++)
	{
		if(row(i)[nAttr] >= dOk)
		{
			*pCur = i;
			pCur++;
		}
	}

	// Replace missing values
	size_t nonMissing = pCur - indexes;
	for(size_t i = 0; i < rows(); i++)
	{
		if(row(i)[nAttr] < dOk)
			row(i)[nAttr] = row(indexes[(size_t)pRand->next(nonMissing)])[nAttr];
	}
}

void WMatrix::principalComponent(WVec& outVector, const WVec& centroi, WRand* pRand) const
{
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
			const WVec& vec = row(n);
			d = 0.0;
			for(size_t j = 0; j < dims; j++)
				d += (vec[j] - centroi[j]) * outVector[j];
			for(size_t j = 0; j < dims; j++)
				pAccumulator[j] += d * (vec[j] - centroi[j]);
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

void WMatrix::principalComponentAboutOrigin(WVec& outVector, WRand* pRand) const
{
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
			const WVec& vec = row(n);
			d = vec.dotProduct(outVector);
			for(size_t j = 0; j < dims; j++)
				pAccumulator[j] += d * vec[j];
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

void WMatrix::principalComponentIgnoreUnknowns(WVec& outVector, const WVec& centroi, WRand* pRand) const
{
	if(!doesHaveAnyMissingValues())
	{
		principalComponent(outVector, centroi, pRand);
		return;
	}

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
			const WVec& vec = row(n);
			d = 0.0;
			for(size_t j = 0; j < dims; j++)
				d += (vec[j] - centroi[j]) * outVector[j];
			for(size_t j = 0; j < dims; j++)
			{
				if(vec[j] != UNKNOWN_REAL_VALUE)
					pAccumulator[j] += d * (vec[j] - centroi[j]);
			}
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

void WMatrix::weightedPrincipalComponent(WVec& outVector, const WVec& centroi, const double* pWeights, WRand* pRand) const
{
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
		const double* pW = pWeights;
		for(size_t n = 0; n < nCount; n++)
		{
			const WVec& vec = row(n);
			d = 0.0;
			for(size_t j = 0; j < dims; j++)
				d += (vec[j] - centroi[j]) * outVector[j];
			for(size_t j = 0; j < dims; j++)
				pAccumulator[j] += (*pW) * d * (vec[j] - centroi[j]);
			pW++;
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

// static
size_t vec_indexOfMaxMagnitude(const double* pVector, size_t dims, WRand* pRand)
{
	size_t index = 0;
	size_t count = 1;
	for(size_t n = 1; n < dims; n++)
	{
		if(std::abs(pVector[n]) >= std::abs(pVector[index]))
		{
			if(std::abs(pVector[n]) == std::abs(pVector[index]))
			{
				count++;
				if(pRand->next(count) == 0)
					index = n;
			}
			else
			{
				index = n;
				count = 1;
			}
		}
	}
	return index;
}

double WMatrix::eigenValue(const double* pMean, const double* pEigenVector, WRand* pRand) const
{
	// Use the element of the eigenvector with the largest magnitude,
	// because that will give us the least rounding error when we compute the eigenvalue.
	size_t dims = cols();
	size_t index = vec_indexOfMaxMagnitude(pEigenVector, dims, pRand);

	// The eigenvalue is the factor by which the eigenvector is scaled by the covariance matrix,
	// so we compute just the part of the covariance matrix that we need to see how much the
	// max-magnitude element of the eigenvector is scaled.
	double d = 0;
	for(size_t i = 0; i < dims; i++)
		d += covariance(index, pMean[index], i, pMean[i]) * pEigenVector[i];
	return d / pEigenVector[index];
}

void WMatrix::removeComponent(const WVec& mean, const WVec& component)
{
	WAssert(mean.size() == cols());
	WAssert(component.size() == cols());
	size_t dims = cols();
	size_t nCount = rows();
	for(size_t i = 0; i < nCount; i++)
	{
		WVec& vec = row(i);
		double d = 0.0;
		for(size_t j = 0; j < dims; j++)
		{
			if(vec[j] != UNKNOWN_REAL_VALUE)
				d += (vec[j] - mean[j]) * component[j];
		}
		for(size_t j = 0; j < dims; j++)
		{
			if(vec[j] != UNKNOWN_REAL_VALUE)
				vec[j] -= d * component[j];
		}
	}
}

void WMatrix::removeComponentAboutOrigin(const WVec& component)
{
	size_t dims = cols();
	size_t nCount = rows();
	for(size_t i = 0; i < nCount; i++)
	{
		WVec& vec = row(i);
		double d = vec.dotProduct(component);
		for(size_t j = 0; j < dims; j++)
			vec[j] -= d * component[j];
	}
}

void WMatrix::centerMeanAtOrigin()
{
	//Calculate mean
	size_t dims = cols();
	WVec mean(dims);
	centroid(mean);
	//Skip non-continuous columns by setting their mean to 0
	for(unsigned i = 0; i < dims; ++i)
	{
		if(relation().valueCount(i) != 0) { mean[i] = 0; }
	}
	//Subtract the new mean from all rows
	for(size_t i = 0; i < rows(); i++)
		(*this)[i] -= mean;
}

size_t WMatrix::countPrincipalComponents(double d, WRand* pRand) const
{
	size_t dims = cols();
	WMatrix tmpData(relation().cloneMinimal());
	tmpData.copy(*this);
	tmpData.centerMeanAtOrigin();
	WVec vec(dims);
	WVec origin(tmpData.cols());
	origin.fill(0.0);
	double thresh = d * d * tmpData.sumSquaredDistance(origin);
	size_t i;
	for(i = 1; i < dims; i++)
	{
		tmpData.principalComponentAboutOrigin(vec, pRand);
		tmpData.removeComponentAboutOrigin(vec);
		if(tmpData.sumSquaredDistance(origin) < thresh)
			break;
	}
	return i;
}

double WMatrix::sumSquaredDistance(const WVec& point) const
{
	double err = 0;
	for(size_t i = 0; i < rows(); i++)
		err += point.squaredDistance(row(i));
	return err;
}

double WMatrix::columnSumSquaredDifference(const WMatrix& that, size_t column, double* pOutSAE) const
{
	if(that.rows() != rows())
		throw Ex("Mismatching number of rows");
	if(column >= cols() || column >= that.cols())
		throw Ex("column index out of range");
	double sse = 0.0;
	double sae = 0.0;
	if(relation().valueCount(column) == 0)
	{
		for(size_t i = 0; i < rows(); i++)
		{
			double d = row(i)[column] - that.row(i)[column];
			sse += (d * d);
			sae += std::abs(d);
		}
	}
	else
	{
		for(size_t i = 0; i < rows(); i++)
		{
			if((int)row(i)[column] != (int)that.row(i)[column])
			{
				sse++;
				sae++;
			}
		}
	}
	if(pOutSAE)
		*pOutSAE = sae;
	return sse;
}

double WMatrix::sumSquaredDifference(const WMatrix& that, bool transposeThat) const
{
	if(transposeThat)
	{
		size_t colCount = (size_t)cols();
		if(rows() != (size_t)that.cols() || colCount != that.rows())
			throw Ex("expected matrices of same size");
		double err = 0;
		for(size_t i = 0; i < rows(); i++)
		{
			const WVec& r = row(i);
			for(size_t j = 0; j < colCount; j++)
			{
				double d = r[j] - that[j][i];
				err += (d * d);
			}
		}
		return err;
	}
	else
	{
		if(this->rows() != that.rows() || this->cols() != that.cols())
			throw Ex("mismatching sizes");
		double d = 0;
		for(size_t i = 0; i < rows(); i++)
			d += this->row(i).squaredDistance(that[i]);
		return d;
	}
}

double WMatrix::linearCorrelationCoefficient(size_t attr1, double attr1Origin, size_t attr2, double attr2Origin) const
{
	double sx = 0;
	double sy = 0;
	double sxy = 0;
	double mx, my;
	size_t count = rows();
	size_t i;
	for(i = 0; i < count; i++)
	{
		const WVec& pat = row(i);
		mx = pat[attr1] - attr1Origin;
		my = pat[attr2] - attr2Origin;
		if(pat[attr1] == UNKNOWN_REAL_VALUE || pat[attr2] == UNKNOWN_REAL_VALUE)
			continue;
		break;
	}
	if(i >= count)
		return 0;
	double d, x, y;
	size_t j = 1;
	for(i++; i < count; i++)
	{
		const WVec& pat = row(i);
		if(pat[attr1] == UNKNOWN_REAL_VALUE || pat[attr2] == UNKNOWN_REAL_VALUE)
			continue;
		x = pat[attr1] - attr1Origin;
		y = pat[attr2] - attr2Origin;
		d = (double)j / (j + 1);
		sx += (x - mx) * (x - mx) * d;
		sy += (y - my) * (y - my) * d;
		sxy += (x - mx) * (y - my) * d;
		mx += (x - mx) / (j + 1);
		my += (y - my) / (j + 1);
		j++;
	}
	if(sx == 0 || sy == 0 || sxy == 0)
		return 0;
	return (sxy / j) / (sqrt(sx / j) * sqrt(sy / j));
}

double WMatrix::covariance(size_t nAttr1, double dMean1, size_t nAttr2, double dMean2, const double* pWeights) const
{
	if(pWeights)
	{
		double sum = 0;
		double sumWeight = 0.0;
		for(size_t i = 0; i < rows(); i++)
		{
			const WVec& vec = row(i);
			sum += ((vec[nAttr1] - dMean1) * (vec[nAttr2] - dMean2) * (*pWeights) * (*pWeights));
			sumWeight += (*pWeights);
			pWeights++;
		}
		return sum / std::max(1e-6, sumWeight - 1.0);
	}
	else
	{
		double sum = 0;
		for(size_t i = 0; i < rows(); i++)
		{
			const WVec& vec = row(i);
			sum += ((vec[nAttr1] - dMean1) * (vec[nAttr2] - dMean2));
		}
		return sum / (rows() - 1);
	}
}

WMatrix* WMatrix::covarianceMatrix() const
{
	size_t colCount = cols();
	WMatrix* pOut = new WMatrix(colCount, colCount);

	// Compute the deviations
	WTEMPBUF(double, pMeans, colCount);
	for(size_t i = 0; i < colCount; i++)
		pMeans[i] = columnMean(i);

	// Compute the covariances for half the matrix
	for(size_t i = 0; i < colCount; i++)
	{
		WVec& r = pOut->row(i);
		for(size_t n = i; n < colCount; n++)
			r[n] = covariance(i, pMeans[i], n, pMeans[n]);
	}

	// Fill out the other half of the matrix
	for(size_t i = 1; i < colCount; i++)
	{
		WVec& r = pOut->row(i);
		for(size_t n = 0; n < i; n++)
			r[n] = pOut->row(n)[i];
	}
	return pOut;
}

double WMatrix::boundingSphere(WVec& center, size_t* pIndexes, size_t indexCount, WDistanceMetric* pMetric) const
{
	size_t dims = cols();
	center.resize(dims);
	if(indexCount < 2)
	{
		if(indexCount < 1)
			throw Ex("Need at least one point");
		center.copy(row(pIndexes[0]));
		return 1e-18;
	}

	// Find the two farthest points
	const WVec& a = row(pIndexes[0]);
	size_t b = 1;
	double sdist = pMetric->squaredDistance(a, row(pIndexes[b]));
	for(size_t i = 2; i < indexCount; i++)
	{
		double cand = pMetric->squaredDistance(a, row(pIndexes[i]));
		if(cand > sdist)
		{
			sdist = cand;
			b = i;
		}
	}
	const WVec& pB = row(pIndexes[b]);
	size_t c = 0;
	sdist = pMetric->squaredDistance(pB, row(pIndexes[c]));
	for(size_t i = 1; i < indexCount; i++)
	{
		if(i == b)
			continue;
		double cand = pMetric->squaredDistance(pB, row(pIndexes[i]));
		if(cand > sdist)
		{
			sdist = cand;
			c = i;
		}
	}

	// Compute initial center and radius
	double sradius = 0.25 * sdist;
	center.copy(row(pIndexes[b]));
	center += row(pIndexes[c]);
	center *= 0.5;

	// Refine and grow the bounding sphere until it definitely includes all the points
	while(true)
	{
		size_t externals = 0;
		for(size_t i = 0; i < indexCount; i++)
		{
			const WVec& cand = row(pIndexes[i]);
			sdist = pMetric->squaredDistance(cand, center);
			if(sdist > sradius)
			{
				externals++;

				// This is supposed to be the ideal way to grow the radius, but it doesn't actually seem to work very well
				//sradius = 0.25 * (sradius * sradius / sdist + sradius + sradius + sdist);

				// This is a simpler way to increase the radius
				//sradius *= 1.02;

				// Increase the radius using some magical heuristic I invented, but cannot remember how it works anymore
				double tmp = (sradius + sdist) * 1.01; // This is the grow rate for the radius
				sradius = tmp * tmp / (4.0 * sdist);

				// Move the center just enough to enclose the candidate point
				double dist = sqrt(sdist);
				double stepFac = (dist - sqrt(sradius)) / dist;
				for(size_t j = 0; j < dims; j++)
					center[j] += stepFac * (cand[j] - center[j]);

				// Increase the radius slightly to prevent precision issues
				sradius += 1e-9;
			}
		}
		if(externals == 0)
			break;
	}
	return sradius;
}

void WMatrix::sort(size_t dim)
{
	std::ranges::sort(m_rows, std::ranges::less{}, [dim](const WVec* pRow)
	{ return (*pRow)[dim]; });
}

void WMatrix::sortPartial(size_t rowIndex, size_t colIndex)
{
	std::ranges::nth_element(m_rows, m_rows.begin() + rowIndex, std::ranges::less{}, [colIndex](const WVec* pRow)
	{ return (*pRow)[colIndex]; });
}

void WMatrix::reverseRows()
{
	std::reverse(m_rows.begin(), m_rows.end());
}

double WMatrix_PairedTTestHelper(void* pThis, double x)
{
	double v = *(double*)pThis;
	return pow(1.0 + x * x / v, -(v + 1) / 2);
}

void WMatrix::pairedTTest(size_t* pOutV, double* pOutT, size_t attr1, size_t attr2, bool normalize) const
{
	double a, b, m;
	double asum = 0;
	double asumOfSquares = 0;
	double bsum = 0;
	double bsumOfSquares = 0;
	size_t rowCount = rows();
	for(size_t i = 0; i < rowCount; i++)
	{
		const WVec& pat = row(i);
		a = pat[attr1];
		b = pat[attr2];
		if(normalize)
		{
			m = (a + b) / 2;
			a /= m;
			b /= m;
		}
		asum += a;
		asumOfSquares += (a * a);
		bsum += b;
		bsumOfSquares += (b * b);
	}
	double amean = asum / rowCount;
	double avariance = (asumOfSquares / rowCount - amean * amean) * rowCount / (rowCount - 1);
	double bmean = bsum / rowCount;
	double bvariance = (bsumOfSquares / rowCount - bmean * bmean) * rowCount / (rowCount - 1);
	double grand = sqrt((avariance + bvariance) / rowCount);
	*pOutV = 2 * rowCount - 2;
	*pOutT = std::abs(bmean - amean) / grand;
}

void WMatrix::wilcoxonSignedRanksTest(size_t attr1, size_t attr2, double tolerance, int* pNum, double* pWMinus, double* pWPlus) const
{
	// Make sorted list of differences
	WMatrix tmp(0, 2); // col 0 holds the absolute difference. col 1 holds the sign.
	for(size_t i = 0; i < rows(); i++)
	{
		const WVec& pat = row(i);
		double absdiff = std::abs(pat[attr2] - pat[attr1]);
		if(absdiff > tolerance)
		{
			WVec& stat = tmp.newRow();
			stat[0] = absdiff;
			if(pat[attr1] < pat[attr2])
				stat[1] = -1;
			else
				stat[1] = 1;
		}
	}

	// Convert column 0 to ranks
	tmp.sort(0);
	double prev = UNKNOWN_REAL_VALUE;
	size_t index = 0;
	size_t j;
	double ave;
	for(size_t i = 0; i < tmp.rows(); i++)
	{
		WVec& pat = tmp[i];
		if(std::abs(pat[0] - prev) >= tolerance)
		{
			ave = (double)(index + 1 + i) / 2;
			for(j = index; j < i; j++)
			{
				WVec& stat = tmp[j];
				stat[0] = ave;
			}
			prev = pat[0];
			index = i;
		}
	}
	ave = (double)(index + 1 + tmp.rows()) / 2;
	for(j = index; j < tmp.rows(); j++)
	{
		WVec& stat = tmp[j];
		stat[0] = ave;
	}

	// Sum up the scores
	double a = 0;
	double b = 0;
	for(size_t i = 0; i < tmp.rows(); i++)
	{
		WVec& stat = tmp[i];
		if(stat[1] > 0)
			a += stat[0];
		else if(stat[1] < 0)
			b += stat[0];
		else
		{
			a += 0.5 * stat[0];
			b += 0.5 * stat[0];
		}
	}
	*pNum = (int)tmp.rows();
	*pWMinus = b;
	*pWPlus = a;
}

size_t WMatrix::countValue(size_t attribute, double value) const
{
	size_t count = 0;
	for(size_t i = 0; i < rows(); i++)
	{
		if(row(i)[attribute] == value)
			count++;
	}
	return count;
}

bool WMatrix::doesHaveAnyMissingValues() const
{
	size_t dims = m_pRelation->size();
	for(size_t j = 0; j < dims; j++)
	{
		if(m_pRelation->valueCount(j) == 0)
		{
			for(size_t i = 0; i < rows(); i++)
			{
				if(row(i)[j] == UNKNOWN_REAL_VALUE)
					return true;
			}
		}
		else
		{
			for(size_t i = 0; i < rows(); i++)
			{
				if(row(i)[j] == UNKNOWN_DISCRETE_VALUE)
					return true;
			}
		}
	}
	return false;
}

void WMatrix::ensureDataHasNoMissingReals() const
{
	size_t dims = m_pRelation->size();
	for(size_t i = 0; i < rows(); i++)
	{
		const WVec& pat = row(i);
		for(size_t j = 0; j < dims; j++)
		{
			if(m_pRelation->valueCount(j) != 0)
				continue;
			if(pat[i] == UNKNOWN_REAL_VALUE)
				throw Ex("Missing values in continuous attributes are not supported");
		}
	}
}

void WMatrix::ensureDataHasNoMissingNominals() const
{
	size_t dims = m_pRelation->size();
	for(size_t i = 0; i < rows(); i++)
	{
		const WVec& pat = row(i);
		for(size_t j = 0; j < dims; j++)
		{
			if(m_pRelation->valueCount(j) == 0)
				continue;
			if((int)pat[i] == UNKNOWN_DISCRETE_VALUE)
				throw Ex("Missing values in nominal attributes are not supported");
		}
	}
}

void WMatrix::print(ostream& stream, char separator) const
{
	m_pRelation->print(stream);
	std::streamsize oldPrecision = stream.precision(14);
	for(size_t i = 0; i < rows(); i++)
		m_pRelation->printRow(stream, row(i).data(), separator);
	stream.precision(oldPrecision);
}

double WMatrix::measureInfo() const
{
	size_t c = cols();
	double dInfo = 0;
	for(size_t n = 0; n < c; n++)
	{
		if(m_pRelation->valueCount(n) == 0)
		{
			if(rows() > 1)
			{
				double m = columnMean(n);
				dInfo += columnVariance(n, m);
			}
		}
		else
			dInfo += entropy(n);
	}
	return dInfo;
}

double WMatrix::sumSquaredDiffWithIdentity()
{
	size_t m = std::min(rows(), (size_t)cols());
	double err = 0;
	double d;
	for(size_t i = 0; i < m; i++)
	{
		WVec& r = row(i);
		for(size_t j = 0; j < m; j++)
		{
			d = r[j];
			if(i == j)
				d -= 1;
			err += (d * d);
		}
	}
	return err;
}
/*
bool WMatrix::leastCorrelatedVector(double* pOut, WMatrix* pThat, WRand* pRand)
{
	if(rows() != pThat->rows() || cols() != pThat->cols())
		throw Ex("Expected matrices with the same dimensions");
	WMatrix* pC = WMatrix::multiply(*pThat, *this, false, true);
	std::unique_ptr<WMatrix> hC(pC);
	WMatrix* pE = WMatrix::multiply(*pC, *pC, true, false);
	std::unique_ptr<WMatrix> hE(pE);
	double d = pE->sumSquaredDiffWithIdentity();
	if(d < 0.001)
		return false;
	WMatrix* pF = pE->mostSignificantEigenVectors(rows(), pRand);
	std::unique_ptr<WMatrix> hF(pF);
	WVec::copy(pOut, pF->row(rows() - 1), rows());
	return true;
}
*/
bool WMatrix::leastCorrelatedVector(WVec& out, const WMatrix* pThat, WRand* pRand) const
{
	if(rows() != pThat->rows() || cols() != pThat->cols())
		throw Ex("Expected matrices with the same dimensions");
	WMatrix* pC = WMatrix::multiply(*pThat, *this, false, true);
	std::unique_ptr<WMatrix> hC(pC);
	WMatrix* pD = WMatrix::multiply(*pThat, *pC, true, false);
	std::unique_ptr<WMatrix> hD(pD);
	double d = pD->sumSquaredDifference(*this, true);
	if(d < 1e-9)
		return false;
	pD->subtract(this, true);
	pD->principalComponentAboutOrigin(out, pRand);
	return true;
	/*
	WMatrix* pE = WMatrix::multiply(*pD, *pD, true, false);
	std::unique_ptr<WMatrix> hE(pE);
	WMatrix* pF = pE->mostSignificantEigenVectors(1, pRand);
	std::unique_ptr<WMatrix> hF(pF);
	WVec::copy(out, pF->row(0), rows());
	return true;
*/
}

double WMatrix::dihedralCorrelation(const WMatrix* pThat, WRand* pRand) const
{
	size_t colCount = cols();
	if(rows() == 1)
		return std::abs(row(0).dotProduct(pThat->row(0)));
	WVec pBuf(rows());
	WVec pA(colCount);
	WVec pB(colCount);
	if(!leastCorrelatedVector(pBuf, pThat, pRand))
		return 1.0;
	multiply(pBuf, pA, true);
	if(!pThat->leastCorrelatedVector(pBuf, this, pRand))
		return 1.0;
	pThat->multiply(pBuf, pB, true);
	return std::abs(pA.correlation(pB));
}

WVec* WMatrix::swapRow(size_t i, WVec* pNewRow)
{
	WVec* pRow = m_rows[i];
	m_rows[i] = pNewRow;
	return pRow;
}

//static
WSimpleAssignment WMatrix::bipartiteMatching(WMatrix& a, WMatrix& b, WDistanceMetric& metric)
{
	if(a.cols() != b.cols())
		throw Ex("Expected input matrices to have the same number of cols");
	metric.init(&a.relation(), false);
	//WSimpleAssignment result(a.rows(), b.rows());
	WMatrix costs(a.rows(), b.rows());
	for(unsigned i = 0; i < a.rows(); ++i)
	{
		for(unsigned j = 0; j < b.rows(); ++j)
		{
			costs[i][j] = metric(a[i], b[j]);
		}
	}
	return linearAssignment(costs);
}


void WMatrix_testParsing()
{
	const char* file =
	  "\n"
	  "% This is a comment\n"
	  "\n"
	  "@RELATION rel\n"
	  "\n"
	  "@ATTRIBUTE 'attr 1' { 'y' , n } \n"
	  "@attribute attr_2 continuous\n"
	  "@Attribute attr3 REAL\n"
	  "\n"
	  "@data\n"
	  " 'y' , 3 , -1.5e-2 \n"
	  "n , 0.1, 1\n"
	  "\n"
	  " % Here is another comment\n"
	  "'y',,99 \n"
	  ",,\n"
	  "?,?,?";
	WMatrix m;
	m.parseArff(file, strlen(file));
	if(m.cols() != 3)
		throw Ex("failed");
	if(m.rows() != 5)
		throw Ex("failed");
	if(m[0][2] != -1.5e-2)
		throw Ex("failed");
	if(m[2][1] != UNKNOWN_REAL_VALUE)
		throw Ex("failed");
	if(m[3][1] != UNKNOWN_REAL_VALUE)
		throw Ex("failed");
	if(m[3][0] != UNKNOWN_DISCRETE_VALUE)
		throw Ex("failed");
	if(m[4][0] != UNKNOWN_DISCRETE_VALUE)
		throw Ex("failed");
}

/// This test does bipartite matching with a bunch of random matrices,
/// to make sure there are no crashes or endless loops. The results
/// are not checked since correct results are not known.  However, the
/// results are checked against WMatrix::bipartiteMatchingLAP and an
/// exception is thrown if the two give different results.
void WMatrix_testBipartiteMatching()
{
	WMatrix a(7, 2);
	a[0][0] = 0;
	a[0][1] = 0;
	a[1][0] = 2;
	a[1][1] = 1;
	a[2][0] = 5;
	a[2][1] = 2;
	a[3][0] = 3;
	a[3][1] = 5;
	a[4][0] = 4;
	a[4][1] = 7;
	a[5][0] = 1;
	a[5][1] = 4;
	a[6][0] = 4;
	a[6][1] = 2;
	WMatrix b(7, 2);
	b[0][0] = 4;
	b[0][1] = 2;
	b[1][0] = 2;
	b[1][1] = 1;
	b[2][0] = 5.01;
	b[2][1] = 2;
	b[3][0] = 6;
	b[3][1] = 2;
	b[4][0] = 3.9;
	b[4][1] = 6.9;
	b[5][0] = 3.1;
	b[5][1] = 5.1;
	b[6][0] = 2.9;
	b[6][1] = 4.9;

	WRowDistance metric;
	WSimpleAssignment results = WMatrix::bipartiteMatching(a, b, metric);
	if(results(0) != 1)
		throw Ex("failed");
	if(results(1) != 0)
		throw Ex("failed");
	if(results(2) != 3)
		throw Ex("failed");
	if(results(3) != 5)
		throw Ex("failed");
	if(results(4) != 4)
		throw Ex("failed");
	if(results(5) != 6)
		throw Ex("failed");
	if(results(6) != 2)
		throw Ex("failed");
}

void WMatrix_testMultiply()
{
	WMatrix a(2, 2);
	a[0][0] = 2;
	a[0][1] = 17;
	a[1][0] = 7;
	a[1][1] = 19;
	WMatrix b(2, 2);
	b[0][0] = 11;
	b[0][1] = 3;
	b[1][0] = 5;
	b[1][1] = 13;
	WMatrix* pC;
	pC = WMatrix::multiply(a, b, false, false);
	if(pC->rows() != 2 || pC->cols() != 2)
		throw Ex("wrong size");
	if(pC->row(0)[0] != 107 || pC->row(0)[1] != 227 || pC->row(1)[0] != 172 || pC->row(1)[1] != 268)
		throw Ex("wrong answer");
	delete(pC);
	WMatrix* pA = a.transpose();
	pC = WMatrix::multiply(*pA, b, true, false);
	if(pC->rows() != 2 || pC->cols() != 2)
		throw Ex("wrong size");
	if(pC->row(0)[0] != 107 || pC->row(0)[1] != 227 || pC->row(1)[0] != 172 || pC->row(1)[1] != 268)
		throw Ex("wrong answer");
	delete(pC);
	WMatrix* pB = b.transpose();
	pC = WMatrix::multiply(a, *pB, false, true);
	if(pC->rows() != 2 || pC->cols() != 2)
		throw Ex("wrong size");
	if(pC->row(0)[0] != 107 || pC->row(0)[1] != 227 || pC->row(1)[0] != 172 || pC->row(1)[1] != 268)
		throw Ex("wrong answer");
	delete(pC);
	pC = WMatrix::multiply(*pA, *pB, true, true);
	if(pC->rows() != 2 || pC->cols() != 2)
		throw Ex("wrong size");
	if(pC->row(0)[0] != 107 || pC->row(0)[1] != 227 || pC->row(1)[0] != 172 || pC->row(1)[1] != 268)
		throw Ex("wrong answer");
	delete(pC);
	delete(pA);
	delete(pB);
}

void WMatrix_testCholesky()
{
	WMatrix m1(3, 3);
	m1[0][0] = 3;
	m1[0][1] = 0;
	m1[0][2] = 0;
	m1[1][0] = 1;
	m1[1][1] = 4;
	m1[1][2] = 0;
	m1[2][0] = 2;
	m1[2][1] = 2;
	m1[2][2] = 7;
	WMatrix* pM3 = WMatrix::multiply(m1, m1, false, true);
	std::unique_ptr<WMatrix> hM3(pM3);
	WMatrix* pM4 = pM3->cholesky();
	std::unique_ptr<WMatrix> hM4(pM4);
	if(m1.sumSquaredDifference(*pM4, false) >= .0001)
		throw Ex("Cholesky decomposition didn't work right");
}

void WMatrix_testInvert()
{
	WMatrix i1(3, 3);
	i1[0][0] = 2;
	i1[0][1] = -1;
	i1[0][2] = 0;
	i1[1][0] = -1;
	i1[1][1] = 2;
	i1[1][2] = -1;
	i1[2][0] = 0;
	i1[2][1] = -1;
	i1[2][2] = 2;
	//	i1.invert();
	WMatrix* pInv = i1.pseudoInverse();
	std::unique_ptr<WMatrix> hInv(pInv);
	WMatrix i2(3, 3);
	i2[0][0] = .75;
	i2[0][1] = .5;
	i2[0][2] = .25;
	i2[1][0] = .5;
	i2[1][1] = 1;
	i2[1][2] = .5;
	i2[2][0] = .25;
	i2[2][1] = .5;
	i2[2][2] = .75;
	if(pInv->sumSquaredDifference(i2, false) >= .0001)
		throw Ex("Not good enough");
	//	i1.invert();
	WMatrix* pInvInv = pInv->pseudoInverse();
	std::unique_ptr<WMatrix> hInvInv(pInvInv);
	WMatrix* pI3 = WMatrix::multiply(*pInvInv, i2, false, false);
	std::unique_ptr<WMatrix> hI3(pI3);
	WMatrix i4(3, 3);
	i4.makeIdentity();
	if(pI3->sumSquaredDifference(i4, false) >= .0001)
		throw Ex("Not good enough");
}

void WMatrix_testDeterminant()
{
	const double dettest[] =
	  {
	    1,
	    2,
	    3,
	    4,
	    5,
	    6,
	    7,
	    8,
	    2,
	    6,
	    4,
	    8,
	    3,
	    1,
	    1,
	    2,
	};
	WMatrix d1(0, 4);
	d1.fromVector(dettest, 4);
	double det = d1.determinant();
	if(std::abs(det - 72.0) >= .0001)
		throw Ex("wrong");
	const double dettest2[] =
	  {
	    3,
	    2,
	    5,
	    7,
	};
	WMatrix d2(0, 2);
	d2.fromVector(dettest2, 2);
	det = d2.determinant();
	if(std::abs(det - 11.0) >= .0001)
		throw Ex("wrong");
	const double dettest3[] =
	  {
	    1,
	    2,
	    3,
	    4,
	    5,
	    6,
	    7,
	    8,
	    9,
	};
	WMatrix d3(0, 3);
	d3.fromVector(dettest3, 3);
	det = d3.determinant();
	if(std::abs(det - 0.0) >= .0001)
		throw Ex("wrong");
}

void WMatrix_testReducedRowEchelonForm()
{
	const double reducedrowechelonformtest[] =
	  {
	    1,
	    -1,
	    1,
	    0,
	    2,
	    2,
	    -2,
	    0,
	    2,
	    2,
	    -1,
	    1,
	    2,
	    -3,
	    1,
	    -2,
	    2,
	    1,
	    -3,
	    -1,
	};
	const double reducedrowechelonformanswer[] =
	  {
	    1,
	    -1,
	    0,
	    1,
	    1,
	    0,
	    0,
	    1,
	    -1,
	    1,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	};
	WMatrix r1(0, 5);
	r1.fromVector(reducedrowechelonformtest, 4);
	if(r1.toReducedRowEchelonForm() != 2)
		throw Ex("wrong answer");
	WMatrix r2(0, 5);
	r2.fromVector(reducedrowechelonformanswer, 4);
	if(r1.sumSquaredDifference(r2) > .001)
		throw Ex("wrong answer");
	const double reducedrowechelonformtest2[] =
	  {
	    -2,
	    -4,
	    4,
	    2,
	    -8,
	    0,
	    8,
	    4,
	    -12,
	};
	const double reducedrowechelonformanswer2[] =
	  {
	    1,
	    0,
	    -4.0 / 3,
	    0,
	    1,
	    -1.0 / 3,
	    0,
	    0,
	    0,
	};
	WMatrix r3(0, 3);
	r3.fromVector(reducedrowechelonformtest2, 3);
	if(r3.toReducedRowEchelonForm() != 2)
		throw Ex("wrong answer");
	WMatrix r4(0, 3);
	r4.fromVector(reducedrowechelonformanswer2, 3);
	if(r4.sumSquaredDifference(r3) > .001)
		throw Ex("wrong answer");
}

void WMatrix_testPrincipalComponents(WRand& prng)
{
	// Test principal components
	WHeap heap(1000);
	WMatrix data(0, 2);
	data.reserve(100);
	for(size_t i = 0; i < 100; i++)
	{
		WVec& newRow = data.newRow();
		newRow[0] = prng.uniform();
		newRow[1] = 2 * newRow[0];
	}
	WVec mean(2);
	mean[0] = data.columnMean(0);
	mean[1] = data.columnMean(1);
	WVec eig(2);
	data.principalComponent(eig, mean, &prng);
	if(std::abs(eig[0] * 2 - eig[1]) > .0001)
		throw Ex("incorrect value");

	// Compute principal components via eigenvectors of covariance matrix, and
	// make sure they're the same
	WMixedRelation rel;
	rel.addAttr(0);
	rel.addAttr(0);
	WMatrix* pM = data.covarianceMatrix();
	std::unique_ptr<WMatrix> hM(pM);
	WVec ev;
	WMatrix* pEigenVecs = pM->eigs(1, ev, &prng, true);
	std::unique_ptr<WMatrix> hEigenVecs(pEigenVecs);
	if(std::abs(pEigenVecs->row(0)[0] * pEigenVecs->row(0)[1] - eig[0] * eig[1]) > .0001)
		throw Ex("answers don't agree");

	// Test most significant eigenvector computation
	WMatrix e1(2, 2);
	e1[0][0] = 1;
	e1[0][1] = 1;
	e1[1][0] = 1;
	e1[1][1] = 4;
	WVec ev2(2);
	WMatrix* pE2 = e1.eigs(2, ev2, &prng, true);
	std::unique_ptr<WMatrix> hE2(pE2);
	if(std::abs(pE2->row(0)[0] * pE2->row(0)[0] + pE2->row(0)[1] * pE2->row(0)[1] - 1) > .0001)
		throw Ex("answer not normalized");
	if(std::abs(pE2->row(0)[0] * pE2->row(0)[1] - .27735) >= .0001)
		throw Ex("wrong answer");
	if(std::abs(pE2->row(1)[0] * pE2->row(1)[0] + pE2->row(1)[1] * pE2->row(1)[1] - 1) > .0001)
		throw Ex("answer not normalized");
	if(std::abs(pE2->row(1)[0] * pE2->row(1)[1] + .27735) >= .0001)
		throw Ex("wrong answer");

	// Test least significant eigenvector computation and gaussian ellimination
	WMatrix e3(2, 2);
	e3[0][0] = 9;
	e3[0][1] = 3;
	e3[1][0] = 3;
	e3[1][1] = 5;
	WMatrix* pE4 = e3.eigs(2, ev2, &prng, true);
	std::unique_ptr<WMatrix> hE4(pE4);
	WMatrix* pE5 = e3.eigs(2, ev2, &prng, false);
	std::unique_ptr<WMatrix> hE5(pE5);
	if(std::abs(std::abs(pE4->row(0)[0]) - std::abs(pE5->row(1)[0])) >= .0001)
		throw Ex("failed");
	if(std::abs(std::abs(pE4->row(0)[1]) - std::abs(pE5->row(1)[1])) >= .0001)
		throw Ex("failed");
	if(std::abs(std::abs(pE4->row(1)[0]) - std::abs(pE5->row(0)[0])) >= .0001)
		throw Ex("failed");
	if(std::abs(std::abs(pE4->row(1)[1]) - std::abs(pE5->row(0)[1])) >= .0001)
		throw Ex("failed");
}

void WMatrix_testDihedralCorrelation(WRand& prng)
{
	// Test dihedral angle computation
	for(size_t iter = 0; iter < 500; iter++)
	{
		// Make a random set of orthonormal basis vectors
		size_t dims = 5;
		WMatrix basis(dims, dims);
		for(size_t i = 0; i < dims; i++)
		{
			basis[i].fillNormal(prng);
			basis[i].normalize();
			for(size_t j = 0; j < i; j++)
			{
				basis[i].subtractComponent(basis[j]);
				basis[i].normalize();
			}
		}

		// Make two planes with a known dihedral angle
		double angle = prng.uniform() * 0.5 * M_PI;
		double angle2 = prng.uniform() * 2.0 * M_PI;
		WMatrix p1(2, dims);
		p1[0].fill(0.0);
		p1[0][0] = cos(angle2);
		p1[0][2] = sin(angle2);
		p1[1].fill(0.0);
		p1[1][0] = -sin(angle2);
		p1[1][2] = cos(angle2);
		WMatrix p2(2, dims);
		p2[0].fill(0.0);
		p2[0][0] = cos(angle);
		p2[0][1] = sin(angle);
		p2[1].fill(0.0);
		p2[1][2] = 1.0;

		// Transform the planes with the basis matrix
		WMatrix p3(2, dims);
		basis.multiply(p1[0], p3[0]);
		basis.multiply(p1[1], p3[1]);
		WMatrix p4(2, dims);
		basis.multiply(p2[0], p4[0]);
		basis.multiply(p2[1], p4[1]);

		// Measure the angle
		double actual = cos(angle);
		double measured = p3.dihedralCorrelation(&p4, &prng);
		if(std::abs(measured - actual) > 1e-8)
			throw Ex("failed");
	}

	// Measure the dihedral angle of two 3-hyperplanes in 5-space
	double angle = 0.54321;
	WMatrix bas(5, 5);
	bas.makeIdentity();
	bas[2][2] = cos(angle);
	bas[2][4] = sin(angle);
	bas[4][2] = -sin(angle);
	bas[4][4] = cos(angle);
	WMatrix sp1(3, 5);
	sp1.makeIdentity();
	WMatrix* sp3 = WMatrix::multiply(sp1, bas, false, true);
	std::unique_ptr<WMatrix> hSp3(sp3);
	double cosangle = sp1.dihedralCorrelation(sp3, &prng);
	double measured = acos(cosangle);
	if(std::abs(measured - angle) > 1e-8)
		throw Ex("failed");

	// Make sure dihedral angles are computed correctly with parallel planes
	static const double aa[] = {1.0, 0.0, 0.0, 0.0, -1.0, 0.0};
	static const double bb[] = {0.6, 0.8, 0.0, -0.8, 0.6, 0.0};
	WMatrix planeA(0, 3);
	planeA.fromVector(aa, 2);
	WMatrix planeB(0, 3);
	planeB.fromVector(bb, 2);
	cosangle = planeA.dihedralCorrelation(&planeB, &prng);
	if(std::abs(cosangle - 1.0) > 1e-8)
		throw Ex("failed");
	cosangle = planeB.dihedralCorrelation(&planeA, &prng);
	if(std::abs(cosangle - 1.0) > 1e-8)
		throw Ex("failed");
}

void WMatrix_testSingularValueDecomposition()
{
	WMatrix* pU;
	double* pDiag;
	WMatrix* pV;
	WMatrix M(2, 2);
	M[0][0] = 4.0;
	M[0][1] = 3.0;
	M[1][0] = 0.0;
	M[1][1] = -5.0;
	M.singularValueDecomposition(&pU, &pDiag, &pV);
	std::unique_ptr<WMatrix> hU(pU);
	std::unique_ptr<double[]> hDiag(pDiag);
	std::unique_ptr<WMatrix> hV(pV);

	// Test that the diagonal values are correct
	if(std::abs(pDiag[0] - sqrt(40.0)) > 1e-8)
		throw Ex("pDiag is not correct");
	if(std::abs(pDiag[1] - sqrt(10.0)) > 1e-8)
		throw Ex("pDiag is not correct");

	// Test that U is unitary
	WMatrix* pT1 = WMatrix::multiply(*pU, *pU, false, true);
	std::unique_ptr<WMatrix> hT1(pT1);
	if(pT1->sumSquaredDiffWithIdentity() > 1e-8)
		throw Ex("U is not unitary");

	// Test that V is unitary
	WMatrix* pT2 = WMatrix::multiply(*pV, *pV, false, true);
	std::unique_ptr<WMatrix> hT2(pT2);
	if(pT2->sumSquaredDiffWithIdentity() > 1e-8)
		throw Ex("V is not unitary");
}

void WMatrix_testPseudoInverse()
{
	{
		WMatrix M(2, 2);
		M[0][0] = 1.0;
		M[0][1] = 1.0;
		M[1][0] = 2.0;
		M[1][1] = 2.0;
		WMatrix* A = M.pseudoInverse();
		std::unique_ptr<WMatrix> hA(A);
		WMatrix B(2, 2);
		B[0][0] = 0.1;
		B[0][1] = 0.2;
		B[1][0] = 0.1;
		B[1][1] = 0.2;
		if(A->sumSquaredDifference(B, false) > 1e-8)
			throw Ex("failed");
	}
	{
		WMatrix M(3, 2);
		M[0][0] = 1.0;
		M[0][1] = 2.0;
		M[1][0] = 3.0;
		M[1][1] = 4.0;
		M[2][0] = 5.0;
		M[2][1] = 6.0;
		WMatrix* A = M.pseudoInverse();
		std::unique_ptr<WMatrix> hA(A);
		if(A->rows() != 2 || A->cols() != 3)
			throw Ex("wrong size");
		WMatrix B(2, 3);
		B[0][0] = -16.0 / 12.0;
		B[0][1] = -4.0 / 12.0;
		B[0][2] = 8.0 / 12.0;
		B[1][0] = 13.0 / 12.0;
		B[1][1] = 4.0 / 12.0;
		B[1][2] = -5.0 / 12.0;
		if(A->sumSquaredDifference(B, false) > 1e-8)
			throw Ex("failed");
	}
	{
		WMatrix M(2, 3);
		M[0][0] = 1.0;
		M[0][1] = 3.0;
		M[0][2] = 5.0;
		M[1][0] = 2.0;
		M[1][1] = 4.0;
		M[1][2] = 6.0;
		WMatrix* A = M.pseudoInverse();
		std::unique_ptr<WMatrix> hA(A);
		if(A->rows() != 3 || A->cols() != 2)
			throw Ex("wrong size");
		WMatrix B(3, 2);
		B[0][0] = -16.0 / 12.0;
		B[0][1] = 13.0 / 12.0;
		B[1][0] = -4.0 / 12.0;
		B[1][1] = 4.0 / 12.0;
		B[2][0] = 8.0 / 12.0;
		B[2][1] = -5.0 / 12.0;
		if(A->sumSquaredDifference(B, false) > 1e-8)
			throw Ex("failed");
	}
}

void WMatrix_testKabsch(WRand& prng)
{
	WMatrix a(20, 5);
	for(size_t i = 0; i < 20; i++)
	{
		a[i].fillNormal(prng);
		a[i].normalize();
		a[i] *= (prng.uniform() + 0.5);
	}
	WMatrix rot(5, 5);
	static const double rr[] = {
	  0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.8, 0.0, -0.6, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.0, 0.8};
	rot.fromVector(rr, 5);
	WMatrix* pB = WMatrix::multiply(a, rot, false, false);
	std::unique_ptr<WMatrix> hB(pB);
	WMatrix* pK = WMatrix::kabsch(&a, pB);
	std::unique_ptr<WMatrix> hK(pK);
	if(pK->sumSquaredDifference(rot, true) > 1e-6)
		throw Ex("Failed to recover rotation matrix");
	WMatrix* pC = WMatrix::multiply(*pB, *pK, false, false);
	std::unique_ptr<WMatrix> hC(pC);
	if(a.sumSquaredDifference(*pC, false) > 1e-6)
		throw Ex("Failed to align data");
}

void WMatrix_testLUDecomposition(WRand& prng)
{
	WMatrix a(5, 5);
	for(size_t i = 0; i < 5; i++)
	{
		for(size_t j = 0; j < 5; j++)
			a[i][j] = prng.normal();
	}
	WMatrix b(a);
	b.LUDecomposition();
	WMatrix l(5, 5);
	l.fill(0.0);
	WMatrix u(5, 5);
	u.fill(0.0);
	for(size_t i = 0; i < 5; i++)
	{
		for(size_t j = 0; j < 5; j++)
		{
			if(i < j)
				u[i][j] = b[i][j];
			else
				l[i][j] = b[i][j];
		}
		u[i][i] = 1.0;
	}
	WMatrix* pProd = WMatrix::multiply(l, u, false, false);
	std::unique_ptr<WMatrix> hProd(pProd);
	if(pProd->sumSquaredDifference(a, false) > 0.00001)
		throw Ex("failed");
}

void WMatrix_testWilcoxon()
{
	// These values were copied from the Wikipedia page about the Wilcoxon signed ranks test
	WMatrix m(10, 2);
	m[0][0] = 125;
	m[0][1] = 110;
	m[1][0] = 115;
	m[1][1] = 122;
	m[2][0] = 130;
	m[2][1] = 125;
	m[3][0] = 140;
	m[3][1] = 120;
	m[4][0] = 140;
	m[4][1] = 140;
	m[5][0] = 115;
	m[5][1] = 124;
	m[6][0] = 140;
	m[6][1] = 123;
	m[7][0] = 125;
	m[7][1] = 137;
	m[8][0] = 140;
	m[8][1] = 135;
	m[9][0] = 135;
	m[9][1] = 145;
	int n;
	double min, plu;
	m.wilcoxonSignedRanksTest(0, 1, 0.0, &n, &min, &plu);
	if(plu != 27)
		throw Ex("incorrect test statistic");
	if(min != 18)
		throw Ex("incorrect test statistic");
}

void WMatrix_testBoundingSphere(WRand& rand)
{
	WVec center(10);
	for(size_t i = 0; i < 30; i++)
	{
		size_t points = (size_t)rand.next(48) + 2;
		size_t dims = (size_t)rand.next(8) + 2;
		WMatrix m(points, dims);
		vector<size_t> indexes;
		indexes.reserve(points);
		for(size_t j = 0; j < points; j++)
		{
			indexes.push_back(j);
			m[j].fillNormal(rand);
			m[j].normalize();
		}
		WRowDistance metric;
		metric.init(&m.relation(), false);
		double r2 = m.boundingSphere(center, indexes.data(), indexes.size(), &metric);
		if(sqrt(r2) > 1.1)
			throw Ex("radius ", to_str(sqrt(r2)), " too large");
		for(size_t j = 0; j < points; j++)
		{
			if(center.squaredDistance(m[j]) > r2)
				throw Ex("point not bounded");
		}
	}
}

void WMatrix_testImport()
{
	const char* csv = "3.3, 1.2, 3.5, 0\n"
	                  "2.1, 5.6, 3.6, 1\n"
	                  "2.0, 0.4, 0.7, 0";
	WCSVParser parser;
	WMatrix m;
	parser.parse(m, csv, strlen(csv));
	if(m[1][2] != 3.6)
		throw Ex("failed");
}

// static
void WMatrix::test()
{
	WRand prng(0);
	WMatrix_testMultiply();
	WMatrix_testCholesky();
	WMatrix_testInvert();
	WMatrix_testDeterminant();
	WMatrix_testReducedRowEchelonForm();
	WMatrix_testPrincipalComponents(prng);
	WMatrix_testDihedralCorrelation(prng);
	WMatrix_testSingularValueDecomposition();
	WMatrix_testPseudoInverse();
	WMatrix_testKabsch(prng);
	WMatrix_testLUDecomposition(prng);
	WMatrix_testBipartiteMatching();
	WMatrix_testParsing();
	WMatrix_testWilcoxon();
	WMatrix_testBoundingSphere(prng);
	WMatrix_testImport();
}

std::string to_str(const WMatrix& m)
{
	std::stringstream out;
	out.precision(14);
	if(!m.relation().areContinuous())
	{
		out << '[';
		for(unsigned j = 0; j < m.cols(); ++j)
		{
			if(j != 0) { out << ", "; }
			if(m.relation().areContinuous(j, 1))
			{
				out << "continuous";
			}
			else
			{
				out << "discrete:" << m.relation().valueCount(j);
			}
		}
		out << "]\n";
	}
	else
	{
		out << "Continuous matrix:\n";
	}

	out << '[';
	for(unsigned i = 0; i < m.rows(); ++i)
	{
		for(unsigned j = 0; j < m.cols(); ++j)
		{
			if(j != 0) { out << ", "; }
			out << m[i][j];
		}
		if(i + 1 < m.rows())
		{
			out << "\n";
		}
	}
	out << "]\n";
	return out.str();
}




WDataColSplitter::WDataColSplitter(const WMatrix& data, size_t labelCount)
{
	m_pFeatures = new WMatrix();
	m_pFeatures->copyCols(data, 0, data.cols() - labelCount);
	m_pLabels = new WMatrix();
	m_pLabels->copyCols(data, data.cols() - labelCount, labelCount);
}

WDataColSplitter::~WDataColSplitter()
{
	delete(m_pFeatures);
	delete(m_pLabels);
}










WCSVParser::WCSVParser()
: m_single_quotes(false),
  m_separator(','),
  m_columnNamesInFirstRow(false),
  m_tolerant(false),
  m_clearlyNumericalThreshold(10),
  m_maxVals(200)
{
}

WCSVParser::~WCSVParser()
{
}

void WCSVParser::setTimeFormat(size_t attr, const char* szFormat)
{
	m_formats.insert(std::pair<size_t, string>(attr, szFormat));
}

void WCSVParser::setNominalAttr(size_t attr)
{
	m_specifiedNominal.insert(std::pair<size_t, size_t>(attr, 0));
}

void WCSVParser::setRealAttr(size_t attr)
{
	m_specifiedReal.insert(std::pair<size_t, size_t>(attr, 0));
}

void WCSVParser::setStripQuotes(size_t attr)
{
	m_stripQuotes.insert(std::pair<size_t, size_t>(attr, 0));
}


class ImportRow
{
public:
	vector<const char*> m_elements;
};

void WCSVParser::parse(WMatrix& outMatrix, const char* szFilename)
{
	size_t nLen;
	char* szFile = WFile::loadFile(szFilename, &nLen);
	std::unique_ptr<char[]> hFile(szFile);
	if(nLen < 1)
		throw Ex("Empty file");
	parse(outMatrix, szFile, nLen);
}

void WCSVParser::parse(WMatrix& outMatrix, const char* pFile, size_t len)
{
	// Extract the elements
	WHeap heap(2048);
	vector<ImportRow> rows;
	size_t columnCount = INVALID_INDEX;
	size_t nFirstDataLine = 1;
	size_t nLine = 1;
	size_t nPos = 0;
	while(true)
	{
		// Skip Whitespace
		while(nPos < len && pFile[nPos] <= ' ' && pFile[nPos] != m_separator)
		{
			if(pFile[nPos] == '\n')
				nLine++;
			nPos++;
		}
		if(nPos >= len)
			break;

		// Count the elements
		if(columnCount == INVALID_INDEX && (!m_columnNamesInFirstRow || nLine > 1))
		{
			if(m_separator == '\0')
			{
				// Elements are separated by an arbitrary amount of whitespace, element values contain no whitespace, and there are no missing elements
				size_t i = nPos;
				columnCount = 0;
				while(true)
				{
					columnCount++;
					while(i < len && pFile[i] > ' ')
						i++;
					while(i < len && pFile[i] <= ' ' && pFile[i] != '\n')
						i++;
					if(pFile[i] == '\n')
						break;
				}
			}
			else
			{
				// Elements are separated by the specified character
				nFirstDataLine = nLine;
				columnCount = 1;
				bool quo = false;
				bool quoquo = false;
				for(size_t i = 0; pFile[nPos + i] != '\n' && pFile[nPos + i] != '\0'; i++)
				{
					if(quoquo)
					{
						if(pFile[nPos + i] == '"')
							quoquo = false;
					}
					else if(quo)
					{
						if(pFile[nPos + i] == '\'')
							quo = false;
					}
					else if(pFile[nPos + i] == '"')
						quoquo = true;
					else if(pFile[nPos + i] == '\'' && m_single_quotes)
						quo = true;
					else if(pFile[nPos + i] == m_separator)
						columnCount++;
				}
			}
		}

		// Extract the elements from the row
		rows.resize(rows.size() + 1);
		ImportRow& row = rows[rows.size() - 1];
		while(true)
		{
			// Skip Whitespace
			while(nPos < len && pFile[nPos] <= ' ' && pFile[nPos] != m_separator)
			{
				if(pFile[nPos] == '\n')
					break;
				nPos++;
			}

			// Extract the element
			size_t i, l;
			if(m_separator == '\0')
			{
				for(l = 0; pFile[nPos + l] > ' '; l++)
				{
				}
				for(i = l; pFile[nPos + i] <= ' ' && pFile[nPos + i] != '\n' && pFile[nPos + i] != '\0'; i++)
				{
				}
			}
			else
			{
				bool quo = false;
				bool quoquo = false;
				for(i = 0; pFile[nPos + i] != '\n' && pFile[nPos + i] != '\0'; i++)
				{
					if(quoquo)
					{
						if(pFile[nPos + i] == '"')
							quoquo = false;
					}
					else if(quo)
					{
						if(pFile[nPos + i] == '\'')
							quo = false;
					}
					else if(pFile[nPos + i] == '"')
						quoquo = true;
					else if(pFile[nPos + i] == '\'' && m_single_quotes)
						quo = true;
					else if(pFile[nPos + i] == m_separator)
						break;
				}
				if(quo)
					throw Ex("Line ", to_str(nLine), " contains an unmatched apostrophe.");
				if(quoquo)
					throw Ex("Line ", to_str(nLine), " contains unmatched quotation marks.");
				for(l = i; l > 0 && pFile[nPos + l - 1] <= ' '; l--)
				{
				}
			}
			WAssert(pFile[nPos] > ' ' || l == 0);
			WAssert(pFile[nPos] != m_separator || l == 0);
			WAssert(pFile[nPos + l - 1] > ' ' || l == 0);
			WAssert(pFile[nPos + l - 1] != m_separator || l == 0);
			std::map<size_t, size_t>::iterator itStripQuotes = m_stripQuotes.find(row.m_elements.size());
			if(itStripQuotes != m_stripQuotes.end())
			{
				if(pFile[nPos] == '"' && pFile[nPos + l - 1] == '"')
				{
					nPos++;
					i--;
					l -= 2;
				}
				if(pFile[nPos] == '\'' && pFile[nPos + l - 1] == '\'')
				{
					nPos++;
					i--;
					l -= 2;
				}

				// Strip any bounding whitespace inside the quotes
				while(l > 0 && pFile[nPos + l - 1] == ' ')
					l--;
				while(l > 0 && pFile[nPos] == ' ')
				{
					nPos++;
					i--;
					l--;
				}
			}
			WAssert(pFile[nPos] > ' ' || l == 0);
			WAssert(pFile[nPos] != m_separator || l == 0);
			WAssert(pFile[nPos + l - 1] > ' ' || l == 0);
			WAssert(pFile[nPos + l - 1] != m_separator || l == 0);
			char* el = heap.add(pFile + nPos, l);

			// Replace any unquoted separator chars with '_'
			bool quo = false;
			bool quoquo = false;
			for(size_t k = 0; el[k] != '\0'; k++)
			{
				if(quo)
				{
					if(el[k] == '\'')
						quo = false;
				}
				else if(quoquo)
				{
					if(el[k] == '"')
						quoquo = false;
				}
				else if(el[k] == '\'' && m_single_quotes)
					quo = true;
				else if(el[k] == '"')
					quoquo = true;
				else if(el[k] == m_separator)
					el[k] = '_';
			}

			row.m_elements.push_back(el);
			if(row.m_elements.size() > columnCount)
				break;
			nPos += i;
			if(nPos >= len || pFile[nPos] == '\n')
				break;
			if(m_separator != '\0' && pFile[nPos] == m_separator)
				nPos++;
		}
		if(m_tolerant)
		{
			if(!m_columnNamesInFirstRow || nLine > 1)
			{
				while(row.m_elements.size() < columnCount)
					row.m_elements.push_back("?");
			}
		}
		else
		{
			if(row.m_elements.size() != (size_t)columnCount && columnCount != INVALID_INDEX)
				throw Ex("Line ", to_str(nLine), " has a different number of elements than line ", to_str(nFirstDataLine));
		}

		// Move to next line
		for(; nPos < len && pFile[nPos] != '\n'; nPos++)
		{
		}
		continue;
	}
	if(m_columnNamesInFirstRow && m_tolerant)
	{
		ImportRow& row = rows[0];
		while(row.m_elements.size() < columnCount)
			row.m_elements.push_back("attr");
	}

	// Parse it all
	size_t rowCount = rows.size() - (m_columnNamesInFirstRow ? 1 : 0);
	outMatrix.flush();
	WArffRelation* pRelation = new WArffRelation();
	outMatrix.setRelation(pRelation);
	for(size_t i = 0; i < rowCount; i++)
	{
		WVec* pNewVec = new WVec();
		outMatrix.takeRow(pNewVec);
		pNewVec->resize(columnCount);
	}
	m_report.resize(columnCount);
	for(size_t attr = 0; attr < columnCount; attr++)
	{
		std::map<size_t, string>::iterator itFormat = m_formats.find(attr);
		if(itFormat != m_formats.end())
		{
			const char* szFormat = itFormat->second.c_str();
			if(m_columnNamesInFirstRow)
			{
				bool quot = false;
				if(rows[0].m_elements[attr][0] != '"' && rows[0].m_elements[attr][0] != '\'')
					quot = true;
				string attrName = "";
				if(quot)
					attrName += '\"';
				attrName += rows[0].m_elements[attr];
				if(quot)
					attrName += '\"';
				pRelation->addAttribute(attrName.c_str(), 0, nullptr);
			}
			else
			{
				string attrName = "attr";
				attrName += to_str(attr);
				pRelation->addAttribute(attrName.c_str(), 0, nullptr);
			}

			size_t i = 0;
			size_t errs = 0;
			string firstErr;
			for(size_t rowNum = m_columnNamesInFirstRow ? 1 : 0; rowNum < rows.size(); rowNum++)
			{
				const char* el = rows[rowNum].m_elements[attr];
				double t;
				if(*el == '\0')
					outMatrix[i][attr] = UNKNOWN_REAL_VALUE;
				else if(WTime::fromString(&t, el, szFormat))
					outMatrix[i][attr] = t;
				else
				{
					outMatrix[i][attr] = UNKNOWN_REAL_VALUE;
					if(errs == 0)
						firstErr = el;
					errs++;
				}
				i++;
			}

			if(m_columnNamesInFirstRow)
			{
				m_report[attr] = rows[0].m_elements[attr];
				m_report[attr] += ": ";
			}
			else
				m_report[attr] = "";
			m_report[attr] += "Formatted date \"";
			m_report[attr] += szFormat;
			m_report[attr] += "\". ";
			m_report[attr] += to_str(errs);
			m_report[attr] += " errors";
			if(errs > 0)
			{
				m_report[attr] += ", such as \"";
				m_report[attr] += firstErr;
				m_report[attr] += "\".";
				string tmp = m_report[attr];
				m_report[attr] = "ERROR   ";
				m_report[attr] += tmp;
			}
			else
			{
				string tmp = m_report[attr];
				m_report[attr] = "OK      ";
				m_report[attr] += tmp;
			}
			continue;
		}

		// Determine if the attribute can be real
		bool real = true;
		bool specified = false;
		string firstNonNumericalValue;
		std::map<size_t, size_t>::iterator itSpecifiedReal = m_specifiedReal.find(attr);
		std::map<size_t, size_t>::iterator itSpecifiedNominal = m_specifiedNominal.find(attr);
		if(itSpecifiedReal != m_specifiedReal.end())
		{
			real = true;
			specified = true;
		}
		else if(itSpecifiedNominal != m_specifiedNominal.end())
		{
			real = false;
			specified = true;
		}
		else
		{
			for(size_t rowNum = m_columnNamesInFirstRow ? 1 : 0; rowNum < rows.size(); rowNum++)
			{
				const char* el = rows[rowNum].m_elements[attr];
				if(el[0] == '\0')
					continue; // unknown value
				if(strcmp(el, "?") == 0)
					continue; // unknown value
				if(WBits::isValidFloat(el, strlen(el)))
					continue;
				firstNonNumericalValue = el;
				real = false;
				break;
			}
		}

		// Make the attribute
		if(real)
		{
			string firstRealError = "";
			size_t realErrs = 0;
			if(m_columnNamesInFirstRow)
			{
				bool quot = false;
				if(rows[0].m_elements[attr][0] != '"' && rows[0].m_elements[attr][0] != '\'')
					quot = true;
				string attrName = "";
				if(quot)
					attrName += '\"';
				attrName += rows[0].m_elements[attr];
				if(quot)
					attrName += '\"';
				pRelation->addAttribute(attrName.c_str(), 0, nullptr);
			}
			else
			{
				string attrName = "attr";
				attrName += to_str(attr);
				pRelation->addAttribute(attrName.c_str(), 0, nullptr);
			}
			size_t i = 0;
			for(size_t rowNum = m_columnNamesInFirstRow ? 1 : 0; rowNum < rows.size(); rowNum++)
			{
				const char* el = rows[rowNum].m_elements[attr];
				double val;
				if(el[0] == '\0' || strcmp(el, "?") == 0) // missing
					val = UNKNOWN_REAL_VALUE;
				else if(!WBits::isValidFloat(el, strlen(el)))
				{
					val = UNKNOWN_REAL_VALUE;
					if(firstRealError.length() < 1)
						firstRealError = el;
					realErrs++;
				}
				else if(!parseNumber(*el == '+' ? el + 1 : el, &val)) // (isValidFloat allows a leading '+')
					throw Ex("Invalid real value: ", el);
				outMatrix[i][attr] = val;
				i++;
			}

			// Report this column
			if(m_columnNamesInFirstRow)
			{
				m_report[attr] = rows[0].m_elements[attr];
				m_report[attr] += ": ";
			}
			else
				m_report[attr] = "";
			size_t uniqueVals = outMatrix.countUniqueValues(attr, m_clearlyNumericalThreshold);
			if(itSpecifiedReal != m_specifiedReal.end())
			{
				m_report[attr] += "Specified to be real. ";
				m_report[attr] += to_str(realErrs);
				m_report[attr] += " errors";
				if(realErrs > 0)
				{
					m_report[attr] += ", such as \"";
					m_report[attr] += firstRealError;
					m_report[attr] += '\"';
					string tmp = m_report[attr];
					m_report[attr] = "ERROR   ";
					m_report[attr] += tmp;
				}
				else
				{
					string tmp = m_report[attr];
					m_report[attr] = "OK      ";
					m_report[attr] += tmp;
				}
				m_report[attr] += '.';
			}
			else if(uniqueVals < m_clearlyNumericalThreshold)
			{
				m_report[attr] += "Ambiguous type. All values in this column are numerical, but there are only ";
				m_report[attr] += to_str(uniqueVals);
				m_report[attr] += " unique values. Assuming a numerical attribute was intended.";
				string tmp = m_report[attr];
				m_report[attr] = "WARNING ";
				m_report[attr] += tmp;
			}
			else
			{
				m_report[attr] += "Clearly numerical.";
				string tmp = m_report[attr];
				m_report[attr] = "OK      ";
				m_report[attr] += tmp;
			}
		}
		else
		{
			// It's categorical
			vector<const char*> values;
			WConstStringHashTable ht(31, true);
			void* pVal;
			uintptr_t n;
			size_t i = 0;
			size_t valueCount = 0;
			for(size_t rowNum = m_columnNamesInFirstRow ? 1 : 0; rowNum < rows.size(); rowNum++)
			{
				const char* el = rows[rowNum].m_elements[attr];
				if(el[0] == '\0' || strcmp(el, "?") == 0) // missing
					outMatrix[i][attr] = UNKNOWN_DISCRETE_VALUE;
				else
				{
					if(specified || valueCount <= m_maxVals)
					{
						if(ht.get(el, &pVal))
							n = (uintptr_t)pVal;
						else
						{
							WAssert(el[0] > ' ');
							WAssert(el[0] != m_separator);
							WAssert(el[strlen(el) - 1] > ' ');
							WAssert(el[strlen(el) - 1] != m_separator);
							values.push_back(el);
							n = valueCount++;
							ht.add(el, (const void*)n);
						}
						outMatrix[i][attr] = (double)n;
					}
					else
						outMatrix[i][attr] = UNKNOWN_DISCRETE_VALUE;
				}
				i++;
			}

			// Make the attribute
			if(m_columnNamesInFirstRow)
			{
				m_report[attr] = rows[0].m_elements[attr];
				m_report[attr] += ": ";
			}
			else
				m_report[attr] = "";

			if(specified)
			{
				m_report[attr] += "Specified to be categorical. ";
				m_report[attr] += to_str(valueCount);
				m_report[attr] += " unique values. (";
				m_report[attr] += to_str((double)valueCount * 100.0 / rows.size());
				m_report[attr] += "% unique.)";
				string tmp = m_report[attr];
				if((double)valueCount / rows.size() < 0.8)
					m_report[attr] = "OK      ";
				else
					m_report[attr] = "WARNING ";
				m_report[attr] += tmp;
			}
			else if(valueCount <= m_maxVals)
			{
				m_report[attr] += "Clearly categorical. ";
				m_report[attr] += to_str(valueCount);
				m_report[attr] += " unique values. (";
				m_report[attr] += to_str((double)valueCount * 100.0 / rows.size());
				m_report[attr] += "% unique.)";
				string tmp = m_report[attr];
				m_report[attr] = "OK      ";
				m_report[attr] += tmp;
			}
			else
			{
				m_report[attr] += "Problematic column!!! Contains non-numerical values, such as \"";
				m_report[attr] += firstNonNumericalValue;
				m_report[attr] += "\", but contains more than ";
				m_report[attr] += to_str(m_maxVals);
				m_report[attr] += " unique values. Parsing of this column was aborted!!!";
				string tmp = m_report[attr];
				m_report[attr] = "ERROR   ";
				m_report[attr] += tmp;
			}
			if(m_columnNamesInFirstRow)
			{
				bool quot = false;
				if(rows[0].m_elements[attr][0] != '"' && rows[0].m_elements[attr][0] != '\'')
					quot = true;
				string attrName = "";
				if(quot)
					attrName += '\"';
				attrName += rows[0].m_elements[attr];
				if(quot)
					attrName += '\"';
				if(!specified && valueCount > m_maxVals)
				{
					attrName += "_aborted_due_to_too_many_vals";
					pRelation->addAttribute(attrName.c_str(), valueCount, &values);
				}
				else
					pRelation->addAttribute(attrName.c_str(), valueCount, &values);
			}
			else
			{
				string attrName = "attr";
				attrName += to_str(attr);
				if(!specified && valueCount > m_maxVals)
					attrName += "_aborted_due_to_too_many_vals";
				pRelation->addAttribute(attrName.c_str(), valueCount, &values);
			}
		}
	}
}






WDataRowSplitter::WDataRowSplitter(const WMatrix& features, const WMatrix& labels, WRand& rand, size_t part1Rows)
: m_f1(features.relation().cloneMinimal()),
  m_f2(features.relation().cloneMinimal()),
  m_l1(labels.relation().cloneMinimal()),
  m_l2(labels.relation().cloneMinimal())
{
	if(features.rows() != labels.rows())
		throw Ex("Expected features and labels to have the same number of rows");
	if(part1Rows > features.rows())
		throw Ex("part1Rows out of range");
	size_t part2Rows = features.rows() - part1Rows;
	for(size_t i = 0; i < features.rows(); i++)
	{
		if(rand.next(part1Rows + part2Rows) < part1Rows)
		{
			m_f1.takeRow((WVec*)&features[i]);
			m_l1.takeRow((WVec*)&labels[i]);
			part1Rows--;
		}
		else
		{
			m_f2.takeRow((WVec*)&features[i]);
			m_l2.takeRow((WVec*)&labels[i]);
			part2Rows--;
		}
	}
}

WDataRowSplitter::~WDataRowSplitter()
{
	m_f1.releaseAllRows();
	m_l1.releaseAllRows();
	m_f2.releaseAllRows();
	m_l2.releaseAllRows();
}







WRaggedMatrix::WRaggedMatrix()
: m_minCols(0), m_maxCols(0)
{
}

WRaggedMatrix::~WRaggedMatrix()
{
	flush();
}

void WRaggedMatrix::flush()
{
	for(size_t i = 0; i < rows(); i++)
		delete(m_rows[i]);
	m_rows.clear();
}

WVec& WRaggedMatrix::newRow(size_t size)
{
	WVec* pNewRow = new WVec(size);
	m_rows.push_back(pNewRow);
	return *pNewRow;
}

void WRaggedMatrix::parseCSV(WTokenizer& tok)
{
	m_minCols = (size_t)-1;
	m_maxCols = 0;
	WCharSet c_whitespace("\t\n\r ");
	WCharSet c_newline("\n");
	WCharSet c_commaNewlineTab(",\n\t");
	WArffRelation rel;
	rel.addAttribute("", 0, nullptr);
	flush();
	vector<double> vals;
	while(true)
	{
		vals.clear();
		tok.skipWhile(c_whitespace);
		char c = tok.peek();
		if(c == '\0')
			break;
		else if(c == '%')
		{
			tok.skip(1);
			tok.skipUntil(c_newline);
		}
		else
		{
			while(true)
			{
				tok.readUntil_escaped_quoted(c_commaNewlineTab);
				const char* szVal = tok.trim(c_whitespace);
				vals.push_back(WMatrix_parseValue(&rel, 0, szVal, tok));
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
					tok.skipUntil(c_newline);
					break;
				}
			}
			WVec& r = newRow(vals.size());
			m_minCols = std::min(m_minCols, vals.size());
			m_maxCols = std::max(m_maxCols, vals.size());
			for(size_t i = 0; i < vals.size(); i++)
				r[i] = vals[i];
		}
	}
}

void WRaggedMatrix::loadCSV(const char* szFilename)
{
	WTokenizer tok(szFilename);
	parseCSV(tok);
}




} // namespace WClasses
