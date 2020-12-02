/* 
 *  File: FPGrowth.h
 *  Copyright (c) 2020 Florian Porrmann
 *  
 *  MIT License
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 */

#pragma once
#include <algorithm>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <stack>
#include <cstring>
#include <memory>
#include <signal.h>
#include <omp.h>
#include <mutex>

#include <experimental/vector>

#include "Defines.h"
#include "Logger.h"
#include "Utils.h"
#include "Timer.h"
#include "Types.h"
#include "FPNode.h"
#include "Memory.h"
#include "SigTerm.h"

#include "FrequencyRef.h"
#include "FPTree.h"
#include "Pattern.h"
#include "ClosedDetect.h"

DEFINE_EXCEPTION(FPGException)

class FPGrowth
{
	DISABLE_COPY_ASSIGN_MOVE(FPGrowth)
public:
	FPGrowth(Transactions& transactions, const Support minSupport = 1, const uint32_t minPatternLen = 1, const uint32_t maxPatternLen = 0) :
		m_minSupport(minSupport),
		m_minPatternLen(minPatternLen),
		m_maxPatternLen(maxPatternLen),
		m_tree(nullptr),
		m_maxItemCnt(0),
		m_objs(1),
		m_pDataObjs(nullptr),
		m_pIdx2Id(nullptr),
		m_pId2Item(nullptr),
		m_memory(65536),
		m_pThreadMem(nullptr),
		m_pPattern(nullptr),
		m_pClosedDetect(nullptr),
		m_initTime()
	{
#ifdef ALL_PATTERN
#ifdef PERF_EXT_EXPANSION
		std::string mode = "All Frequent Itemsets with Perfect Extension Expansion";
#else
		std::string mode = "All Frequent Itemsets without Perfect Extension Expansion";
#endif
#else
		std::string mode = "Closed Itemsets";
#endif
		LOG_INFO << "  =====  FP-Growth (" << mode << ")  =====" << std::endl;

		DataBase db;
		FrequencyMap frequency;
		Timer timerSub;

		m_initTime.Start();

		frequency = getFrequency(transactions);

		LOG_INFO << "Items: " << frequency.size() << std::endl;
		LOG_INFO << "Transactions: " << transactions.size() << std::endl;

		LOG_VERBOSE << "Reducing and sorting transactions ... " << std::flush;
		timerSub.Start();


		do
		{
			reduceTransactions(transactions);
			frequency = getFrequency(transactions);
		} while (reduceItems(transactions, frequency));

		for (const Transaction& trans : transactions)
		{
			TransactionC tC;
			for (const ItemC& item : trans)
				tC.push_back(item);

			db.push_back(tC);
		}

		timerSub.Stop();
		LOG_VERBOSE << "Done after: " << timerSub << std::endl;
		LOG_VERBOSE << "Items: " << frequency.size() << std::endl;
		LOG_VERBOSE << "Transactions: " << transactions.size() << std::endl;

		timerSub.Start();
		m_maxItemCnt = frequency.size();

#ifdef USE_OPENMP
		//omp_set_num_threads(4);
		m_objs = omp_get_max_threads();
		LOG_INFO << "Number of Threads: " << m_objs << std::endl;
#endif
		m_pDataObjs = new DataObjs[m_objs]();
		m_pThreadMem = new FPNMemory[m_objs];

		for (int32_t i = 0; i < m_objs; i++)
		{
			m_pDataObjs[i].Init(m_maxItemCnt);
			m_pThreadMem[i].Init(65536);
		}

		m_pPattern = new Pattern[m_maxItemCnt];

		m_pIdx2Id = new uint32_t[m_maxItemCnt]();
		m_pId2Item = new ItemC[m_maxItemCnt]();

		m_pClosedDetect = new ClosedDetect(m_maxItemCnt);

		timerSub.Stop();
		LOG_VERBOSE << "Memory Allocation done after: " << timerSub << std::endl;

		FrequencyMapC F;

		for (TransactionC& transaction : db)
		{
			for (ItemRef& itemRef : transaction)
			{
				F.try_emplace(itemRef.item, std::make_shared<FrequencyRef>(F.size()));
				F[itemRef.item]->Inc(&itemRef);
			}
		}

		// This is currently required to be a RefPair to later update the index, allowing for proper sorting
		// TODO: Try to fully remove RefPairs
		std::vector<RefPair> fF;

		for (const RefPair& p : F)
		{
#ifdef DEBUG
			LOG_DEBUG << (char)p.first << ":" << p.second->support << std::endl;
#endif
			fF.push_back(p);
		}


		std::sort(std::begin(fF), std::end(fF), [](const RefPair& a, const RefPair& b) { return a.second->item() > b.second->item(); });

		std::sort(std::begin(fF), std::end(fF), [](const RefPair& a, const RefPair& b) { return a.second->support > b.second->support; });


		for (std::size_t i = 0; i < fF.size(); i++)
		{
			fF[i].second->SetIdx(i);
#ifdef DEBUG
			LOG_DEBUG << (char)fF[i].first << ":" << i << std::endl;
#endif
		}


		timerSub.Start();

		for (TransactionC& trans : db)
		{
			std::sort(std::begin(trans), std::end(trans), [](const ItemRef& a, const ItemRef& b) { return *a.pFRef > * b.pFRef; });
		}

		std::sort(std::begin(db), std::end(db), [](const TransactionC& a, const TransactionC& b)
				  {
					  std::size_t l = a.size() > b.size() ? b.size() : a.size();
					  for (std::size_t i = 0; i < l; i++)
					  {
						  if (a[i] != b[i])
						  {
							  if (a[i].Idx() > b[i].Idx())
								  return false;
							  else
								  return true;
						  }
					  }

					  if (a.size() == b.size())
						  return false;

					  if (a.size() > b.size())
						  return true;
					  else
						  return false;
				  });

		std::reverse(std::begin(db), std::end(db));

		std::vector<ItemC> known;

		std::sort(std::begin(fF), std::end(fF), [](const RefPair& a, const RefPair& b) { return a.second->Idx() < b.second->Idx(); });

		timerSub.Stop();
		LOG_VERBOSE << "Sorting done after: " << timerSub << std::endl;

		m_tree = new FPTree(fF, m_pIdx2Id, m_pId2Item, &m_memory);


		for (TransactionC& trans : db)
			m_tree->Add(trans, 1);

		m_initTime.Stop();
		LOG_VERBOSE << "Creating Tree done after: " << m_initTime << std::endl;

#ifdef DEBUG
		m_tree->PrintTree();
#endif

		LOG_VERBOSE << "Tree Cnt: " << m_tree->cnt << std::endl;
	}

	~FPGrowth()
	{
		delete[] m_pDataObjs;
		delete[] m_pThreadMem;
		delete[] m_pPattern;
		delete[] m_pIdx2Id;
		delete[] m_pId2Item;
		delete m_tree;
		delete m_pClosedDetect;
	}

	const uint32_t& GetMinPatternLen() const
	{
		return m_minPatternLen;
	}

	const uint32_t& GetMaxPatternLen() const
	{
		return m_maxPatternLen;
	}

	const std::size_t& GetItemCount() const
	{
		return m_maxItemCnt;
	}

	const ItemC* GetId2Item() const
	{
		return m_pId2Item;
	}


	const Pattern* Growth()
	{
		Timer t;
		t.Start();
		growthTop(m_tree);
		t.Stop();
		std::size_t cnt = 0;
		for (std::size_t i = 0; i < m_tree->cnt; i++)
			cnt += m_pPattern[i].GetCount();
		LOG_INFO << "\x1B[31mRuntime:\x1B[0m " << t + m_initTime << " - Frequent Item-Sets: " << cnt << std::endl;
		return m_pPattern;
	}

private:
	bool project(const int32_t& tId, FPTree* pDst, const FPTree* pSrc, const std::size_t& id)
	{
		memset(m_pDataObjs[tId].m_pSubs, 0, id * sizeof(Support));
		FPNode* pNode;
		FPNode* pAnc;

		for (pNode = pSrc->pHeads[id].list; pNode; pNode = pNode->succ)
		{
			for (pAnc = pNode->parent; pAnc->id != IDX_MAX; pAnc = pAnc->parent)
			{
				m_pDataObjs[tId].m_pSubs[pAnc->id] += pNode->support;
			}
		}

		Support n = 0;
		FPHead* pH;

		for (std::size_t i = 0; i < id; i++)
		{
			if (m_pDataObjs[tId].m_pSubs[i] < m_minSupport)
			{
				// Invalidate
				m_pDataObjs[tId].m_pSubs[i] = SUPP_MAX;
				continue;
			}

			pH = pDst->pHeads + n;
			pH->item = pSrc->pHeads[i].item;
			pH->support = m_pDataObjs[tId].m_pSubs[i];
			pH->list = nullptr;
			pH->pMemory = pSrc->pMemory;
			m_pDataObjs[tId].m_pSubs[i] = n++;
		}

		if (n == 0) return false;

		// As the Tree is reused for several iterations initialize cnt and root support here
		pDst->cnt = n;
		pDst->root.support = 0;

		std::size_t i;
		for (pNode = pSrc->pHeads[id].list; pNode; pNode = pNode->succ)
		{
			std::size_t* d = m_pDataObjs[tId].m_pMap + id;
			for (pAnc = pNode->parent; pAnc->id != IDX_MAX; pAnc = pAnc->parent)
			{
				if ((i = m_pDataObjs[tId].m_pSubs[pAnc->id]) != SUPP_MAX)
					*--d = i;
			}

			pDst->Add(d, (m_pDataObjs[tId].m_pMap + id) - d, pNode->support);
		}

		return true;
	}

	void beginPattern(const int32_t& tId)
	{
		if (!m_pDataObjs[tId].m_patternOpen)
		{
			m_pDataObjs[tId].m_patternOpen = true;
			std::memset(m_pDataObjs[tId].m_pAdded, 0, m_maxItemCnt);
			std::memset(m_pDataObjs[tId].m_pAddedPerfExt, 0, m_maxItemCnt);
			m_pDataObjs[tId].m_lastIDCnt = 0;
			m_pDataObjs[tId].m_perfExtIDCnt = 0;
#ifdef DEBUG
			LOG_DEBUG << std::endl << std::endl << "--- BEGIN PATTERN ---" << std::endl;
#endif
		}
	}

	bool addPatternElement(const int32_t& tId, const ItemID& item, const Support& supp)
	{
		if (supp < m_minSupport) return true;
		if (!m_pDataObjs[tId].m_patternOpen) return true;

		if (!m_pDataObjs[tId].m_pAddedPerfExt[item] && !m_pDataObjs[tId].m_pAdded[item])
		{
#ifdef DEBUG
			LOG_DEBUG << "itemID=" << item << "; item=" << (char)m_pId2Item[item] << "; supp=" << supp << std::endl;
#endif
			if (m_pClosedDetect->Add(item, supp) > 0)
			{
				m_pDataObjs[tId].m_pAdded[item] = true;
				m_pDataObjs[tId].m_pSupports[m_pDataObjs[tId].m_lastIDCnt] = supp;
				m_pDataObjs[tId].m_pLastID[m_pDataObjs[tId].m_lastIDCnt++] = item;


				if (m_pDataObjs[tId].m_lastIDCnt >= m_maxItemCnt) LOG_ERROR << "ERROR: lastIDCnt >= maxItemCnt" << std::endl;
			}
			else
				return false;
		}

		return true;
	}

	void addPerfectExt(const int32_t& tId, const ItemID& item, const Support& supp)
	{
		if (supp < m_minSupport) return;
		if (!m_pDataObjs[tId].m_patternOpen) return;

		if (!m_pDataObjs[tId].m_pAddedPerfExt[item] && !m_pDataObjs[tId].m_pAdded[item])
		{
			m_pDataObjs[tId].m_pAddedPerfExt[item] = true;
			m_pDataObjs[tId].m_pPerfExtIDs[m_pDataObjs[tId].m_perfExtIDCnt++] = item;
		}
	}

	void pp(Pattern& results, const ItemID* pIDs, const std::size_t& size, const std::size_t& pos, const std::size_t& minLen, PatternType* pBase, PatternType basePos, const Support& supp)
	{
		pBase[basePos++] = m_pId2Item[pIDs[pos]];
		for (std::size_t i = pos + 1; i < size; i++)
			pp(results, pIDs, size, i, minLen, pBase, basePos, supp);

		if (basePos >= minLen)
			results.AddPattern(basePos, supp, pBase);
	}

	void endLocalPattern(const int32_t& tId, const int64_t& pId, const ItemID& item)
	{
		UNUSED(item);
		if (m_pDataObjs[tId].m_patternOpen)
		{
			size_t combLength = m_pDataObjs[tId].m_lastIDCnt + m_pDataObjs[tId].m_perfExtIDCnt;
			if (combLength >= m_minPatternLen && (m_maxPatternLen == 0 || combLength <= m_maxPatternLen))
			{
				Support s = m_pDataObjs[tId].m_pSupports[m_pDataObjs[tId].m_lastIDCnt - 1];
#ifdef ALL_PATTERN
				for (std::size_t i = 0; i < m_pDataObjs[tId].m_lastIDCnt; i++)
					m_pDataObjs[tId].m_pPatternBase[i] = m_pDataObjs[tId].m_pLastID[i] | (static_cast<ItemID>(m_pDataObjs[tId].m_pSupports[i]) << 32);

#ifdef PERF_EXT_EXPANSION
				// TODO: Add maxPatternLength
				for (std::size_t i = 0; i < m_pDataObjs[tId].m_perfExtIDCnt; i++)
					pp(m_pPattern[pId], m_pDataObjs[tId].m_pPerfExtIDs, m_pDataObjs[tId].m_perfExtIDCnt, i, m_minPatternLen, m_pDataObjs[tId].m_pPatternBase, static_cast<ItemC>(m_pDataObjs[tId].m_lastIDCnt), s);

				if (m_pDataObjs[tId].m_lastIDCnt >= m_minPatternLen && (m_maxPatternLen == 0 || m_pDataObjs[tId].m_lastIDCnt <= m_maxPatternLen))
					m_pPattern[pId].AddPattern(static_cast<ItemC>(m_pDataObjs[tId].m_lastIDCnt), s, m_pDataObjs[tId].m_pPatternBase);

#else
				for (std::size_t i = m_pDataObjs[tId].m_lastIDCnt; i < m_pDataObjs[tId].m_lastIDCnt + m_pDataObjs[tId].m_perfExtIDCnt; i++)
					m_pDataObjs[tId].m_pPatternBase[i] = m_pDataObjs[tId].m_pPerfExtIDs[i - m_pDataObjs[tId].m_lastIDCnt] | (static_cast<ItemID>(0) << 32);
				m_pPattern[pId].AddPattern(static_cast<ItemC>(m_pDataObjs[tId].m_lastIDCnt + m_pDataObjs[tId].m_perfExtIDCnt), s, m_pDataObjs[tId].m_pPatternBase);
#endif
#else // Only extract closed pattern
				Support r = m_pClosedDetect->GetSupport();

#ifdef DEBUG
				LOG_DEBUG << "s=" << s << "; r=" << r << std::endl;
#endif
				if (r < s)
				{
					int32_t k = static_cast<int32_t>(m_pDataObjs[tId].m_lastIDCnt + m_pDataObjs[tId].m_perfExtIDCnt);

					for (std::size_t i = 0; i < m_pDataObjs[tId].m_lastIDCnt; i++)
						m_pDataObjs[tId].m_pPatternBase[i] = m_pId2Item[m_pDataObjs[tId].m_pLastID[i]];
					for (std::size_t i = m_pDataObjs[tId].m_lastIDCnt; i < m_pDataObjs[tId].m_lastIDCnt + m_pDataObjs[tId].m_perfExtIDCnt; i++)
						m_pDataObjs[tId].m_pPatternBase[i] = m_pId2Item[m_pDataObjs[tId].m_pPerfExtIDs[i - m_pDataObjs[tId].m_lastIDCnt]];

					std::memcpy(m_pDataObjs[tId].m_pCMem, m_pDataObjs[tId].m_pLastID, m_pDataObjs[tId].m_lastIDCnt * sizeof(ItemID));
					std::memcpy(m_pDataObjs[tId].m_pCMem + m_pDataObjs[tId].m_lastIDCnt, m_pDataObjs[tId].m_pPerfExtIDs, m_pDataObjs[tId].m_perfExtIDCnt * sizeof(ItemID));
#ifdef DEBUG
					for (std::size_t i = 0; i < m_pDataObjs[id].m_lastIDCnt + m_pDataObjs[id].m_perfExtIDCnt; i++)
						LOG_DEBUG << m_pDataObjs[id].m_pCMem[i] << " ";
					LOG_DEBUG << std::endl;
#endif

					m_pClosedDetect->Update(m_pDataObjs[tId].m_pCMem, k, s);
					m_pPattern[pId].AddPattern(static_cast<ItemC>(m_pDataObjs[tId].m_lastIDCnt + m_pDataObjs[tId].m_perfExtIDCnt), s, m_pDataObjs[tId].m_pPatternBase);
#ifdef DEBUG
					LOG_DEBUG << std::endl << std::endl;
#endif
				}
#endif
			}

#ifndef ALL_PATTERN
			m_pClosedDetect->Remove(1);
#endif

			// pre-decrement due to the post increment during the setting
			if (m_pDataObjs[tId].m_lastIDCnt > 0)
				m_pDataObjs[tId].m_pAdded[m_pDataObjs[tId].m_pLastID[--m_pDataObjs[tId].m_lastIDCnt]] = false;

			for (std::size_t i = 0; i < m_pDataObjs[tId].m_perfExtIDCnt; i++)
				m_pDataObjs[tId].m_pAddedPerfExt[m_pDataObjs[tId].m_pPerfExtIDs[i]] = false;
			m_pDataObjs[tId].m_perfExtIDCnt = 0;
		}
	}

	void EndPattern(const int32_t& tId, const ItemID& item)
	{
		if (m_pDataObjs[tId].m_patternOpen && m_pDataObjs[tId].m_pLastID[0] == item)
		{
#ifdef DEBUG
			LOG_DEBUG << "Pattern-End: " << (char)m_pId2Item[item] << "; id=" << item << std::endl;
#endif
			m_pDataObjs[tId].m_patternOpen = false;
		}
	}


	void growthTop(FPTree* pTree)
	{
		FPTree** ppDst = new FPTree*[m_objs]();

#ifdef WITH_SIG_TERM
		if (sigAborted()) throw(FPGException("CTRL-C abort"));
#endif

		if (pTree->cnt > 1)
		{
			for (int32_t i = 0; i < m_objs; i++)
			{
				ppDst[i] = new FPTree(m_tree->cnt - 1, m_tree->pIdx2Id, m_tree->pId2Item, &m_pThreadMem[i]);
				ppDst[i]->root.id = IDX_MAX;
				ppDst[i]->root.succ = nullptr;
				ppDst[i]->root.parent = nullptr;
			}
		}

#ifdef USE_OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
#ifdef ALL_PATTERN
		for (int64_t i = 0; i < (int64_t)pTree->cnt; i++)
#else
		for (int64_t i = pTree->cnt - 1; i > -1; i--)
#endif
		{
#ifdef USE_OPENMP
			int32_t tId = omp_get_thread_num();
#else
			int32_t tId = 0;
#endif
			FPHead* pH = pTree->pHeads + i;
			beginPattern(tId);
			if (!addPatternElement(tId, pH->item, pH->support))
				continue;

			FPNode* pNode = pH->list;
			if (pNode && !pNode->succ)
			{
				for (FPNode* pAnc = pNode->parent; pAnc->id != IDX_MAX; pAnc = pAnc->parent)
					addPerfectExt(tId, pTree->pHeads[pAnc->id].item, pTree->pHeads[pAnc->id].support);
			}
			else if (ppDst[tId])
			{
				if (project(tId, ppDst[tId], pTree, i))
					growth(tId, i, ppDst[tId]);
			}

			endLocalPattern(tId, i,  pH->item);

			EndPattern(tId, pH->item);
#ifdef ALL_PATTERN
			LOG_INFO << "\r" << i + 1 << " / " << pTree->cnt << " Done" << std::flush;
#else
			LOG_INFO << "\r" << pTree->cnt-i << " / " << pTree->cnt << " Done" << std::flush;
#endif
		}

		for (int32_t i = 0; i < m_objs; i++)
			if (ppDst[i]) delete(ppDst[i]);

		delete[] ppDst;

		LOG_INFO << std::endl;
	}

	void growth(const int32_t& tId, const int64_t& pId, FPTree* pTree)
	{
		FPTree* pDst = nullptr;
		FPHead* pH = nullptr;
		FPNode* pNode = nullptr;
		FPNode* pAnc = nullptr;

#ifdef WITH_SIG_TERM
		if (sigAborted()) throw(FPGException("CTRL-C abort"));
#endif

		if (pTree->cnt > 1)
		{
			pDst = new FPTree(m_tree->cnt - 1, m_tree->pIdx2Id, m_tree->pId2Item, &m_pThreadMem[tId]);
			pDst->root.id = IDX_MAX;
			pDst->root.succ = nullptr;
			pDst->root.parent = nullptr;
		}

		pTree->pMemory->PushState();

		for (int64_t i = pTree->cnt - 1; i > -1; i--)
		{
			pH = pTree->pHeads + i;
			if (!addPatternElement(tId, pH->item, pH->support))
				continue;

			pNode = pH->list;
			if (pNode && !pNode->succ)
			{
				for (pAnc = pNode->parent; pAnc->id != IDX_MAX; pAnc = pAnc->parent)
					addPerfectExt(tId, pTree->pHeads[pAnc->id].item, pTree->pHeads[pAnc->id].support);
			}
			else if (pDst)
			{
				if (project(tId, pDst, pTree, i))
					growth(tId, pId, pDst);
			}

			endLocalPattern(tId, pId, pH->item);
		}

		pTree->pMemory->PopState();
		if (pDst) delete(pDst);
	}

	FrequencyMap getFrequency(const Transactions& transactions)
	{
		FrequencyMap frequency;
		for (const Transaction& transaction : transactions)
		{
			for (const ItemC& item : transaction)
				frequency[item]++;
		}

		return frequency;
	}

	bool reduceItems(Transactions& transactions, FrequencyMap& frequency)
	{
		bool reduced = false;
		for (Transaction& trans : transactions)
		{
			for (Transaction::iterator it = std::begin(trans); it != std::end(trans); it++)
			{
				if (frequency[*it] < m_minSupport)
				{
					it = trans.erase(it);
					if (it != std::begin(trans))
						it--; // Decrement because erase returns the iterater after the deleted element which would be skipped due to the loop increment 
					reduced = true;

					if (it == std::end(trans)) break;
				}
			}
		}

		map_erase_if(frequency, [&minSupport = m_minSupport](const std::pair<ItemC, uint64_t>& p) { return p.second < minSupport; });

		return reduced;
	}

	void reduceTransactions(Transactions& transactions)
	{
		std::experimental::erase_if(transactions, [&minPatternLen = m_minPatternLen](const Transaction& t) { return t.size() < minPatternLen; });
	}




private:
	Support m_minSupport;
	uint32_t m_minPatternLen;
	uint32_t m_maxPatternLen;
	FPTree* m_tree;
	std::size_t m_maxItemCnt;
	int32_t m_objs;

	struct DataObjs
	{
		DISABLE_COPY_ASSIGN_MOVE(DataObjs)

		Support* m_pSubs;
		std::size_t* m_pMap;

		bool* m_pAdded;
		bool* m_pAddedPerfExt;
		ItemID* m_pLastID;
		ItemID* m_pPerfExtIDs;
		Support* m_pSupports;
		std::size_t m_lastIDCnt;
		std::size_t m_perfExtIDCnt;

		bool m_patternOpen;
		PatternType* m_pPatternBase;
#ifndef ALL_PATTERN
		ItemID* m_pCMem;
#endif
		DataObjs() :
			m_pSubs(nullptr),
			m_pMap(nullptr),
			m_pAdded(nullptr),
			m_pAddedPerfExt(nullptr),
			m_pLastID(nullptr),
			m_pPerfExtIDs(nullptr),
			m_pSupports(nullptr),
			m_lastIDCnt(0),
			m_perfExtIDCnt(0),
			m_patternOpen(false),
			m_pPatternBase(nullptr)
#ifndef ALL_PATTERN
			, m_pCMem(nullptr)
#endif
		{}

		~DataObjs()
		{
			delete[] m_pSubs;
			delete[] m_pMap;
			delete[] m_pAdded;
			delete[] m_pAddedPerfExt;
			delete[] m_pLastID;
			delete[] m_pPerfExtIDs;
			delete[] m_pSupports;
			delete[] m_pPatternBase;
#ifndef ALL_PATTERN
			delete[] m_pCMem;
#endif
		}

		void Init(const std::size_t& elements)
		{
			m_pSubs = new Support[elements]();
			m_pMap = new std::size_t[elements]();

			m_pAdded = new bool[elements]();
			m_pAddedPerfExt = new bool[elements]();
			m_pLastID = new ItemID[elements]();
			m_pPerfExtIDs = new ItemID[elements]();
			m_pSupports = new Support[elements]();

			m_pPatternBase = new PatternType[elements]();
#ifndef ALL_PATTERN
			m_pCMem = new ItemID[elements]();
#endif
		}
	};

	DataObjs* m_pDataObjs;

	uint32_t* m_pIdx2Id;
	ItemC* m_pId2Item;

	FPNMemory m_memory;
	FPNMemory* m_pThreadMem;
	Pattern* m_pPattern;

	ClosedDetect* m_pClosedDetect;
	Timer m_initTime;
};

void PostProcessing(const Pattern* pPattern, const std::size_t& maxC, const std::size_t& itemCount, const std::size_t& minPatternLength, const PatternType& winLen, const ItemC* pId2Item, std::vector<const PatternType*>& res)
{
	LOG_VERBOSE << "Result Filtering ... " << std::flush;
	Timer timer;
	timer.Start();

	for (int64_t i = itemCount - 1; i > -1; i--)
	{
		for (const PatternType* pPtr : pPattern[i])
		{
#ifdef WITH_SIG_TERM
			if (sigAborted()) throw(FPGException("CTRL-C abort"));
#endif
			const PatternType* pStart = pPtr + Pattern::DATA_IDX;
			const PatternType* pEnd = pStart + pPtr[Pattern::LEN_IDX];
			if (pPtr[Pattern::LEN_IDX] <= maxC)
			{
				if (std::any_of(pStart, pEnd, [&winLen, &pId2Item](const PatternType& i) { return ((pId2Item[i & 0xFFFFFFFF]) % winLen) == 0; }))
				{
					std::set<PatternType> v;
					std::transform(pStart, pEnd, std::inserter(v, std::begin(v)), [&winLen, &pId2Item](const PatternType& i) { return (pId2Item[i & 0xFFFFFFFF]) / winLen; });

					// TODO: Maybe remove vector here and find different way
					if (v.size() >= minPatternLength)
						res.push_back(pPtr);
				}
			}
		}
	}

	std::size_t cnt = 0;
	for (std::size_t i = 0; i < itemCount; i++)
		cnt += pPattern[i].GetCount();


	timer.Stop();
	LOG_VERBOSE << "Done after: " << timer << std::endl;
	LOG_INFO << "Reduction: " << cnt << " -> " << res.size() << std::endl;
}

void ClosedDetection(const std::size_t& itemCount, const ItemC* pId2Item, const std::vector<const PatternType*>& itemSets, std::vector<PatternPair>& closed)
{
	if (itemSets.empty())
	{
		LOG_VERBOSE << "No itemsets provided, skipping Closed Detection" << std::endl;
		return;
	}

	Timer timer;

	LOG_VERBOSE << "Closed Detection ... " << std::flush;

	timer.Start();

	ClosedDetect cd(itemCount);
	PatternType* pM = new PatternType[itemCount];
	PatternType* pPfExt = new PatternType[itemCount];
	PatternType* pItems = new PatternType[itemCount];
	bool* pAdded = new bool[itemCount]();

	ItemID base = ITEM_ID_MAX;
	int32_t k = 0;

	for (const PatternType* pp : itemSets)
	{
#ifdef WITH_SIG_TERM
		if (sigAborted()) throw(FPGException("CTRL-C abort"));
#endif
		int32_t pfExtCnt = 0;
		bool skip = false;

		if (base != pp[Pattern::DATA_IDX])
		{
			cd.Remove(k);
			base = pp[Pattern::DATA_IDX];
			std::memset(pAdded, 0, itemCount * sizeof(bool));
			k = 0;
		}

		for (int32_t i = 0; i < k; i++)
		{
			// TODO: Probably can start at 1 here
			if (pItems[i] != (pp[Pattern::DATA_IDX + i] & 0xFFFFFFFF))
			{
				for (int32_t j = i; j < k; j++)
				{
					pAdded[pItems[j]] = false;
					cd.Remove(1);
				}

				k = i;
				break;
			}
		}

		for (PatternType p = 0; p < pp[Pattern::LEN_IDX]; p++)
		{
			PatternType i = pp[Pattern::DATA_IDX + p];
			Support supp = i >> 32;
			ItemID item = i & 0xFFFFFFFF;
			if (supp == 0)
				pPfExt[pfExtCnt++] = item;
			else if (!pAdded[item])
			{
				if (cd.Add2(item, supp) > 0)
				{
					pItems[k++] = item;
					pAdded[item] = true;
				}
				else
				{
					skip = true;
					break;
				}
			}
		}

		if (skip) continue;

		Support s = static_cast<Support>(pp[Pattern::SUPP_IDX]);
		Support r = cd.GetSupport();

		if (static_cast<std::size_t>(k) + pfExtCnt == pp[Pattern::LEN_IDX])
		{
#ifdef DEBUG
			LOG_DEBUG << "s=" << s << "; r=" << r << std::endl;
#endif
			if (r < s)
			{
				std::memcpy(pM, pItems, k * sizeof(ItemID));
				std::memcpy(pM + k, pPfExt, pfExtCnt * sizeof(ItemID));

#ifdef DEBUG
				for (int32_t i = 0; i < k + pfExtCnt; i++)
					LOG_DEBUG << pM[i] << " ";
				LOG_DEBUG << std::endl;
#endif

				cd.Update(pM, k + pfExtCnt, s);

				PatternPair ppN;
				ppN.first.reserve(k + pfExtCnt);
				ppN.second = s;

				for (PatternType p = 0; p < pp[Pattern::LEN_IDX]; p++)
				{
					PatternType id = pp[Pattern::DATA_IDX + p];
					ppN.first.push_back(static_cast<PatternType>(pId2Item[id & 0xFFFFFFFF]));
				}

				closed.push_back(ppN);

#ifdef DEBUG
				LOG_DEBUG << std::endl << std::endl;
#endif
			}

			if (k > 0) pAdded[pItems[--k]] = false;
			cd.Remove(1);
		}
	}

	delete[] pM;
	delete[] pPfExt;
	delete[] pItems;
	delete[] pAdded;

	timer.Stop();
	LOG_VERBOSE << "Done after: " << timer << std::endl;
	LOG_INFO << "Closed Pattern: " << closed.size() << std::endl;
}

