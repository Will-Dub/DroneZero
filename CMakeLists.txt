cmake_minimum_required(VERSION 3.0)
project(dronezero)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(dronezero main.cpp)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(JPEG REQUIRED)

pkg_check_modules(BLUEZ REQUIRED bluez)

include_directories(${BLUEZ_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS})

target_sources(${PROJECT_NAME} PRIVATE
    UART.cpp
    UART.h
    message.cpp
    Camera.h
    Camera.cpp
    Bluetooth.h
    Bluetooth.cpp
)

target_link_libraries(dronezero
    Threads::Threads
    pthread
    ${BLUEZ_LIBRARIES}
    raspicam
    ${Boost_LIBRARIES}
    spdlog::spdlog
    ${JPEG_LIBRARIES}
)
