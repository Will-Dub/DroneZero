cmake_minimum_required(VERSION 3.0)
project(droneZero)

set(CMAKE_CXX_STANDARD 14)

add_executable(droneZero main.cpp)

target_sources(${PROJECT_NAME} PRIVATE
    uart.cpp
    uart.h
)

find_package(Threads REQUIRED)

target_link_libraries(droneZero
    Threads::Threads
    pthread
)