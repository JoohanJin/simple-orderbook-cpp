#pragma once
#include <list>
#include <exception>
#include <string>
#include <memory>
#include <stdexcept>

#include "Types.h"

class Order {
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity);

    Order(OrderId orderId, Side side, Quantity quantity);

    OrderId GetOrderId() const { return orderId_; }
    OrderType GetOrderType() const { return orderType_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }

    void Fill(Quantity quantity);
    bool isFilled() const { return GetRemainingQuantity() == 0; }
    void ToGoodTillCancel(Price price);

private:
    OrderId orderId_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;
