#include "OrderBook.h"
#include <numeric> // for std::accumulate
#include <algorithm> // for std::min

bool OrderBook::CanMatch(Side side, Price price) const{
    if (side == Side::Buy){
        if (asks_.empty()){
            return false;
        }

        const auto & [bestAsk, _] = *asks_.begin();
        return price >= bestAsk;
    }
    else {
        if (bids_.empty()){
            return false;
        }

        const auto& [bestBid, _] = *bids_.begin();
        return price <= bestBid;
    }
}

Trades OrderBook::MatchOrders(){
    Trades trades;
    trades.reserve(orders_.size());

    while (true){
        if (bids_.empty() || asks_.empty()){
            break;
        }

        auto& [bidPrice, bids] = *bids_.begin();  // get the highest price to buy
        auto& [askPrice, asks] = *asks_.begin();  // get the lowest price to sell

        if (bidPrice < askPrice){
            // there is nothing to match
            break;
        }

        while (bids.size() && asks.size()){
            auto& bid = bids.front();
            auto& ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->isFilled()){
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            }

            if (ask->isFilled()){
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            }

            if (bids.empty()){
                bids_.erase(bidPrice);
            }

            if (asks.empty()){
                asks_.erase(askPrice);
            }

            trades.push_back(
                Trade{
                    TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
                    TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
                } 
            );
        }
    }
    if (!bids_.empty()){
        auto& [_, bids] = *bids_.begin();  // get the most probable bid Order list at a price
        auto& order = bids.front();  // get the first order
        if (order->GetOrderType() == OrderType::FillAndKill){
            CancelOrder(order->GetOrderId());
        }
    }

    if (!asks_.empty()){
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();  // get the first order
        if (order->GetOrderType() == OrderType::FillAndKill){
            CancelOrder(order->GetOrderId());
        }
    }
    return trades;
}

Trades OrderBook::AddOrder(OrderPointer order){
    // if the order already exists in the order book.
    if (orders_.find(order->GetOrderId()) != orders_.end()){
        return {};
    }

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice())){
        return {};
    }

    OrderPointers::iterator iterator;  // ? 

    if (order->GetSide() == Side::Buy){
        auto& orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);  // ?
    } else {
        auto& orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);  // ?
    }

    orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator}});
    return MatchOrders();
}

void OrderBook::CancelOrder(OrderId orderId){
    // the given orderId doe snot exist
    if (orders_.find(orderId) == orders_.end()){
        return;            
    }
    
    // need to erase the given orderId from the unordered_map: orders_.
    const auto orderIterator_ = orders_.find(orderId);
    if (orderIterator_ == orders_.end()) return;

    const auto& [orderId_, orderEntry_] = *orderIterator_;
    const auto order = orderEntry_.order_;  // pointer to the corresponding order object.
    const auto orderIterator = orderEntry_.location_;

    orders_.erase(orderId);
    
    // now need to erase the given order id from the map for bids and asks based on the OrderType.
    if (order->GetSide() == Side::Sell){
        auto price = order->GetPrice();
        auto& orders = asks_.at(price);
        orders.erase(orderIterator);  // remove the given order from the list of orders (sell) on the given price level.
        if (orders.empty()){  // remove this price from the asks entirely.
            asks_.erase(price);
        }
    }

    if (order->GetSide() == Side::Buy){
        auto price = order->GetPrice();
        auto& orders = bids_.at(price);
        orders.erase(orderIterator);
        if (orders.empty()){
            bids_.erase(price);
        }
    }
}

Trades OrderBook::ModifyOrder(OrderModify order){
    // it does not exist
    if (orders_.find(order.GetOrderId()) == orders_.end()){
        return {};
    }

    const auto orderIterator_ = orders_.find(order.GetOrderId());

    if (orderIterator_ == orders_.end()){  // ! it does not exist, just one more time for exception handling.
        return {};
    }

    const auto& [orderId_, orderEntry_] = *orderIterator_;

    const auto existingOrder = orderEntry_.order_;
    CancelOrder(existingOrder->GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}

std::size_t OrderBook::Size() const { return orders_.size(); }

OrderBookLevelInfos OrderBook::GetOrderInfos() const {
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(bids_.size());
    askInfos.reserve(asks_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders){
        return LevelInfo{price, std::accumulate(
            orders.begin(), orders.end(), (Quantity)0,
            [](std::size_t runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); } )};
    };

    for (const auto& [price, orders] : bids_){
        bidInfos.push_back(CreateLevelInfos(price, orders));
    }

    for (const auto& [price, orders]: asks_) {
        askInfos.push_back(CreateLevelInfos(price, orders));
    }

    return OrderBookLevelInfos{ bidInfos, askInfos };
}
