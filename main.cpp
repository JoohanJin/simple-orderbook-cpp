#include <cstdint>
#include <vector>
#include <string>
#include <list>
#include <map>
#include <unordered_map>
#include <memory>
#include <stdexcept>

enum class OrderType{
    GoodTillCancel,
    FillAndKill
};

enum class Side{
    Buy,
    Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

/*
 * 
 */

struct LevelInfo{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

// Orderbook
class OrderBookLevelInfos{
public:
    OrderBookLevelInfos(
        const LevelInfos& bids,
        const LevelInfos& asks
    ): bids_{ bids }, asks_{ asks }{}

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};


// Order Objects
class Order {
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{orderType}
        , orderId_{orderId}
        , side_{side}
        , price_{price}
        , initialQuantity_{quantity}
        , remainingQuantity_{quantity}
    {}

    OrderId GetOrderId() const { return orderId_; }
    OrderType GetOrderType() const { return orderType_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }

    void Fill(Quantity quantity){
        /*
         * The lowest quantity between both orders is the quantity used to fill the order.
         */
        if (quantity > GetRemainingQuantity()){
            throw std::logic_error("Order (" + std::to_string(GetOrderId()) + ") cannot be filled for more than its remaining quantity.");
        }

        remainingQuantity_ -= quantity;
    }

    bool isFilled() const { return GetRemainingQuantity() == 0; }

private:
    OrderId orderId_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};


// Reference Expression
using OrderPointer = std::shared_ptr<Order>;  // Smart Pointer Type Alias
using OrderPointers = std::list<OrderPointer>;

// add, cancel, modify (cancel + replace)
class OrderModify{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        :orderId_{orderId}
        , side_{side}
        , price_{price}
        , quantity_{quantity}
    {}

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const {
        // to initialize an instance
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade{
private:
    /* data */
    TradeInfo bidTrade_;  // the data is OWNED by Trade Class -> so it is actually holding the obejct not the reference.
    TradeInfo askTrade_;
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_{ bidTrade }  // performs copy -> so the value of param bidTrade is copied to the bidTrade_, which is totally different object.
        , askTrade_{ askTrade }
    {   }

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }
};

using Trades = std::vector<Trade>;

class OrderBook{
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

    bool CanMatch(Side side, Price price) const{
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

    Trades MatchOrders(){
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

public:
    Trades AddOrder(OrderPointer order){
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

    void CancelOrder(OrderId orderId){
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

    Trades ModifyOrder(OrderModify order){
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

    std::size_t Size() const { return orders_.size(); }

    OrderBookLevelInfos GetOrderInfos() const {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(bids_.size());
        askInfos.reserve(asks_.size());

        for (const auto& [price, orders] : bids_) {
            Quantity totalQuantity = 0;
            for (const auto& order : orders) {
                totalQuantity += order->GetRemainingQuantity();
            }
            bidInfos.push_back({ price, totalQuantity });
        }

        for (const auto& [price, orders] : asks_) {
            Quantity totalQuantity = 0;
            for (const auto& order : orders) {
                totalQuantity += order->GetRemainingQuantity();
            }
            askInfos.push_back({ price, totalQuantity });
        }

        return OrderBookLevelInfos{ bidInfos, askInfos };
    }
};

int main(){
    return 0;
}
