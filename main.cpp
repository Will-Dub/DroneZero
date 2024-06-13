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
#include "uart.h"

void uartListenerTask(UART* uart) {
    uart->listenForData();
}

void uartSenderTask(UART* uart) {
    while (true) {
        std::string message = "Hello";
        uart->writeLine(message);
        uart->flush();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    UART uart("/dev/ttyS0", B9600);

    std::thread listenerThread(uartListenerTask, &uart);
    std::thread senderThread(uartSenderTask, &uart);

    while (true) {
        if (uart.isNewDataReceived()) {
            std::vector<std::string> lines = uart.getReceivedLines();
            for (const std::string& line : lines) {
                std::cout << "Received: " << line << std::endl;
                // Additional processing logic for each line
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    listenerThread.join();
    senderThread.join();
    return 0;
}