cmake_minimum_required(VERSION 3.20)
project(nasdaq_itch_lob LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -O3 -march=native -flto)

include_directories(include)

add_executable(itch_lob
    src/main.cpp
    src/order_book.cpp
)