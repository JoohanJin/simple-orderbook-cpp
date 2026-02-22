#pragma once
#include "Types.h"
#include <vector>

struct LevelInfo{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;
