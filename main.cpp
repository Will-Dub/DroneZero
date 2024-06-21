#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include "message.cpp"
#include "uart.h"

/**
 * Listen for new data on the uart and store it
 */
void uartListenerTask(UART* uart) {
    uart->listenForData();
}

void uartSenderTask(UART* uart) {
    int count = 0;
    while (true) {
        std::string message = "Salut " + std::to_string(count);
        count++;

        std::cout << "Out: " << message << std::endl;

        uart->writeLine(message);
        uart->flush();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    //-------------------------------------------
    //Variable declaration
    UART uart("/dev/ttyS0", B9600);

    //-------------------------------------------
    //Start threads
    std::thread listenerThread(uartListenerTask, &uart);
    std::thread senderThread(uartSenderTask, &uart);

    //-------------------------------------------
    //Main loop
    while (true) {
        //-------------------------------------------
        //Handle new data from the pico
        if (uart.isNewDataReceived()) {
            auto start = std::chrono::high_resolution_clock::now();
            std::optional<Message> receivedMessage = uart.getReceiveMessage();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            std::cout << "Temps: " << duration_micros << " sec" << std::endl;
            
            if (receivedMessage.has_value()) {
                std::cout << "Lat: " << receivedMessage.value().data.sensor_data.gps_latitude << std::endl;
                std::cout << "Long: " << receivedMessage.value().data.sensor_data.gps_longitude << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    //-------------------------------------------
    //Wait for thread to finish
    listenerThread.join();
    senderThread.join();

    return 0;
}