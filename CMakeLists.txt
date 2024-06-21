cmake_minimum_required(VERSION 3.0)
project(dronezero)

set(CMAKE_CXX_STANDARD 17)

add_executable(dronezero main.cpp)

target_sources(${PROJECT_NAME} PRIVATE
    uart.cpp
    uart.h
    message.cpp
)

find_package(Threads REQUIRED)

target_link_libraries(dronezero
    Threads::Threads
    pthread
)