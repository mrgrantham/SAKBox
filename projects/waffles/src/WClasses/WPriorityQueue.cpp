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

#include "WPriorityQueue.h"
#include "WError.h"
#include "WHolders.h"
#include "WRand.h"
#include <functional>
#include <queue>
#include <memory>

using std::priority_queue;

namespace WClasses
{

WPriorityQueue::WPriorityQueue(PointerComparer pCompareFunc, void* pThis)
: m_pCompareFunc(pCompareFunc), m_pThis(pThis)
{
	WPriorityQueueEntry tmp;
	m_entries.push_back(tmp);
}

WPriorityQueue::~WPriorityQueue()
{
}

int WPriorityQueue::size()
{
	return (int)m_entries.size() - 1;
}

void WPriorityQueue::minHeapSwap(int a, int b)
{
	WPriorityQueueEntry* pA = &m_entries[a];
	WPriorityQueueEntry* pB = &m_entries[b];

	// swap the min indexes
	WPriorityQueueEntry* pAOther = &m_entries[pA->m_maxIndex];
	WPriorityQueueEntry* pBOther = &m_entries[pB->m_maxIndex];
	pAOther->m_minIndex = b;
	pBOther->m_minIndex = a;

	// swap the max indexes
	int t = pA->m_maxIndex;
	pA->m_maxIndex = pB->m_maxIndex;
	pB->m_maxIndex = t;

	// swap the payloads
	void* pTmp = pA->m_pPayload;
	pA->m_pPayload = pB->m_pPayload;
	pB->m_pPayload = pTmp;
}

void WPriorityQueue::maxHeapSwap(int a, int b)
{
	WPriorityQueueEntry* pA = &m_entries[a];
	WPriorityQueueEntry* pB = &m_entries[b];

	// swap the max indexes
	WPriorityQueueEntry* pAOther = &m_entries[pA->m_minIndex];
	WPriorityQueueEntry* pBOther = &m_entries[pB->m_minIndex];
	pAOther->m_maxIndex = b;
	pBOther->m_maxIndex = a;

	// swap the in indexes
	int t = pA->m_minIndex;
	pA->m_minIndex = pB->m_minIndex;
	pB->m_minIndex = t;
}

void WPriorityQueue::minHeapBubbleUp(int index)
{
	int parIndex;
	WPriorityQueueEntry* pPar;
	WPriorityQueueEntry* pChild = &m_entries[index];
	while(index > 1)
	{
		parIndex = index / 2;
		pPar = &m_entries[parIndex];
		if(m_pCompareFunc(m_pThis, pChild->m_pPayload, pPar->m_pPayload) >= 0)
			break;
		minHeapSwap(index, parIndex);
		index = parIndex;
		pChild = pPar;
	}
}

void WPriorityQueue::maxHeapBubbleUp(int index)
{
	int parIndex;
	WPriorityQueueEntry* pPar;
	WPriorityQueueEntry* pChild = &m_entries[index];
	void* pChildValue;
	void* pParValue;
	while(index > 1)
	{
		parIndex = index / 2;
		pPar = &m_entries[parIndex];
		pChildValue = m_entries[pChild->m_minIndex].m_pPayload;
		pParValue = m_entries[pPar->m_minIndex].m_pPayload;
		if(m_pCompareFunc(m_pThis, pChildValue, pParValue) <= 0)
			break;
		maxHeapSwap(index, parIndex);
		index = parIndex;
		pChild = pPar;
	}
}

void WPriorityQueue::minHeapBubbleDown(int index)
{
	int last = (int)m_entries.size() - 1;
	int childIndex;
	while(true)
	{
		childIndex = index * 2;
		if(childIndex >= last)
			break;
		if(m_pCompareFunc(m_pThis, m_entries[childIndex].m_pPayload, m_entries[childIndex + 1].m_pPayload) < 0)
		{
			minHeapSwap(index, childIndex);
			index = childIndex;
		}
		else
		{
			minHeapSwap(index, childIndex + 1);
			index = childIndex + 1;
		}
	}
	if(index >= last)
		return;
	minHeapSwap(index, last);
	minHeapBubbleUp(index);
}

void WPriorityQueue::maxHeapBubbleDown(int index)
{
	int last = (int)m_entries.size() - 1;
	int childIndex;
	while(true)
	{
		childIndex = index * 2;
		if(childIndex >= last)
			break;
		if(
		  m_pCompareFunc(
		    m_pThis,
		    m_entries[m_entries[childIndex].m_minIndex].m_pPayload,
		    m_entries[m_entries[childIndex + 1].m_minIndex].m_pPayload)
		  > 0)
		{
			maxHeapSwap(index, childIndex);
			index = childIndex;
		}
		else
		{
			maxHeapSwap(index, childIndex + 1);
			index = childIndex + 1;
		}
	}
	if(index >= last)
		return;
	maxHeapSwap(index, last);
	maxHeapBubbleUp(index);
}

void WPriorityQueue::insert(void* pObj)
{
	int index = (int)m_entries.size();
	WPriorityQueueEntry entry(index, index, pObj);
	m_entries.push_back(entry);
	minHeapBubbleUp(index);
	maxHeapBubbleUp(index);
}

void* WPriorityQueue::minimum()
{
	return m_entries[1].m_pPayload;
}

void* WPriorityQueue::maximum()
{
	return m_entries[m_entries[1].m_minIndex].m_pPayload;
}

void WPriorityQueue::removeMin()
{
	int maxIndex = m_entries[1].m_maxIndex;
	minHeapBubbleDown(1);
	maxHeapBubbleDown(maxIndex);
	m_entries.pop_back();
}

void WPriorityQueue::removeMax()
{
	int minIndex = m_entries[1].m_minIndex;
	maxHeapBubbleDown(1);
	minHeapBubbleDown(minIndex);
	m_entries.pop_back();
}

int WPriorityQueueTestComparer(void* pThis, void* pA, void* pB)
{
	int a = *(int*)pA;
	int b = *(int*)pB;
	if(a < b)
		return -1;
	if(a > b)
		return 1;
	return 0;
}

#define TEST_SIZE 4096
void WPriorityQueue::test()
{
	int* buf = new int[TEST_SIZE];
	std::unique_ptr<int[]> hBuf(buf);
	int i, t, r;
	for(i = 0; i < TEST_SIZE; i++)
		buf[i] = i;
	WRand prng(0);
	for(i = TEST_SIZE; i > 2; i--)
	{
		r = (int)prng.next(i);
		t = buf[r];
		buf[r] = buf[i - 1];
		buf[i - 1] = t;
	}
	WPriorityQueue q(WPriorityQueueTestComparer, nullptr);
	for(i = 0; i < TEST_SIZE; i++)
		q.insert(buf + i);
	int* pInt;
	for(i = 0; i < (TEST_SIZE / 2); i++)
	{
		pInt = (int*)q.minimum();
		q.removeMin();
		if(*pInt != i)
			throw Ex("wrong answer");
		pInt = (int*)q.maximum();
		q.removeMax();
		if(*pInt != (TEST_SIZE - 1) - i)
			throw Ex("wrong answer");
	}
}









#define TEST_PQ_SIZE 8192
void WSimplePriorityQueue_test()
{
	WRand r(0);
	priority_queue<double> stdq;
	WSimplePriorityQueue<double*> q;
	double stoof[7];
	for(size_t i = 0; i < 2 * TEST_PQ_SIZE; i++)
	{
		double d = r.uniform();
		size_t n = (size_t)(7.0 * d);
		q.insert(&stoof[n], d);
		stdq.push(-d);
	}
	for(size_t i = 0; i < TEST_PQ_SIZE; i++)
	{
		double stdval = stdq.top();
		stdq.pop();
		double val = q.peekValue();
		double* obj = q.peekObject();
		q.pop();
		if(val != -stdval)
			throw Ex("misorder");
		size_t n = (size_t)(7.0 * val);
		if(obj != &stoof[n])
			throw Ex("mismatch");
	}
	for(size_t i = 2 * TEST_PQ_SIZE; i < 4 * TEST_PQ_SIZE; i++)
	{
		double d = r.uniform();
		size_t n = (size_t)(7.0 * d);
		q.insert(&stoof[n], d);
		stdq.push(-d);
	}
	for(size_t i = TEST_PQ_SIZE; i < 4 * TEST_PQ_SIZE; i++)
	{
		double stdval = stdq.top();
		stdq.pop();
		double val = q.peekValue();
		double* obj = q.peekObject();
		q.pop();
		if(val != -stdval)
			throw Ex("misorder");
		size_t n = (size_t)(7.0 * val);
		if(obj != &stoof[n])
			throw Ex("mismatch");
	}
}




} // namespace WClasses

