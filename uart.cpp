#include "uart.h"

UART::UART(const char* device, int baud) : newDataReceived(false) {
    uart_filestream = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_filestream == -1) {
        std::cerr << "Error - Unable to open UART." << std::endl;
    }

    struct termios options;
    tcgetattr(uart_filestream, &options);
    options.c_cflag = baud | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart_filestream, TCIFLUSH);
    tcsetattr(uart_filestream, TCSANOW, &options);
}

UART::~UART() {
    close(uart_filestream);
}

void UART::write(const std::string& data) {
    if (uart_filestream != -1) {
        int count = ::write(uart_filestream, data.c_str(), data.size());
        if (count < 0) {
            std::cerr << "UART TX error" << std::endl;
        }
    }
}

void UART::writeLine(const std::string& data) {
    if (uart_filestream != -1) {
        int count = ::write(uart_filestream, data.c_str(), data.size());
        if (count < 0) {
            std::cerr << "UART TX error" << std::endl;
        }

        // Write newline character
        const char newline = '\n';
        int count_newline = ::write(uart_filestream, &newline, 1);
        if (count_newline < 0) {
            std::cerr << "UART TX error" << std::endl;
        }
    }
}

void UART::flush() {
    if (uart_filestream != -1) {
        tcdrain(uart_filestream);
    }
}

bool UART::isNewDataReceived() {
    std::lock_guard<std::mutex> lock(mtx);
    return newDataReceived;
}

std::vector<std::string> UART::getReceivedLines() {
    std::vector<std::string> lines;
    std::lock_guard<std::mutex> lock(mtx);

    size_t pos = 0;
    while ((pos = receivedData.find('\n')) != std::string::npos) {
        lines.push_back(receivedData.substr(0, pos));
        receivedData.erase(0, pos + 1);
    }
    newDataReceived = !receivedData.empty();
    return lines;
}

void UART::listenForData() {
    char buffer[256];
    while (true) {
        if (uart_filestream != -1) {
            int length = read(uart_filestream, buffer, sizeof(buffer) - 1);
            if (length > 0) {
                std::lock_guard<std::mutex> lock(mtx);
                buffer[length] = '\0';
                receivedData += buffer;
                newDataReceived = true;
            }
        }
    }
}