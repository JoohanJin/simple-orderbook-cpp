#pragma once
#include <cstdint>
#include <limits>
#include <vector>

enum class OrderType{
    GoodTillCancel,   // The easiest form of the trade -> stays on the orderbook until filled or killed (manually)
    FillAndKill,  // fill as much as it can, and cancel the rest of it.
    FillOrKill,  // fill if we can, cancel otherwise.
    GoodForDay,  // Similar to the GTC -> need to cancel it based on the time.
    Market,  // gimme whatever price, but I want to be filled
};

enum class Side{
    Buy,
    Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using OrderIds = std::vector<OrderId>;

struct Constants
{
    static const Price InvalidPrice = std::numeric_limits<Price>::quiet_NaN();  // Not a number
};
