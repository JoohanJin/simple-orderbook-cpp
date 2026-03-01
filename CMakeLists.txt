# which version of cmake I need!
cmake_minimum_required(VERSION 3.10)

# name of the project
project(OrderBook)

# let the compiler (Clang on the mac) to enable c++ 20
set(CMAKE_CXX_STANDARD 20)


add_executable(main  # Final Executable at the end
    main.cpp
    OrderBook.cpp
    Order.cpp
    OrderModify.cpp
    Trade.cpp
)
