#include "OrderModify.h"

OrderModify::OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
    :orderId_{orderId}
    , side_{side}
    , price_{price}
    , quantity_{quantity}
{}

OrderPointer OrderModify::ToOrderPointer(OrderType type) const {
    // to initialize an instance
    return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
}
