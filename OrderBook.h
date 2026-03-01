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
#include <thread>

class OrderBook{
public:
    OrderBook();  // Default Constructor

    // explicitly telling the compiler to preven
    // Prevents creating a new book from an old one: OrderBook b2 = b1; will fail to compile
    OrderBook(const OrderBook&) = delete;

    // Prevents copying an existing book into another; b2 = b1; will fail to compile
    void operator=(const OrderBook&) = delete;

    // Prvent stealing resources from a temporary book to create a new one.
    OrderBook(OrderBook&&) = delete;

    // Prevents moving a temporary book into an existing one
    void operator=(const OrderBook&&) = delete;

    ~OrderBook();  // Destructor

    // Main public API of the class
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

    struct LevelData{
        /*
         * This is for book-keeping

         * Data at each price level
         * It is used for data_: Price to LevelData
         */
        Quantity quantity_{};
        Quantity count_{};

        enum class Action{
            Add,
            Remove,
            Match,
        };
    };

    // ! This is the data structure for Action at each price point.
    std::unordered_map<Price, LevelData> data_;

    // ! Sell -> asks_.begin() should be the the first greater price than the market price -> ascending
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    // ! Buy -> bids_.begin() should be the first less price than the market price -> descending
    std::map<Price, OrderPointers, std::greater<Price>> bids_;

    /*
     * Synchronization Method + Thread
     * Managing a background worker thread with a graceful shutdown mechanism and thread-safe data access.
     */
    // A mutual exclusion lock used to prevent race conditions when multiple threads access the order book.
    // The `mutable` keyword lets the varible to be locked and unlocked inside `const` member function.
    // we need to protect the data structure with lock, i.e., race condition
    mutable std::mutex orderMutex_;

    // A separate Os-level thread of execution
    // Used for a backgroudn loop that asynchronously removes expired, canceld or fully fille dorders.
    // cancel the GTD order at the end of the day.
    std::thread ordersPruneThread_;

    // A syncrhonizatio primitive that allows a thread to suspend execution until notified by another thread.
    std::condition_variable shutdownConditionVariable_;

    // a boolean flag that can be read and write simultaneoulsy by multiple threads wihtout causing a data race or requiring a mutex lock.
    // The main thread sets this to `true` when the application is closing, and the background thread checks it to know when to exit its loop safely.
    std::atomic<bool> shutdown_{ false };

    std::unordered_map<OrderId, OrderEntry> orders_;


    bool CanMatch(Side side, Price price) const;  // Getter
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;  // Getter
    Trades MatchOrders();

    void PruneGoodForDayOrders();
    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternals(OrderId orderId);

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);

};
