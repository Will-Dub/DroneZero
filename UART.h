#ifndef UART_H
#define UART_H

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
#include <optional>
#include <iostream>
#include <algorithm>	

#include "message.cpp"

class UART {
private:
    int uart_filestream;
    std::mutex mtx;
    bool newDataReceived;
    std::vector<uint8_t> received_data;

public:
    UART(const char* device, int baud);

    ~UART();

    int write(const std::string& data);

    void flush();

    bool isNewDataReceived();

    std::vector<std::string> getReceivedLines();

    int writeMessage(const Message &message);

    std::optional<Message> getReceivedMessage();

    void listenForData();
};

#endif