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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "WDom.h"
#include "WFile.h"
#include "WHolders.h"
#include <vector>
#include <deque>
#include <sstream>
#include <fstream>
#include <map>
#include <errno.h>
#include "WTokenizer.h"
#include "WString.h"


namespace WClasses
{

using std::vector;
using std::deque;
using std::map;
using std::string;


class WDomObjField
{
public:
	const char* m_pName;
	WDomNode* m_pValue;
	WDomObjField* m_pPrev;
};

/// An element in a WDom list
class WDomListItem
{
public:
	/// Pointer to the value contained in this list item
	WDomNode* m_pValue;

	/// Pointer to the previous node in the list
	WDomListItem* m_pPrev;
};

class WDomArrayList
{
public:
	/// Total number of elements in this list
	size_t m_size;
	size_t m_capacity;

	/// The items in the list
	WDomNode* m_items[2]; // 2 is a bogus value
};



WDomListIterator::WDomListIterator(const WDomNode* pNode)
{
	if(pNode->m_type != WDomNode::type_list)
		throw Ex(to_str_brief(*pNode), " is not a list type");
	m_pList = pNode;
	m_index = 0;
}

WDomListIterator::~WDomListIterator()
{
}

WDomNode* WDomListIterator::current()
{
	if(!m_pList->m_value.m_pArrayList)
		return nullptr;
	if(m_index < m_pList->m_value.m_pArrayList->m_size)
		return m_pList->m_value.m_pArrayList->m_items[m_index];
	else
		return nullptr;
}

bool WDomListIterator::currentBool()
{
	return current()->asBool();
}

long long WDomListIterator::currentInt()
{
	return current()->asInt();
}

double WDomListIterator::currentDouble()
{
	return current()->asDouble();
}

const char* WDomListIterator::currentString()
{
	return current()->asString();
}

void WDomListIterator::advance()
{
	m_index++;
}

size_t WDomListIterator::remaining()
{
	if(!m_pList->m_value.m_pArrayList)
		return 0;
	return m_pList->m_value.m_pArrayList->m_size - m_index;
}


WDomNode* WDomNode::getIfExists(const char* szName) const
{
	if(m_type != type_obj)
		throw Ex(to_str_brief(*this), " is not an obj");
	WDomObjField* pField;
	for(pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
	{
		if(strcmp(szName, pField->m_pName) == 0)
			return pField->m_pValue;
	}
	return nullptr;
}

size_t WDomNode::reverseFieldOrder() const
{
	WAssert(m_type == type_obj);
	size_t count = 0;
	WDomObjField* pNewHead = nullptr;
	while(m_value.m_pLastField)
	{
		WDomObjField* pTemp = m_value.m_pLastField;
		((WDomNode*)this)->m_value.m_pLastField = pTemp->m_pPrev;
		pTemp->m_pPrev = pNewHead;
		pNewHead = pTemp;
		count++;
	}
	((WDomNode*)this)->m_value.m_pLastField = pNewHead;
	return count;
}

size_t WDomNode::size() const
{
	WAssert(m_type == type_list);
	if(!m_value.m_pArrayList)
		return 0;
	return m_value.m_pArrayList->m_size;
}

WDomNode* WDomNode::get(size_t index) const
{
	WAssert(m_type == type_list);
	WAssert(index < m_value.m_pArrayList->m_size);
	return m_value.m_pArrayList->m_items[index];
}

WDomNode* WDomNode::set(WDom* pDoc, const char* szName, WDomNode* pNode)
{
	if(m_type != type_obj)
		throw Ex(to_str_brief(*this), " is not an obj");
	WDomObjField* pField;
	for(pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
	{
		if(strcmp(szName, pField->m_pName) == 0)
		{
			pField->m_pValue = pNode;
			return pNode;
		}
	}
	pField = pDoc->newField();
	pField->m_pPrev = m_value.m_pLastField;
	m_value.m_pLastField = pField;
	WHeap* pHeap = pDoc->heap();
	pField->m_pName = pHeap->add(szName);
	pField->m_pValue = pNode;
	return pNode;
}

WDomNode* WDomNode::set(WDom* pDoc, size_t index, WDomNode* pNode)
{
	if(m_type != type_list)
		throw Ex(to_str_brief(*this), " is not a list");
	// SAKBox: size(), not m_pArrayList->m_size: a new list has no array yet,
	// so setting its first element dereferenced null
	if(index == size())
		return add(pDoc, pNode);
	else if(index < size())
	{
		m_value.m_pArrayList->m_items[index] = pNode;
		return pNode;
	}
	else
		throw Ex("tried to set index ", to_str(index), " in a list of size ", to_str(size()));
}

WDomNode* WDomNode::add(WDom* pDoc, const char* szName, WDomNode* pNode)
{
	if(m_type != type_obj)
		throw Ex(to_str_brief(*this), " is not an obj");
	WDomObjField* pField = pDoc->newField();
	pField->m_pPrev = m_value.m_pLastField;
	m_value.m_pLastField = pField;
	WHeap* pHeap = pDoc->heap();
	pField->m_pName = pHeap->add(szName);
	pField->m_pValue = pNode;
	return pNode;
}

WDomNode* WDomNode::add(WDom* pDoc, const char* szName, bool b)
{
	return add(pDoc, szName, pDoc->newBool(b));
}

WDomNode* WDomNode::add(WDom* pDoc, const char* szName, long long n)
{
	return add(pDoc, szName, pDoc->newInt(n));
}

WDomNode* WDomNode::add(WDom* pDoc, const char* szName, size_t n)
{
	return add(pDoc, szName, pDoc->newInt((long long)n));
}

WDomNode* WDomNode::add(WDom* pDoc, const char* szName, double d)
{
	return add(pDoc, szName, pDoc->newDouble(d));
}

WDomNode* WDomNode::add(WDom* pDoc, const char* szName, const char* str)
{
	return add(pDoc, szName, pDoc->newString(str));
}

WDomNode* WDomNode::add(WDom* pDoc, WDomNode* pNode)
{
	if(m_type != type_list)
		throw Ex(to_str_brief(*this), " is not a list");
	if(!m_value.m_pArrayList || m_value.m_pArrayList->m_size >= m_value.m_pArrayList->m_capacity)
	{
		// Reallocate the array of node pointers
		size_t newCapacity = std::max((size_t)4, (m_value.m_pArrayList ? m_value.m_pArrayList->m_size * 2 : 0));
		WDomArrayList* pArrayList = (WDomArrayList*)pDoc->m_heap.allocAligned(sizeof(size_t) + sizeof(size_t) + sizeof(WDomNode*) * newCapacity);
		if(m_value.m_pArrayList)
		{
			for(size_t i = 0; i < m_value.m_pArrayList->m_size; i++)
				pArrayList->m_items[i] = m_value.m_pArrayList->m_items[i];
			pArrayList->m_size = m_value.m_pArrayList->m_size;
		}
		else
			pArrayList->m_size = 0;
		pArrayList->m_capacity = newCapacity;
		m_value.m_pArrayList = pArrayList;
	}
	m_value.m_pArrayList->m_items[m_value.m_pArrayList->m_size] = pNode;
	m_value.m_pArrayList->m_size++;
	return pNode;
}

WDomNode* WDomNode::add(WDom* pDoc, bool b)
{
	return add(pDoc, pDoc->newBool(b));
}

WDomNode* WDomNode::add(WDom* pDoc, long long n)
{
	return add(pDoc, pDoc->newInt(n));
}

WDomNode* WDomNode::add(WDom* pDoc, size_t n)
{
	return add(pDoc, pDoc->newInt((long long)n));
}

WDomNode* WDomNode::add(WDom* pDoc, double d)
{
	return add(pDoc, pDoc->newDouble(d));
}

WDomNode* WDomNode::add(WDom* pDoc, const char* str)
{
	return add(pDoc, pDoc->newString(str));
}

void WDomNode::del(const char* szField)
{
	if(m_type != type_obj)
		throw Ex(to_str_brief(*this), " is not an obj");
	if(strcmp(m_value.m_pLastField->m_pName, szField) == 0)
	{
		m_value.m_pLastField = m_value.m_pLastField->m_pPrev;
		return;
	}
	else
	{
		for(WDomObjField* pField = m_value.m_pLastField; pField->m_pPrev; pField = pField->m_pPrev)
		{
			if(strcmp(pField->m_pPrev->m_pName, szField) == 0)
			{
				pField->m_pPrev = pField->m_pPrev->m_pPrev;
				return;
			}
		}
		throw Ex("No field named ", szField);
	}
}

void WDomNode::del(size_t index)
{
	if(m_type != type_list)
		throw Ex(to_str_brief(*this), " is not a list");
	if(index >= m_value.m_pArrayList->m_size)
		throw Ex("Index out of range. Index ", to_str(index), ". Size ", to_str(m_value.m_pArrayList->m_size));
	for(size_t i = index; i + 1 < m_value.m_pArrayList->m_size; i++)
		m_value.m_pArrayList->m_items[i] = m_value.m_pArrayList->m_items[i + 1];
	m_value.m_pArrayList->m_size--;
}

void writeJSONString(std::ostream& stream, const char* szString)
{
	stream << '"';
	while(*szString != '\0')
	{
		if(*szString < ' ')
		{
			switch(*szString)
			{
				case '\b': stream << "\\b"; break;
				case '\f': stream << "\\f"; break;
				case '\n': stream << "\\n"; break;
				case '\r': stream << "\\r"; break;
				case '\t': stream << "\\t"; break;
				default:
					stream << (*szString);
			}
		}
		else if(*szString == '\\')
			stream << "\\\\";
		else if(*szString == '"')
			stream << "\\\"";
		else
			stream << (*szString);
		szString++;
	}
	stream << '"';
}

size_t writeJSONStringCpp(std::ostream& stream, const char* szString)
{
	stream << "\\\"";
	size_t chars = 2;
	while(*szString != '\0')
	{
		if(*szString < ' ')
		{
			switch(*szString)
			{
				case '\b': stream << "\\\\b"; break;
				case '\f': stream << "\\\\f"; break;
				case '\n': stream << "\\\\n"; break;
				case '\r': stream << "\\\\r"; break;
				case '\t': stream << "\\\\t"; break;
				default:
					stream << (*szString);
			}
			chars += 3;
		}
		else if(*szString == '\\')
		{
			stream << "\\\\\\\\";
			chars += 4;
		}
		else if(*szString == '"')
		{
			stream << "\\\\\\\"";
			chars += 4;
		}
		else
		{
			stream << (*szString);
			chars++;
		}
		szString++;
	}
	stream << "\\\"";
	chars += 2;
	return chars;
}

void WDomNode::saveJson(const char* filename) const
{
	WDom doc;
	doc.setRoot(this);
	doc.saveJson(filename);
}

void WDomNode::writeJson(std::ostream& stream) const
{
	std::ios_base::fmtflags oldflags = stream.flags();
	stream << std::fixed;
	switch(m_type)
	{
		case type_obj:
			stream << "{";
			reverseFieldOrder();
			for(WDomObjField* pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
			{
				if(pField != m_value.m_pLastField)
					stream << ",";
				writeJSONString(stream, pField->m_pName);
				stream << ":";
				pField->m_pValue->writeJson(stream);
			}
			reverseFieldOrder();
			stream << "}";
			break;
		case type_list:
			stream << "[";
			if(m_value.m_pArrayList)
			{
				if(m_value.m_pArrayList->m_size > 0)
					m_value.m_pArrayList->m_items[0]->writeJson(stream);
				for(size_t i = 1; i < m_value.m_pArrayList->m_size; i++)
				{
					stream << ",";
					m_value.m_pArrayList->m_items[i]->writeJson(stream);
				}
			}
			stream << "]";
			break;
		case type_bool:
			stream << (m_value.m_bool ? "true" : "false");
			break;
		case type_int:
			stream << m_value.m_int;
			break;
		case type_double:
			stream << m_value.m_double;
			break;
		case type_string:
			writeJSONString(stream, m_value.m_string);
			break;
		case type_null:
			stream << "null";
			break;
		default:
			throw Ex("Unrecognized node type");
	}
	stream.flags(oldflags);
}

void newLineAndIndent(std::ostream& stream, size_t indents)
{
	stream << "\n";
	for(size_t i = 0; i < indents; i++)
		stream << "	";
}

void WDomNode::writeJsonPretty(std::ostream& stream, size_t indents) const
{
	std::ios_base::fmtflags oldflags = stream.flags();
	stream << std::fixed;
	switch(m_type)
	{
		case type_obj:
			stream << "{";
			reverseFieldOrder();
			for(WDomObjField* pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
			{
				newLineAndIndent(stream, indents + 1);
				writeJSONString(stream, pField->m_pName);
				stream << ":";
				pField->m_pValue->writeJsonPretty(stream, indents + 1);
				if(pField->m_pPrev)
					stream << ",";
			}
			reverseFieldOrder();
			newLineAndIndent(stream, indents);
			stream << "}";
			break;
		case type_list:
		{
			// Check whether all items in the list are atomic
			bool allAtomic = true;
			if(m_value.m_pArrayList)
			{
				if(m_value.m_pArrayList->m_size >= 1024)
					allAtomic = false;
				for(size_t i = 0; i < m_value.m_pArrayList->m_size && allAtomic; i++)
				{
					WDomNode* pNode = m_value.m_pArrayList->m_items[i];
					if(pNode->type() == WDomNode::type_obj || pNode->type() == WDomNode::type_list)
						allAtomic = false;
				}
			}

			// Print the items
			if(allAtomic)
			{
				// All items are atomic, so let's put them all on one line
				stream << "[";
				if(m_value.m_pArrayList)
				{
					for(size_t i = 0; i < m_value.m_pArrayList->m_size; i++)
					{
						if(i > 0)
						{
							stream << ",";
							if(i % 100 == 0)
								newLineAndIndent(stream, indents);
						}
						WDomNode* pNode = m_value.m_pArrayList->m_items[i];
						pNode->writeJson(stream);
					}
				}
				stream << "]";
			}
			else
			{
				// Some items are non-atomic, so let's spread across multiple lines
				newLineAndIndent(stream, indents);
				stream << "[";
				for(size_t i = 0; i < m_value.m_pArrayList->m_size; i++)
				{
					WDomNode* pNode = m_value.m_pArrayList->m_items[i];
					newLineAndIndent(stream, indents + 1);
					pNode->writeJsonPretty(stream, indents + 1);
					if(i + 1 < m_value.m_pArrayList->m_size)
						stream << ",";
				}
				newLineAndIndent(stream, indents);
				stream << "]";
			}
		}
		break;
		case type_bool:
			stream << (m_value.m_bool ? "true" : "false");
			break;
		case type_int:
			stream << m_value.m_int;
			break;
		case type_double:
			stream << m_value.m_double;
			break;
		case type_string:
			writeJSONString(stream, m_value.m_string);
			break;
		case type_null:
			stream << "null";
			break;
		default:
			throw Ex("Unrecognized node type");
	}
	stream.flags(oldflags);
}

size_t WDomNode::writeJsonCpp(std::ostream& stream, size_t col) const
{
	std::ios_base::fmtflags oldflags = stream.flags();
	stream << std::fixed;
	switch(m_type)
	{
		case type_obj:
			stream << "{";
			col++;
			reverseFieldOrder();
			for(WDomObjField* pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
			{
				if(pField != m_value.m_pLastField)
				{
					stream << ",";
					col++;
				}
				if(col >= 200)
				{
					stream << "\"\n\"";
					col = 0;
				}
				col += writeJSONStringCpp(stream, pField->m_pName);
				stream << ":";
				col++;
				col = pField->m_pValue->writeJsonCpp(stream, col);
			}
			reverseFieldOrder();
			stream << "}";
			col++;
			break;
		case type_list:
			stream << "[";
			col++;
			if(m_value.m_pArrayList)
			{
				for(size_t i = 0; i < m_value.m_pArrayList->m_size; i++)
				{
					if(i + 1 < m_value.m_pArrayList->m_size)
					{
						stream << ",";
						col++;
					}
					if(col >= 200)
					{
						stream << "\"\n\"";
						col = 0;
					}
					col = m_value.m_pArrayList->m_items[i]->writeJsonCpp(stream, col);
				}
			}
			stream << "]";
			col++;
			break;
		case type_bool:
			stream << (m_value.m_bool ? "true" : "false");
			col += 4;
			break;
		case type_int:
			stream << m_value.m_int;
			col += 4; // just a guess
			break;
		case type_double:
			stream << m_value.m_double;
			col += 8; // just a guess
			break;
		case type_string:
			col += writeJSONStringCpp(stream, m_value.m_string);
			break;
		case type_null:
			stream << "null";
			col += 4;
			break;
		default:
			throw Ex("Unrecognized node type");
	}
	if(col >= 200)
	{
		stream << "\"\n\"";
		col = 0;
	}
	stream.flags(oldflags);
	return col;
}

bool isXmlInlineType(int type)
{
	if(type == WDomNode::type_string || type == WDomNode::type_int || type == WDomNode::type_double || type == WDomNode::type_bool || type == WDomNode::type_null)
		return true;
	else
		return false;
}

void WDomNode::writeXmlInlineValue(std::ostream& stream)
{
	switch(m_type)
	{
		case type_string:
			stream << m_value.m_string; // todo: escape the string as necessary
			break;
		case type_int:
			stream << m_value.m_int;
			break;
		case type_double:
			stream << m_value.m_double;
			break;
		case type_bool:
			stream << (m_value.m_bool ? "true" : "false");
			break;
		case type_null:
			stream << "null";
			break;
		default:
			throw Ex("Type cannot be inlined");
	}
}

void WDomNode::writeXml(std::ostream& stream, const char* szLabel) const
{
	switch(m_type)
	{
		case type_obj:
		{
			stream << "<" << szLabel;
			reverseFieldOrder();
			size_t nonInlinedChildren = 0;
			for(WDomObjField* pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
			{
				if(isXmlInlineType(pField->m_pValue->m_type))
				{
					stream << " " << pField->m_pName << "=\"";
					pField->m_pValue->writeXmlInlineValue(stream);
					stream << "\"";
				}
				else
					nonInlinedChildren++;
			}
			if(nonInlinedChildren == 0)
				stream << " />";
			else
			{
				stream << ">";
				for(WDomObjField* pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
				{
					if(!isXmlInlineType(pField->m_pValue->m_type))
						pField->m_pValue->writeXml(stream, pField->m_pName);
				}
				stream << "</" << szLabel << ">";
			}
			reverseFieldOrder();
		}
			return;
		case type_list:
			stream << "<" << szLabel << ">";
			if(m_value.m_pArrayList)
			{
				for(size_t i = 0; i < m_value.m_pArrayList->m_size; i++)
				{
					WDomNode* pNode = m_value.m_pArrayList->m_items[i];
					pNode->writeXml(stream, "i");
				}
			}
			stream << "</" << szLabel << ">";
			return;
		case type_bool:
			stream << "<" << szLabel << ">";
			stream << (m_value.m_bool ? "true" : "false");
			stream << "</" << szLabel << ">";
			break;
		case type_int:
			stream << "<" << szLabel << ">";
			stream << m_value.m_int;
			stream << "</" << szLabel << ">";
			break;
		case type_double:
			stream << "<" << szLabel << ">";
			stream << m_value.m_double;
			stream << "</" << szLabel << ">";
			break;
		case type_string:
			stream << "<" << szLabel << ">";
			stream << m_value.m_string; // todo: escape the string as necessary
			stream << "</" << szLabel << ">";
			break;
		case type_null:
			stream << "<" << szLabel << ">";
			stream << "null";
			stream << "</" << szLabel << ">";
			break;
		default:
			throw Ex("Unrecognized node type");
	}
}

bool WDomNode::isEqual(const WDomNode* pOther) const
{
	switch(m_type)
	{
		case type_obj:
			if(pOther->m_type != type_obj)
				return false;
			{
				WDomObjField* pOth = pOther->m_value.m_pLastField;
				for(WDomObjField* pField = m_value.m_pLastField; pField; pField = pField->m_pPrev)
				{
					if(pOth && strcmp(pField->m_pName, pOth->m_pName) == 0)
					{
						if(!pField->m_pValue->isEqual(pOth->m_pValue))
							return false;
					}
					else
					{
						for(pOth = pOther->m_value.m_pLastField; pOth; pOth = pOth->m_pPrev)
						{
							if(strcmp(pField->m_pName, pOth->m_pName) == 0)
							{
								if(!pField->m_pValue->isEqual(pOth->m_pValue))
									return false;
								break;
							}
						}
					}
					if(pOth)
						pOth = pOth->m_pPrev;
				}
			}
			return true;

		case type_list:
			if(pOther->m_type != type_list)
				return false;
			if(size() != pOther->size())
				return false;
			for(size_t i = 0; i < size(); i++)
			{
				if(!get(i)->isEqual(pOther->get(i)))
					return false;
			}
			return true;

		case type_bool:
			if(pOther->m_type != type_bool)
				return false;
			if(m_value.m_bool == pOther->m_value.m_bool)
				return true;
			else
				return false;

		case type_int:
			if(pOther->m_type != type_int)
				return false;
			if(m_value.m_int == pOther->m_value.m_int)
				return true;
			else
				return false;

		case type_double:
			if(pOther->m_type != type_double)
				return false;
			if(m_value.m_double == pOther->m_value.m_double)
				return true;
			else
				return false;

		case type_string:
			if(pOther->m_type != type_string)
				return false;
			if(strcmp(m_value.m_string, pOther->m_value.m_string) == 0)
				return true;
			else
				return false;

		case type_null:
			if(pOther->m_type != type_bool)
				return false;
			else
				return true;

		default:
			throw Ex("Unrecognized node type");
	}
}


// -------------------------------------------------------------------------------

class WJsonTokenizer : public WTokenizer
{
public:
	WCharSet m_whitespace, m_real, m_quot;

	WJsonTokenizer(const char* szFilename) : WTokenizer(szFilename),
	                                         m_whitespace("\t\n\r "), m_real("-.+0-9eE"), m_quot("\"") {}
	WJsonTokenizer(const char* pFile, size_t len) : WTokenizer(pFile, len),
	                                                m_whitespace("\t\n\r "), m_real("-.+0-9eE"), m_quot("\"") {}
	~WJsonTokenizer() override {}
};

class Bogus1
{
public:
	char m_type;
	double m_double;
};

WDom::WDom()
: m_heap(2000), m_pRoot(nullptr), m_line(0), m_len(0), m_pDoc(nullptr)
{
}

WDom::~WDom()
{
}

void WDom::clear()
{
	m_pRoot = nullptr;
	m_heap.clear();
}

WDomNode* WDom::newObj()
{
	WDomNode* pNewObj = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double) + sizeof(WDomObjField*));
	pNewObj->m_type = WDomNode::type_obj;
	pNewObj->m_value.m_pLastField = nullptr;
	return pNewObj;
}

WDomNode* WDom::newList()
{
	WDomNode* pNewList = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double) + sizeof(WDomListItem*));
	pNewList->m_type = WDomNode::type_list;
	pNewList->m_value.m_pArrayList = nullptr;
	return pNewList;
}

WDomNode* WDom::newNull()
{
	WDomNode* pNewNull = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double));
	pNewNull->m_type = WDomNode::type_null;
	return pNewNull;
}

WDomNode* WDom::newBool(bool b)
{
	WDomNode* pNewBool = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double) + sizeof(bool));
	pNewBool->m_type = WDomNode::type_bool;
	pNewBool->m_value.m_bool = b;
	return pNewBool;
}

WDomNode* WDom::newInt(long long n)
{
	WDomNode* pNewInt = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double) + sizeof(long long));
	pNewInt->m_type = WDomNode::type_int;
	pNewInt->m_value.m_int = n;
	return pNewInt;
}

WDomNode* WDom::newDouble(double d)
{
	if(d >= -1.5e308 && d <= 1.5e308)
	{
		WDomNode* pNewDouble = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double) + sizeof(double));
		pNewDouble->m_type = WDomNode::type_double;
		pNewDouble->m_value.m_double = d;
		return pNewDouble;
	}
	else
	{
		throw Ex("Invalid value: ", to_str(d));
		return nullptr;
	}
}

WDomNode* WDom::newString(const char* pString, size_t len)
{
	WDomNode* pNewString = (WDomNode*)m_heap.allocAligned(offsetof(Bogus1, m_double) + len + 1);
	pNewString->m_type = WDomNode::type_string;
	memcpy(pNewString->m_value.m_string, pString, len);
	pNewString->m_value.m_string[len] = '\0';
	return pNewString;
}

WDomNode* WDom::newString(const char* szString)
{
	return newString(szString, strlen(szString));
}

WDomObjField* WDom::newField()
{
	return (WDomObjField*)m_heap.allocAligned(sizeof(WDomObjField));
}

WDomNode* WDom::clone(WDomNode* pNode)
{
	switch(pNode->type())
	{
		case WDomNode::type_obj:
		{
			WDomNode* pClone = newObj();
			for(WDomObjField* pField = pNode->m_value.m_pLastField; pField; pField = pField->m_pPrev)
			{
				WDomObjField* pFieldClone = newField();
				pFieldClone->m_pPrev = pClone->m_value.m_pLastField;
				pClone->m_value.m_pLastField = pFieldClone;
				pFieldClone->m_pName = m_heap.add(pField->m_pName);
				pFieldClone->m_pValue = clone(pField->m_pValue);
			}
			return pClone;
		}
		case WDomNode::type_list:
		{
			WDomNode* pClone = newList();
			for(size_t i = 0; i < pNode->size(); i++)
				pClone->add(this, clone(pNode->get(i)));
			return pClone;
		}
		case WDomNode::type_bool: return newBool(pNode->asBool());
		case WDomNode::type_int: return newInt(pNode->asInt());
		case WDomNode::type_double: return newDouble(pNode->asDouble());
		case WDomNode::type_string: return newString(pNode->asString());
		case WDomNode::type_null: return newNull(); ;
		default: throw Ex("Unexpected type");
	}
}

char* WDom::loadJsonString(WJsonTokenizer& tok)
{
	tok.skipExact("\"");
	char* szTok = tok.readUntil_escaped('\\', tok.m_quot);
	tok.skip(1);
	size_t eat = 0;
	char* szString = szTok;
	while(szString[eat] != '\0')
	{
		char c = szString[eat];
		if(c == '\\')
		{
			switch(szString[eat + 1])
			{
				case '"': c = '"'; break;
				case '\\': c = '\\'; break;
				case '/': c = '/'; break;
				case 'b': c = '\b'; break;
				case 'f': c = '\f'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'u':
					WAssert(false); // Escaped unicode characters are not yet supported
					c = '_';
					eat += 3;
					break;
				default:
					throw Ex("Unrecognized escape sequence");
			}
			eat++;
		}
		*szString = c;
		szString++;
	}
	*szString = '\0';
	return szTok;
}

WDomNode* WDom::loadJsonObject(WJsonTokenizer& tok)
{
	tok.skipExact("{");
	WDomNode* pNewObj = newObj();
	bool readyForField = true;
	WCharSet& whitespace = tok.m_whitespace;
	while(tok.has_more())
	{
		tok.skipWhile(whitespace);
		char c = tok.peek();
		if(c == '}')
		{
			tok.skip(1);
			break;
		}
		else if(c == ',')
		{
			if(readyForField)
				throw Ex("Unexpected ',' in JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			tok.skip(1);
			readyForField = true;
		}
		else if(c == '\"')
		{
			if(!readyForField)
				throw Ex("Expected a ',' before the next field in JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			WDomObjField* pNewField = newField();
			pNewField->m_pPrev = pNewObj->m_value.m_pLastField;
			pNewObj->m_value.m_pLastField = pNewField;
			pNewField->m_pName = m_heap.add(loadJsonString(tok));
			tok.skipWhile(whitespace);
			tok.skipExact(":");
			tok.skipWhile(whitespace);
			pNewField->m_pValue = loadJsonValue(tok);
			readyForField = false;
		}
		else if(c == '\0')
			throw Ex("Expected a matching '}' in JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
		else
			throw Ex("Expected a '}' or a '\"' at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
	}
	return pNewObj;
}

WDomNode* WDom::loadJsonArray(WJsonTokenizer& tok)
{
	tok.skipExact("[");
	WDomNode* pNewList = newList();
	bool readyForValue = true;
	while(tok.has_more())
	{
		tok.skipWhile(tok.m_whitespace);
		char c = tok.peek();
		if(c == ']')
		{
			tok.skip(1);
			break;
		}
		else if(c == ',')
		{
			if(readyForValue)
				throw Ex("Unexpected ',' in JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			tok.skip(1);
			readyForValue = true;
		}
		else if(c == '\0')
			throw Ex("Expected a matching ']' in JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
		else
		{
			if(!readyForValue)
				throw Ex("Expected a ',' or ']' in JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
			WDomNode* pValue = loadJsonValue(tok);
			pNewList->add(this, pValue);
			readyForValue = false;
		}
	}
	return pNewList;
}

WDomNode* WDom::loadJsonNumber(WJsonTokenizer& tok)
{
	char* szString = tok.readWhile(tok.m_real);
	// SAKBox: was a double only with a '.', so the integer reader stopped at an
	// exponent's 'e' (1e5 read as 1), and malformed numbers were cut short.
	// Now: an integer if all of it is one (and it fits a long long), else a
	// double (1.5, 1e5, or a bigger integer), else an error.
	long long n;
	if(parseNumber(szString, &n))
		return newInt(n);
	double d;
	if(!parseNumber(szString, &d))
		throw Ex("Invalid JSON number: ", szString);
	return newDouble(d);
}

WDomNode* WDom::loadJsonValue(WJsonTokenizer& tok)
{
	char c = tok.peek();
	if(c == '"')
		return newString(loadJsonString(tok));
	else if(c == '{')
		return loadJsonObject(tok);
	else if(c == '[')
		return loadJsonArray(tok);
	else if(c == 't')
	{
		tok.skipExact("true");
		return newBool(true);
	}
	else if(c == 'f')
	{
		tok.skipExact("false");
		return newBool(false);
	}
	else if(c == 'n')
	{
		tok.skipExact("null");
		return newNull();
	}
	else if((c >= '0' && c <= '9') || c == '-')
		return loadJsonNumber(tok);
	else if(c == '\0')
	{
		throw Ex("Unexpected end of file while parsing JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
		return nullptr;
	}
	else
	{
		throw Ex("Unexpected token, \"", to_str(c), "\", while parsing JSON file at line ", to_str(tok.line()), ", col ", to_str(tok.col()));
		return nullptr;
	}
}

void WDom::parseJson(const char* pJsonString, size_t len)
{
	WJsonTokenizer tok(pJsonString, len);
	tok.skipWhile(tok.m_whitespace);
	setRoot(loadJsonValue(tok));
}

void WDom::loadJson(const char* szFilename)
{
	WJsonTokenizer tok(szFilename);
	tok.skipWhile(tok.m_whitespace);
	setRoot(loadJsonValue(tok));
}

void WDom::writeJson(std::ostream& stream) const
{
	if(!m_pRoot)
		throw Ex("No root node has been set");
	stream.precision(14);
	m_pRoot->writeJson(stream);
}

void WDom::writeJsonPretty(std::ostream& stream) const
{
	if(!m_pRoot)
		throw Ex("No root node has been set");
	stream.precision(14);
	m_pRoot->writeJsonPretty(stream, 0);
}

void WDom::writeJsonCpp(std::ostream& stream) const
{
	if(!m_pRoot)
		throw Ex("No root node has been set");
	stream.precision(14);
	stream << "const char* g_rename_me = \"";
	m_pRoot->writeJsonCpp(stream, 0);
	stream << "\";\n\n";
}

void WDom::saveJson(const char* szFilename) const
{
	std::ofstream os;
	os.exceptions(std::ios::badbit | std::ios::failbit);
	try
	{
		os.open(szFilename, std::ios::binary);
	}
	catch(const std::exception&)
	{
		throw Ex("Error while trying to create the file, ", szFilename, ". ", strerror(errno));
	}
	writeJson(os);
}

void WDom::writeXml(std::ostream& stream) const
{
	if(!m_pRoot)
		throw Ex("No root node has been set");
	stream.precision(14);
	stream << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	m_pRoot->writeXml(stream, "root");
}

std::string to_str(const WDomNode& node)
{
	std::ostringstream os;
	node.writeJsonPretty(os, 0);
	return os.str();
}

std::string to_str_brief(const WDomNode& node)
{
	std::ostringstream os;
	node.writeJsonPretty(os, 0);
	std::string s = os.str();
	if(s.length() < 60)
		return s;
	std::string s2 = s.substr(0, 30);
	s2 += " ... ";
	s2 += s.substr(s.length() - 30, 30);
	return s2;
}


std::string to_str(const WDom& doc)
{
	std::ostringstream os;
	doc.writeJsonPretty(os);
	return os.str();
}

// static
void WDom::test()
{
	const char* szTestFile =
	  "{\n"
	  "	\"name\":\"Bob\\nis\\\\cool\",\n"
	  "	\"pet\":{\n"
	  "		\"hair\":\"thick\",\n"
	  "		\"age\":12\n"
	  "	},\n"
	  "	\"acquantances\":[\n"
	  "		{\n"
	  "			\"name\":\"Bill\",\n"
	  "			\"age\":31\n"
	  "		},\n"
	  "		{\n"
	  "			\"name\":\"Sally\",\n"
	  "			\"age\":24\n"
	  "		},\n"
	  "		{\n"
	  "			\"name\":\"George\",\n"
	  "			\"age\":18\n"
	  "		}\n"
	  "	],\n"
	  "	\"names\":[\n"
	  "		\"Bob\",\n"
	  "		\"Doe\"\n"
	  "	],\n"
	  "	\"male\"  :  true , \n"
	  "	\"height\": 5.8,\n"
	  "	\"temp\":98.6\n"
	  "}\n";
	WDom doc;
	doc.parseJson(szTestFile, strlen(szTestFile));
}

















WJsonAsADatabase::WJsonAsADatabase(const char* szBasePath)
: m_basePath(szBasePath)
{
}

WJsonAsADatabase::~WJsonAsADatabase()
{
	flush(true);
}

void WJsonAsADatabase::flush(bool clear_cache)
{
	map<string, WJaadDom*>::iterator it = m_doms.begin();
	while(it != m_doms.end())
	{
		const string& filename = it->first;
		WJaadDom* pDom = it->second;
		if(pDom->modCount() > 0)
		{
			string fullpath = m_basePath;
			fullpath += filename;
			pDom->saveJson(fullpath.c_str());
			pDom->resetModCount();
		}
		++it;
	}
	if(clear_cache)
	{
		map<string, WJaadDom*>::iterator it = m_doms.begin();
		while(it != m_doms.end())
		{
			WJaadDom* pDom = it->second;
			delete(pDom);
			++it;
		}
		m_doms.clear();
	}
}

// static
size_t WJsonAsADatabase::findTok(const char* s, char tok, size_t start)
{
	bool q = false;
	bool qq = false;
	size_t esc = 0;
	size_t brace = 0;
	size_t brack = 0;
	size_t i;
	for(i = start; s[i] != '\0'; i++)
	{
		if(!q && !qq)
		{
			if(s[i] == tok && brace == 0 && brack == 0)
				break;
			else if(s[i] == '[')
				brace++;
			else if(s[i] == ']')
				brace--;
			else if(s[i] == '{')
				brack++;
			else if(s[i] == '}')
				brack--;
			else if(s[i] == '\'')
				q = true;
			else if(s[i] == '"')
				qq = true;
		}
		else
		{
			if(esc > 0)
				esc--;
			else if(s[i] == '\\')
				esc = 2;
			else if(q && s[i] == '\'')
				q = false;
			else if(qq && s[i] == '"')
				qq = false;
		}
	}
	return i;
}


WDomNode* WJsonAsADatabase::findNode(WDom* pDoc, WDomNode* pOb, WDom* pResponseDom, const char* szCmd, char op)
{
	size_t pos = 0;
	while(true)
	{
		// Eat whitespace
		while(szCmd[pos] != '\0' && szCmd[pos] <= ' ')
			pos++;
		char c = szCmd[pos];
		if(c == '\0')
			break;

		if(c == '.')
		{
			size_t i;
			for(i = pos + 1; szCmd[i] != '\0' && szCmd[i] != '.' && szCmd[i] != '['; i++)
			{
			}
			string field(szCmd + pos + 1, i - (pos + 1));
			field = trim(field);
			if(field.length() == 0)
				throw Ex("Expected a field name after '.'");
			if(!pOb && op != '\0')
				pOb = (WDomNode*)pDoc->setRoot(pDoc->newObj());
			if(pOb->type() != WDomNode::type_obj)
				throw Ex("'.' can only be used on object types.");
			WDomNode* pChild = pOb->getIfExists(field.c_str());
			if(!pChild)
			{
				if(op == '\0')
					throw Ex("No field named ", field);
				else if(szCmd[i] == '.') // the next connector will be a '.'
					pChild = pOb->add(pDoc, field.c_str(), pDoc->newObj());
				else if(szCmd[i] == '[' || op == '+') // indexed next, or (with no next connector) appended to: a list
					pChild = pOb->add(pDoc, field.c_str(), pDoc->newList());
				else
					throw Ex("Cannot determine the type for missing field ", field);
			}
			pOb = pChild;
			pos = i;
		}
		else if(c == '[')
		{
			if(pOb && pOb->type() != WDomNode::type_list)
				throw Ex("'[]' can only be used with list types");

			// Find the corresponding ']'
			size_t i = findTok(szCmd, ']', pos + 1);
			size_t indStart = pos + 1;
			while(indStart < i && szCmd[indStart] <= ' ')
				indStart++;
			if(indStart == i)
				throw Ex("Expected an index or equation after '['");
			string sIndex(szCmd + indStart, i - indStart);
			if(!pOb && op != '\0')
				pOb = (WDomNode*)pDoc->setRoot(pDoc->newList());

			// Parse the index
			char c = sIndex[0];
			if(c >= '0' && c <= '9')
			{
				// It's a numerical index
				std::stringstream sstream(sIndex);
				size_t index;
				sstream >> index;
				if(index >= pOb->size())
					throw Ex("index out of range. Index: ", to_str(index), ". Size: ", to_str(pOb->size()));
				pOb = pOb->get(index);
			}
			else
			{
				// It's an equation
				if(c != '.' && c != '[' && c != '=')
					throw Ex("Unexpected index format");

				// Find the "=="
				size_t eq = findTok(sIndex.c_str(), '=');
				if(eq >= sIndex.length())
					throw Ex("Expected an '=' in an equation");
				string pre(sIndex, 0, eq);
				if(sIndex.length() > eq && sIndex[eq + 1] == '=')
					eq++;
				pResponseDom->parseJson(sIndex.c_str() + eq + 1, sIndex.length() - eq - 1);
				WDomNode* pPost = pResponseDom->root();
				size_t j;
				for(j = 0; j < pOb->size(); j++)
				{
					WDomNode* pCand = pOb->get(j);
					WDomNode* pComp = findNode(pDoc, pCand, pResponseDom, pre.c_str(), op);
					if(pComp->isEqual(pPost))
					{
						pOb = pCand;
						break;
					}
				}
				if(j >= pOb->size())
					throw Ex("No matching node found");
			}

			// Move past the ']'
			pos = i;
			if(szCmd[pos] == ']')
				pos++;
		}
		else
			throw Ex("Unexpected token: ", string(szCmd + pos, 1));
	}
	return pOb;
}

WDomNode* WJsonAsADatabase::findLValue(WDom* pDoc, WDomNode* pOb, WDom* pResponseDom, const char* szCmd, string* pOutField, size_t* pOutIndex)
{
	WDomNode* pPrevOb = nullptr;
	size_t pos = 0;
	while(true)
	{
		// Eat whitespace
		while(szCmd[pos] != '\0' && szCmd[pos] <= ' ')
			pos++;
		char c = szCmd[pos];
		if(c == '\0')
			break;

		pPrevOb = pOb;
		if(c == '.')
		{
			size_t i;
			for(i = pos + 1; szCmd[i] != '\0' && szCmd[i] != '.' && szCmd[i] != '['; i++)
			{
			}
			string field(szCmd + pos + 1, i - (pos + 1));
			field = trim(field); // SAKBox: as findNode does; ".a = 1" made a field named "a "
			if(pOutField)
				*pOutField = field;
			if(field.length() == 0)
				throw Ex("Expected a field name after '.'");
			if(pOb->type() != WDomNode::type_obj)
				throw Ex("'.' can only be used on object types.");
			pOb = pOb->getIfExists(field.c_str());
			if(!pOb)
			{
				// SAKBox: these used pOb, which is null here: adding through it
				// crashed, and returning it made apply replace the whole document
				size_t nextPos = i;
				while(szCmd[nextPos] != '\0' && szCmd[nextPos] <= ' ')
					nextPos++;
				if(szCmd[nextPos] == '.')
					pOb = pPrevOb->add(pDoc, field.c_str(), pDoc->newObj());
				else if(szCmd[nextPos] == '[')
					pOb = pPrevOb->add(pDoc, field.c_str(), pDoc->newList());
				else if(szCmd[nextPos] == '\0')
					return pPrevOb; // the object to add the field to
				else
					throw Ex("Unexpected token: ", string(szCmd + nextPos, 1));
			}
			pos = i;
		}
		else if(c == '[')
		{
			if(pOb->type() != WDomNode::type_list)
				throw Ex("'[]' can only be used with list types");

			// Find the corresponding ']'
			size_t i = findTok(szCmd, ']', pos + 1);
			size_t indStart = pos + 1;
			while(indStart < i && szCmd[indStart] <= ' ')
				indStart++;
			if(indStart == i)
				throw Ex("Expected an index or equation after '['");
			string sIndex(szCmd + indStart, i - indStart);

			// Parse the index
			char c = sIndex[0];
			if(c >= '0' && c <= '9')
			{
				// It's a numerical index
				std::stringstream sstream(sIndex);
				size_t index;
				sstream >> index;
				if(pOutIndex)
					*pOutIndex = index;
				if(index > pOb->size())
					throw Ex("index out of range. Index: ", to_str(index), ". Size: ", to_str(pOb->size()));
				if(index == pOb->size())
				{
					size_t nextPos = i;
					if(szCmd[nextPos] == ']')
						nextPos++;
					while(szCmd[nextPos] != '\0' && szCmd[nextPos] <= ' ')
						nextPos++;
					// SAKBox: appends the element, and pOb->get(index) below then
					// finds it (it used to look the index up in the new element)
					if(szCmd[nextPos] == '.')
						pOb->add(pDoc, pDoc->newObj());
					else if(szCmd[nextPos] == '[')
						pOb->add(pDoc, pDoc->newList());
					else if(szCmd[nextPos] == '\0')
						return pOb;
					else
						throw Ex("Unexpected token: ", string(szCmd + nextPos, 1));
				}
				pOb = pOb->get(index);
			}
			else
			{
				// It's an equation
				if(c != '.' && c != '[' && c != '=')
					throw Ex("Unexpected index format");

				// Find the "=="
				size_t eq = findTok(sIndex.c_str(), '=');
				if(eq >= sIndex.length())
					throw Ex("Expected an '=' in an equation");
				string pre(sIndex, 0, eq);
				if(sIndex.length() > eq && sIndex[eq + 1] == '=')
					eq++;
				pResponseDom->parseJson(sIndex.c_str() + eq + 1, sIndex.length() - eq - 1);
				WDomNode* pPost = pResponseDom->root();
				size_t j;
				for(j = 0; j < pOb->size(); j++)
				{
					WDomNode* pCand = pOb->get(j);
					WDomNode* pComp = findNode(pDoc, pCand, pResponseDom, pre.c_str(), '=');
					if(pComp->isEqual(pPost))
					{
						if(pOutIndex)
							*pOutIndex = j;
						pOb = pCand;
						break;
					}
				}
				if(j >= pOb->size())
					throw Ex("No matching node found");
			}

			// Move past the ']'
			pos = i;
			if(szCmd[pos] == ']')
				pos++;
		}
		else
			throw Ex("Unexpected token: ", string(szCmd + pos, 1));
	}
	return pPrevOb;
}

WJaadDom* WJsonAsADatabase::getDom(const char* szFile)
{
	map<string, WJaadDom*>::iterator it = m_doms.find(szFile);
	if(it == m_doms.end())
	{
		// Load the DOM from file
		WJaadDom* pDoc = new WJaadDom();
		Holder<WJaadDom> hDoc(pDoc);
		string filename = m_basePath;
		filename += szFile;
		if(WFile::doesFileExist(filename.c_str()))
			pDoc->loadJson(filename.c_str());
		else
		{
			PathData pd;
			WFile::parsePath(filename.c_str(), &pd);
			std::string dir = filename.substr(0, pd.fileStart);
			if(!WFile::doesDirExist(dir.c_str()))
				throw Ex("No such directory: ", dir.substr(m_basePath.length()));
		}
		hDoc.release();
		m_doms.insert(std::pair<string, WJaadDom*>(szFile, pDoc));
		return pDoc;
	}
	else
		return it->second;
}

void WJsonAsADatabase::add(WDomNode* pRequest, WDom* pDoc, WDomNode* pOb)
{
	if(pOb->type() == WDomNode::type_obj)
	{
		const char* szName = pRequest->getString("name");
		WDomNode* pValue = pRequest->get("val");
		pOb->add(pDoc, szName, pDoc->clone(pValue));
	}
	else if(pOb->type() == WDomNode::type_list)
	{
		WDomNode* pValue = pRequest->get("val");
		pOb->add(pDoc, pDoc->clone(pValue));
	}
	else
		throw Ex("An object or list type is needed for add");
}

void WJsonAsADatabase::del(WDomNode* pRequest, WDom* pDoc, WDomNode* pOb)
{
	if(pOb->type() == WDomNode::type_obj)
	{
		const char* szName = pRequest->getString("name");
		pOb->del(szName);
	}
	else if(pOb->type() == WDomNode::type_list)
	{
		size_t index = pRequest->getInt("index");
		pOb->del(index);
	}
	else
		throw Ex("An object or list type is needed for del");
}

const WDomNode* WJsonAsADatabase::apply(const char* szFilename, const char* szCmd, WDom* pResponseDom)
{
	// Find or load the DOM
	WJaadDom* pDoc = getDom(szFilename);

	// Do the command
	size_t op = findTok(szCmd, '='); // possible bug: the '==' operator should not be detected here
	if(szCmd[op] == '\0')
	{
		// No '=', so the user must be requesting an object be returned
		WDomNode* pNode = findNode(pDoc, pDoc->root(), pResponseDom, szCmd, '\0');
		return pNode;
	}
	pDoc->incModCount();
	if(op > 0 && szCmd[op - 1] == '+')
	{
		// It's a "+=" operation
		string sLeft(szCmd, op - 1);
		WDomNode* pNode = findNode(pDoc, pDoc->root(), pResponseDom, sLeft.c_str(), '+');
		if(pNode)
		{
			if(pNode->type() != WDomNode::type_list)
				throw Ex("The left part before '+=' is not a list type");
			WDomNode* pOldRoot = pDoc->root();
			pDoc->parseJson(szCmd + op + 1, strlen(szCmd + op + 1));
			pNode->add(pDoc, pDoc->root());
			pDoc->setRoot(pOldRoot);
		}
		else
		{
			// Empty document. Make a root list node.
			pNode = pDoc->newList();
			pDoc->parseJson(szCmd + op + 1, strlen(szCmd + op + 1));
			pNode->add(pDoc, pDoc->root());
			pDoc->setRoot(pNode);
		}
		return nullptr;
	}
	else if(op > 0 && szCmd[op - 1] == '-')
	{
		// It's a "-=" operation
		string sLeft(szCmd, op - 1);
		WDomNode* pNode = findNode(pDoc, pDoc->root(), pResponseDom, sLeft.c_str(), '-');
		if(pNode)
		{
			string sField;
			size_t index;
			WDomNode* pVal = findLValue(pDoc, pNode, pResponseDom, szCmd + op + 1, &sField, &index);
			if(pVal->type() == WDomNode::type_list)
			{
				pVal->del(index);
				return nullptr;
			}
			else if(pVal->type() == WDomNode::type_obj)
			{
				pVal->del(sField.c_str());
				return nullptr;
			}
			else
				throw Ex("Can only remove from a list or object type");
		}
		else
			throw Ex("No document to remove from");
	}
	else
	{
		// It must be an "=" operation
		string sLeft(szCmd, op);
		string sField;
		size_t index;
		WDomNode* pNode = findLValue(pDoc, pDoc->root(), pResponseDom, sLeft.c_str(), &sField, &index);
		if(!pNode)
			pDoc->parseJson(szCmd + op + 1, strlen(szCmd + op + 1));
		else if(pNode->type() == WDomNode::type_obj)
		{
			WDomNode* pOldRoot = pDoc->root();
			pDoc->parseJson(szCmd + op + 1, strlen(szCmd + op + 1));
			WDomNode* pVal = pDoc->root();
			pDoc->setRoot(pOldRoot);
			pNode->set(pDoc, sField.c_str(), pVal);
		}
		else if(pNode->type() == WDomNode::type_list)
		{
			WDomNode* pOldRoot = pDoc->root();
			pDoc->parseJson(szCmd + op + 1, strlen(szCmd + op + 1));
			WDomNode* pVal = pDoc->root();
			pDoc->setRoot(pOldRoot);
			pNode->set(pDoc, index, pVal);
		}
		else
			throw Ex("Not an LValue");
		return nullptr;
	}
}




} // namespace WClasses
