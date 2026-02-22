#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional> // for std::less, std::greater

#include "Types.h"
#include "Order.h"
#include "OrderModify.h"
#include "Trade.h"
#include "OrderBookLevelInfos.h"

class OrderBook{
public:
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    std::size_t Size() const;
    OrderBookLevelInfos GetOrderInfos() const;

private:
    // List or Vectors -> 
    // Maps or Unordered Maps -> bids and asks
    struct OrderEntry{
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location_;
    };

    // ! Sell -> asks_.begin() should be the the first greater price than the market price -> ascending
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    // ! Buy -> bids_.begin() should be the first less price than the market price -> descending
    std::map<Price, OrderPointers, std::greater<Price>> bids_;

    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
};
