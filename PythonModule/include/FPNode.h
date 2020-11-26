#pragma once

#include "Types.h"
#include "Logger.h"

#include <string>
#include <iostream>
#include <sstream>

struct FPNode
{
	std::size_t id;
	Support support;
	struct FPNode* parent;
	struct FPNode* succ;
#ifdef DEBUG
	ItemC item;
#endif

	FPNode() :
		id(std::numeric_limits<size_t>::max()),
		support(0),
		parent(nullptr),
		succ(nullptr)
#ifdef DEBUG
		, item(0)
#endif
	{}

#ifdef DEBUG
	~FPNode()
	{
		parent = nullptr;
		succ = nullptr;
	}
#endif

	void SetFreeNode(FPNode* pNode)
	{
		parent = pNode;
	}

	FPNode* GetFreeNode() const
	{
		return parent;
	}

	void PrintTree(const std::string& prefix = "") const
	{
		const std::string space = "    ";
		const std::string connectSpace = u8"│   ";
		const bool isLast = parent == nullptr;

		LOG_VERBOSE << prefix;
		LOG_VERBOSE << (isLast ? u8"└──" : u8"├──");
		// print the value of the node
#ifdef DEBUG
		LOG_DEBUG << (char)item << ":" << support << std::endl;
#endif

		// enter the next tree level - left and right branch
		if (parent != nullptr)
			parent->PrintTree(prefix + (isLast ? space : connectSpace));
		if (succ != nullptr)
			succ->PrintTree(prefix/* + (isLast ? space : connectSpace)*/);
	}

	friend std::ostream& operator<<(std::ostream& os, const FPNode& rhs)
	{
		os << "id=" << rhs.id << "; support=" << rhs.support << "; parent=" << rhs.parent << "; succ=" << rhs.succ;
		return os;
	}

};
