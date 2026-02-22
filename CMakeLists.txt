cmake_minimum_required(VERSION 3.10)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)

add_executable(main
    main.cpp
    OrderBook.cpp
    Order.cpp
    OrderModify.cpp
    Trade.cpp
)
