#include <iostream>
#include "OrderBook.h"

int main(){
    // TESTING  
    OrderBook orderbook;
    const OrderId orderId = 1;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));

    std::cout << orderbook.Size() << "\n";  // 1
    
    orderbook.CancelOrder(orderId);

    std::cout << orderbook.Size() << "\n";  // 0
    return 0;
}
