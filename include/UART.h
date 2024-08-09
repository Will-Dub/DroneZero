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
#include <spdlog/spdlog.h>

#include "Message.h"

class UART {
private:
    int uart_filestream;
    std::mutex mtx;
    bool newDataReceived;
    std::vector<uint8_t> received_data;

public:
    UART(const char* device, int baud);

    ~UART();

    void flush();

    std::vector<std::string> getReceivedLines();

    std::optional<Message> getReceivedMessage();

    bool isNewDataReceived();

    void listenForData();

    int write(const std::string& data);

    int writeMessage(const Message &message);
};

#endif