# Simple Order Book C++

A robust, efficient, and clean C++ implementation of a limit order book with an integrated matching engine. This project demonstrates core concepts of low-latency trading systems, including order management, matching algorithms, and market data aggregation.

## Features

*   **Order Management**: Supports adding, canceling, and modifying orders.
*   **Order Types**:
    *   **GoodTillCancel (GTC)**: Remains in the order book until filled or manually canceled.
    *   **FillAndKill (FAK)**: Immediately fills as much as possible against existing orders and cancels the remainder.
*   **Matching Engine**: Automatically matches incoming buy and sell orders based on price-time priority.
*   **Level 2 Data**: Provides aggregated market depth (bids and asks) via `GetOrderInfos`.
*   **Smart Pointers**: Utilizes `std::shared_ptr` for safe and efficient memory management.
*   **Clean Architecture**: Modular design with separate classes for Orders, Trades, and the OrderBook itself.

## Getting Started

### Prerequisites

*   A C++ compiler supporting **C++20** (e.g., `g++`, `clang++`, MSVC).
*   (Optional) **CMake** (version 3.10 or higher) for building.

### Building the Project

#### Option 1: Using CMake (Recommended)

1.  Clone the repository:
    ```bash
    git clone https://github.com/JoohanJin/simple-orderbook-cpp.git
    cd simple-orderbook-cpp
    ```

2.  Create a build directory:
    ```bash
    mkdir build && cd build
    ```

3.  Configure and build:
    ```bash
    cmake ..
    make
    ```

4.  Run the executable:
    ```bash
    ./main
    ```

#### Option 2: Manual Compilation

You can compile the source files directly using `g++`:

```bash
g++ -std=c++20 main.cpp OrderBook.cpp Order.cpp OrderModify.cpp Trade.cpp -o main
./main
```

## Usage Example

Here is a simple example of how to use the `OrderBook` class (from `main.cpp`):

```cpp
#include <iostream>
#include "OrderBook.h"

int main(){
    // Create an OrderBook instance
    OrderBook orderbook;
    
    // Define an order ID
    const OrderId orderId = 1;

    // Create a Buy order: Limit Price 100, Quantity 10
    // OrderType: GoodTillCancel
    orderbook.AddOrder(std::make_shared<Order>(
        OrderType::GoodTillCancel, 
        orderId, 
        Side::Buy, 
        100, 
        10
    ));

    std::cout << "Order Book Size: " << orderbook.Size() << "
";  // Output: 1
    
    // Cancel the order
    orderbook.CancelOrder(orderId);

    std::cout << "Order Book Size: " << orderbook.Size() << "
";  // Output: 0
    return 0;
}
```

## Project Structure

*   **`OrderBook`**: The core class managing bids, asks, and order matching logic.
*   **`Order`**: Represents an individual order with price, quantity, side, and type.
*   **`OrderModify`**: Request object for modifying an existing order.
*   **`Trade`**: Represents a matched trade between a buyer and a seller.
*   **`OrderBookLevelInfos`**: Aggregated market depth data (Level 2).
*   **`Types.h`**: Common type definitions (`Price`, `Quantity`, `OrderId`, enums).

## License

This project is open-source and available under the [MIT License](LICENSE).
