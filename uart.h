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

#ifndef UART_H
#define UART_H

class UART {
private:
    int uart_filestream;
    std::mutex mtx;
    bool newDataReceived;
    std::string receivedData;

public:
    UART(const char* device, int baud);

    ~UART();

    void write(const std::string& data);

    void writeLine(const std::string& data);

    void flush();

    bool isNewDataReceived();

    std::vector<std::string> getReceivedLines();

    void listenForData();
};

#endif