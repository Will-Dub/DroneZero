find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(JPEG REQUIRED)

pkg_check_modules(BLUEZ REQUIRED bluez)

include_directories(${BLUEZ_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS})

add_executable(droneZero
    main.cpp
    UART.cpp
    Camera.cpp
    Bluetooth.cpp
)

target_include_directories(droneZero PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(droneZero
    Threads::Threads
    ${BLUEZ_LIBRARIES}
    raspicam
    spdlog::spdlog
    ${JPEG_LIBRARIES}
)