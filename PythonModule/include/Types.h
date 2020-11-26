#pragma once

#include "Defines.h"

#include <stdint.h>
#include <limits>
#include <vector>
#include <map>

using ItemC = uint32_t;
using Support = uint32_t;
using ItemID = std::size_t;

using Transaction = std::vector<ItemC>;
using Transactions = std::vector<Transaction>;
using FrequencyMap = std::map<ItemC, Support>;

const std::size_t IDX_MAX = std::numeric_limits<std::size_t>::max();
const Support SUPP_MAX = std::numeric_limits<Support>::max();
const ItemC ITEM_MAX = std::numeric_limits<ItemC>::max();
const ItemID ITEM_ID_MAX = std::numeric_limits<ItemID>::max();

using ItemOccurence = std::pair<ItemC, Support>;
using ItemOccurences = std::vector<ItemOccurence>;

using PatternType = ItemID;
using PatternVec = std::vector<PatternType>;
using PatternPair = std::pair<PatternVec, Support>;

