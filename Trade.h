#pragma once
#include "Types.h"
#include <vector>

struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade);

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
    /* data */
    TradeInfo bidTrade_;  // the data is OWNED by Trade Class -> so it is actually holding the obejct not the reference.
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;
