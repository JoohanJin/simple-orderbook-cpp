#pragma once
#include "Order.h"

class OrderModify{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity);

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const;

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};
