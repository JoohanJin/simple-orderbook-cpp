#include <numeric> // for std::accumulate
#include <algorithm> // for std::min
#include <iterator>

#include "OrderBook.h"

/*
 * Important information about Orders
 * Two types of order in Order Book: BUY (bid) and SELL (ask)
 * For the person who wants to buy (bid), it is better to BUY it at lower price.
 * For the person who wants to sell (ask), it is better to SELL it at higher price.
 *
 * For the OrderBook, which needs to match BUY orders and SELL orders, it it is totally opposite.
 * To math orders, we need to match "the highest bid" (BUY) and "the lowest ask" (SELL)
 * Therefore, in order book: Best Bid Offer (BBO) and Best Ask Offer (BAO) is kinda counter-intuitive:
    * Best Bid Offer (BBO)
 */

bool OrderBook::CanMatch(Side side, Price price) const{
    if (side == Side::Buy){  // BUY
        if (asks_.empty()){
            return false;
        }

        // Get the best ask price
        const auto& [bestAsk, _] = *asks_.begin();
        // if the price to buy is equal to or higher than the best sell price -> then we can sell
        /*
         * Someone wants to sell stock A at 100 USDT
         * Other person shows up and says he wants to buy the stock at 101
         * then that "someone" will be happy to sell.
         */
        return price >= bestAsk;
    } else {  // SELL
        if (bids_.empty()){
            return false;
        }

        // Get the best bid price
        const auto& [bestBid, _] = *bids_.begin();
        // if the price to sell is equal to or lower than the best buy price
        /*
         * A wants to buy a at 100 USDT
         * B shows up and says he can sell a at 99 USDT
         * A would be happy to buy (since he got it cheaper than his expectation)
         */
        return price <= bestBid;
    }
}

Trades OrderBook::MatchOrders(){
    // Match the orders from bids and asks.
    Trades trades;  // This should be an empty trades
    trades.reserve(orders_.size());

    while (true){
        // If either of them is empty, then we cannot proceed
        if (bids_.empty() || asks_.empty()){
            break;
        }

        // get the highest price to buy -> the best bid (the highest price to buy)
        auto& [bidPrice, bids] = *bids_.begin();

        // get the lowest price to sell -> the best ask (the lowest price to sell)
        auto& [askPrice, asks] = *asks_.begin();

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

// ? [this] -> lambda capture -> It allows a lambda function to access the members and
// ? methods of the class it is currently inside.
/*
 * Constructor definition, member initializer list, and creates a thread running a lambda expresssion
 * `std::thread` in cpp initialization must have something to execute, a function, or a lambda expression.
    * as soon as std::thread is successfully initialized with an executable target, the thread immediately
    * starts running that target in the background
    * So, `ordersPruneThread_ { ...  }` is creating a new operating system thread and telling it to run whatever is inside
    * the braces.
 * `[this]` : A lambda function creates its own isoalted scope.
    * By default it cannot see any variables outside of itself.
    * The capture clause tlells the lambda what outside variabels it is allowed to access.
    * By putting `this` in the brackets, you are passing a ptr to the current isntances of the OrderBook class into the lambda.
 * `{ PruenGoodForDayOrders(); }`
 */
OrderBook::OrderBook() : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); }} {}
// Alternative
// OrderBook::OrderBook() : ordersPruneThread_{ [this] () { this->PruneGoodForDayOrders(); } } {}

OrderBook::~OrderBook() {
    shutdown_.store(true, std::memory_order_release);
    shutdownConditionVariable_.notify_all();
    if (ordersPruneThread_.joinable()) {
        ordersPruneThread_.join();
    }
}

Trades OrderBook::AddOrder(OrderPointer order){
    /*
     * FIFO for queue for each price level
     */
    std::scoped_lock ordersLock{ orderMutex_ };

    // if the order already exists in the order book.
    if (orders_.contains(order->GetOrderId())){
        return {};
    }

    // logic for market order
    if (order->GetOrderType() == OrderType::Market){
        if (order->GetSide() == Side::Buy && !asks_.empty()){
            const auto& [worstAsk, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAsk);
        } else if (order->GetSide() == Side::Sell && !bids_.empty()){
            const auto& [worstBid, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBid);
        } else
            return {};  // invalid Order
    }

    if (
        order->GetOrderType() == OrderType::FillAndKill &&
        !CanMatch(order->GetSide(), order->GetPrice())
    )
        return {};

    if (
        order->GetOrderType() == OrderType::FillOrKill &&
        !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity())
    )
        return {};

    OrderPointers::iterator iterator;  // ?

    if (order->GetSide() == Side::Buy){
        auto& orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
    } else {
        auto& orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
    }

    orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator }});

    OnOrderAdded(order);

    return MatchOrders();
}

Trades OrderBook::ModifyOrder(OrderModify order){
    OrderType orderType;

    {
        // RAII-style Lock Acquire
        std::scoped_lock ordersLock{ orderMutex_ };

        if (!orders_.contains(order.GetOrderId()))
            return {};

        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        orderType = existingOrder->GetOrderType();
    }  // Release the lock here

    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(orderType));
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

void OrderBook::PruneGoodForDayOrders(){
    using namespace std::chrono;
    const auto end = hours(16);

    while (true){
        const auto now = system_clock::now();
        const auto now_c = system_clock::to_time_t(now);
        std::tm now_parts;
        localtime_r(&now_c, &now_parts);

        if (now_parts.tm_hour >= end.count())
            now_parts.tm_mday += 1;

        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;

        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(100);

        {
            // RAII pattern for mutex management -> similar to python context management?

            // A classic pattern in multi-threaded C++ programming used for efficiently
            // putting a thread to sleep and waking it up
            std::unique_lock ordersLock { orderMutex_ };

            // std::conition_variable -> that is why unique_lock is used
            if (
                shutdown_.load(std::memory_order_acquire) ||  // check if the app is already trying to shut down. -> if yes -> return
                // std::memory_order_acquire -> a C++11 atomic memory ordering constraint for load operations that
                // ensures no subsequent memory reads or writes in the current thread are reordered before it.
                shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout  // go to sleep until 4:00 pm or until notified
                // `== std::cv_status::no_timeout` => when wait_for finishes, it returnsa  status telling you why it woke up.
                // if it returns std::cv_status::timeout -> Then it has hitted 16:00
                // if it returns std::cv_status::no_timeout -> then another thread explicitly notified the conitional_variable. (so interrupt/signal from external source)
                // `std::cv_status`: condition_variable used to indicate the result of a timed-wait on a conditional variable.
            )
                return;
        }

        OrderIds orderIds;

        {
            // RAII pattern for mutex management.

            std::scoped_lock ordersLock{ orderMutex_ };

            for (const auto& [_, entry] : orders_){
                const auto& [order, _] = entry;

                if (order->GetOrderType() != OrderType::GoodForDay)
                    continue;

                orderIds.push_back(order->GetOrderId());
            }
        }

        CancelOrders(orderIds);
    }
}

void OrderBook::CancelOrders(OrderIds orderIds){
    // acquire mutex for data.
    std::scoped_lock orderLock{ orderMutex_ };

    for (const auto& orderId : orderIds){
        CancelOrderInternals(orderId);
    }
}

void OrderBook:: CancelOrderInternals(OrderId orderId){
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

void OrderBook::OnOrderCancelled(OrderPointer order){
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void OrderBook::OnOrderAdded(OrderPointer order){
    UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void OrderBook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled){
    UpdateLevelData(price, quantity, LevelData::Action::Match);
}

void OrderBook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action){
    /*
     * What does it do?
     */
    auto& data = data_[price];  // Returns LevelData, [price: LevelData]

    // update data count
    data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1 : 0;

    // if match or remove -> then reduce the given quantity at the given Price
    if (action == LevelData::Action::Remove || action == LevelData::Action::Match){
        data.quantity_ -= quantity;
    } else {
        // LevelData::Action::Add
        data.quantity_ += quantity;
    }

    // If there is no order anymore -> then just delete it from the data_
    if (data.count_ == 0){
        data_.erase(price);
    }

    return;
}

bool OrderBook::CanFullyFill(Side side, Price price, Quantity quantity) const {
    /*
     * The CanFullyKill function checks if an incoming FOK order can be completely executed immediately without leaving any remaining quantity
     * on the book. If it cannot be filled entirely, a FOK order should be rejected and killed.
     *
     * It is trying to sum up the toal available volume that amatches the price criteria.
     * Because addition is commutative, the order in which we sum up the valid levelData_.quantity_ does not matter,
     * as long as we only sum up the quanitties at prices that are valid for the incoming order.
     *
     * need to check if the given quantity can be filled in the existing OrderBook Entries
     * we need to figure out if the volumen presented is enough to cover the given quantity.
     * if the given quantity can be filled -> Then return true; return false otherwise.
     *
     * if side == Side::BUY
     * -> then we need to go through the asks_
     *
     * else if side == Side::SELL
     * -> then we need to go through the bids_
     */

    // check if there is a quantity where we can match, at least one quantity.
    if (!CanMatch(side, price)){
        return false;
    }

    // The optional contained value -> the value may or may not be present
    // ? Threshold is the price for BBO/BAO => The starting point of prices we are allowed to consider.
    // ? If I am a Buyer, I want to buy from the lowest asks going upwards.
    // ? If I am a Seller, I want to sell from the highest bids going downwards.
    std::optional<Price> threshold;

    if (side == Side::Buy){  // Buy -> get the best sell (asks) price
        // ! _ does not give it any special magical properties -> so the OrderPointers would be "Copied" so it is better to use reference rather than the value copied.
        const auto& [askPrice, _] = *asks_.begin();
        threshold = askPrice;
    } else {  // Sell -> get the best buy (bids) price
        const auto& [bidPrice, _] = *bids_.begin();
        threshold = bidPrice;
    }

    // ! Syntax: Reference -> read only refernece to the elements already sitting inside the data_ map.
    for (const auto& [levelPrice, levelData] : data_){
        // check if the given condition is even possible
        if (
            threshold.has_value() &&  // if the threshold value is present
            // threshold is the BAO, if the side is BUY
            // threshold is the BBO, if the side is SELL
            (
                (side == Side::Buy && threshold.value() > levelPrice) ||  // if the random levelPrice is less than the BAO, then it is on Bids side
                (side == Side::Sell && threshold.value() < levelPrice)  // if the random levelPRice is greater than the BBO, then it is on Asks side
            )
        )
            continue;

        // check if the given condition is desired by the client.
        if (  // If the threshold value is not present
            // price is the limit price, in the given order.
            (side == Side::Buy && levelPrice > price) ||  // For BUY, if the levelPrice is greter than limitPrice, then I would not buy.
            (side == Side::Sell && levelPrice < price)  // For SELL, if the levelPrice is less than limitPrice, then I would not sell.
        )
            continue;

        // if the quantity to be filled can be filled with the quantity at the current priceLevel, then just return true
        if (quantity <= levelData.quantity_)
            return true;

        // if the quantity on the given levelPrice is not enough, then just get the difference.
        quantity -= levelData.quantity_;
    }

    return false;
}

void OrderBook::CancelOrder(OrderId orderId){
    std::scoped_lock ordersLock{ orderMutex_ };

    CancelOrderInternals(orderId);
}
