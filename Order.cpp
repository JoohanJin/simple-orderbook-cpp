#include "Order.h"

Order::Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
    : orderType_{orderType}
    , orderId_{orderId}
    , side_{side}
    , price_{price}
    , initialQuantity_{quantity}
    , remainingQuantity_{quantity}
{}

void Order::Fill(Quantity quantity){
    /*
        * The lowest quantity between both orders is the quantity used to fill the order.
        */
    if (quantity > GetRemainingQuantity()){
        throw std::logic_error("Order (" + std::to_string(GetOrderId()) + ") cannot be filled for more than its remaining quantity.");
    }

    remainingQuantity_ -= quantity;
}
