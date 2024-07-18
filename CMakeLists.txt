cmake_minimum_required(VERSION 3.0)
project(dronezero)

set(CMAKE_CXX_STANDARD 17)

add_executable(dronezero main.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BLUEZ REQUIRED bluez)

include_directories(${BLUEZ_INCLUDE_DIRS})

target_sources(${PROJECT_NAME} PRIVATE
    uart.cpp
    uart.h
    message.cpp
)

find_package(Threads REQUIRED)

target_link_libraries(dronezero
    Threads::Threads
    pthread
    ${BLUEZ_LIBRARIES}
    raspicam
)

find_package(Threads REQUIRED)
target_link_libraries(dronezero Threads::Threads)