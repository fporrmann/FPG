#pragma once


#include "Defines.h"
#include "Logger.h"
#include "Types.h"
#include "ClosedTree.h"

#include <iostream>

class ClosedDetect
{
	DISABLE_COPY_ASSIGN_MOVE(ClosedDetect)

public:
	ClosedDetect(const std::size_t& size) :
		m_size(size),
		m_cnt(0),
		m_pTrees(nullptr)
	{
		m_pTrees = new ClosedTree[size + 1];
		m_pTrees[0].Init();
		m_pTrees[0].Add(nullptr, 0, 0);
		m_pTrees[0].SetItem(ITEM_MAX - 1);
	}

	~ClosedDetect()
	{
		delete[] m_pTrees;
	}

	int Add(ItemID item, Support supp)
	{
		UNUSED(item);
		UNUSED(supp);
#ifndef ALL_PATTERN
#ifdef DEBUG
		LOG_DEBUG << "CD_ADD: item=" << item << "; supp=" << supp << std::flush;
#endif
		ClosedTree* t = m_pTrees + m_cnt;

		if (!t || !(t->Valid()))
		{
			ClosedTree* prev = m_pTrees + (m_cnt - 1);
			t = prev->Project(t);
			if (!t) return -1;
		}

		t->Prune(item);
#ifdef DEBUG
		LOG_DEBUG << " max=" << t->GetMax() << std::flush;
#endif
		if (t->GetMax() >= supp)
		{
#ifdef DEBUG
			LOG_DEBUG << " Exit" << std::endl;
#endif
			return 0;
		}
		++m_cnt;
#ifdef DEBUG
		LOG_DEBUG << std::endl;
#endif
#endif
		return 1;
	}

	int Add2(ItemID item, Support supp)
	{
		UNUSED(item);
		UNUSED(supp);
#ifdef DEBUG
		LOG_DEBUG << "CD_ADD: item=" << item << "; supp=" << supp << std::flush;
#endif
		ClosedTree* t = m_pTrees + m_cnt;

		if (!t || !(t->Valid()))
		{
			ClosedTree* prev = m_pTrees + (m_cnt - 1);
			t = prev->Project(t);
			if (!t) return -1;
		}

		t->Prune(item);
#ifdef DEBUG
		LOG_DEBUG << " max=" << t->GetMax() << std::flush;
#endif
		if (t->GetMax() >= supp)
		{
#ifdef DEBUG
			LOG_DEBUG << " Exit" << std::endl;
#endif
			return 0;
		}
		++m_cnt;
#ifdef DEBUG
		LOG_DEBUG << std::endl;
#endif
		return 1;
	}

	int Update(ItemID* items, int32_t n, const Support& supp)
	{
		for (size_t i = 0; i < m_cnt; i++)
		{
			ClosedTree* t = &m_pTrees[i];
			while (*items != t->GetItem())
			{
				++items;
				--n;
			}

			t->Add(++items, --n, supp);
		}
		return 0;
	}


	void Remove(std::size_t n)
	{
#ifdef DEBUG
		LOG_DEBUG << "remove" << std::flush;
#endif
		for (n = (n < m_cnt) ? m_cnt - n : 0; m_cnt > n; m_cnt--)
		{
			if (m_pTrees[m_cnt].Valid())
			{
#ifdef DEBUG
				LOG_DEBUG << " item=" << m_pTrees[m_cnt].GetItem() << std::flush;
#endif
				m_pTrees[m_cnt].Clear();
			}
		}
#ifdef DEBUG
		LOG_DEBUG << std::endl;
#endif
	}

	Support GetSupport() const
	{
		return (m_cnt > 0) ? m_pTrees[m_cnt - 1].GetMax() : m_pTrees[0].GetSupport();
	}

private:
	std::size_t m_size;
	std::size_t m_cnt;
	ClosedTree* m_pTrees;
};
