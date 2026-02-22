#include "Trade.h"

Trade::Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
    : bidTrade_{ bidTrade }  // performs copy -> so the value of param bidTrade is copied to the bidTrade_, which is totally different object.
    , askTrade_{ askTrade }
{   }
