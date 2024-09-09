#include "UART.h"

UART::UART(const char* device, int baud) : newDataReceived(false) {
    uart_filestream = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_filestream == -1) {
        spdlog::critical("Unable to open UART");
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

int UART::write(const std::string& data) {
    if (uart_filestream != -1) {
        int count = ::write(uart_filestream, data.c_str(), data.size());
        if (count <= 0) {
            return 1;
        }
    }
    return 0;
}

void UART::writeDataPacket(const DataPacket &data_packet){
    uint8_t buffer[256];
    size_t data_size = data_packet.serialize(buffer, sizeof(buffer));
    
    if (uart_filestream != -1) {
        int count = ::write(uart_filestream, buffer, data_size);
        if(count <= 0){
            return;
        }
    }

    return;
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

std::optional<DataPacket> UART::getReceivedDataPacket() {
    std::lock_guard<std::mutex> lock(mtx);
    while (receivedData.size() >= 10) {
        // Find the start marker
        auto start_it = std::find(receivedData.begin(), receivedData.end(), DataPacket::START_MARKER);
        if (start_it == receivedData.end()) {
            // No start marker found, clear all data if incomplete data packet
            receivedData.clear();
            return std::nullopt;
        }

        // Calculate the remaining data after the start marker
        size_t remaining_data = std::distance(start_it, receivedData.end());
        if (remaining_data < 12) {  // Minimum size check
            return std::nullopt;
        }

        // Extract the data length
        uint32_t dataLength;
        memcpy(&dataLength, &*(start_it + 7), sizeof(uint32_t));

        //Verify data length is in the range
        if (dataLength > MAX_BUFFER_SIZE) {
            // Data size exceeds buffer limit, remove all
            auto next_start_it = std::find(start_it + 1, receivedData.end(), DataPacket::START_MARKER);
            receivedData.erase(receivedData.begin(), next_start_it);
            return std::nullopt;
        }

        // Ensure we have the complete data packet
        size_t totalDataPacketSize = 12 + dataLength;
        if (remaining_data < totalDataPacketSize) {
            return std::nullopt;
        }

        // Check the end marker
        auto end_it = start_it + totalDataPacketSize - 1;
        if (*end_it != DataPacket::END_MARKER) {
            // Invalid end marker, discard data up to next start marker
            auto next_start_it = std::find(start_it + 1, receivedData.end(), DataPacket::START_MARKER);
            if (next_start_it != receivedData.end()) {
                receivedData.erase(receivedData.begin(), next_start_it); // Remove to next start marker
            } else {
                receivedData.clear(); // No more start marker found, remove all
            }
            return std::nullopt;
        }

        // Extract and deserialize the data packet
        std::vector<uint8_t> buffer(start_it, end_it + 1);
        DataPacket dataPacket;
        if (dataPacket.deserialize(buffer.data(), buffer.size())) {
            receivedData.erase(receivedData.begin(), end_it + 1); // Remove the processed data packet + end marker
            return dataPacket;
        } else {
            receivedData.erase(receivedData.begin(), start_it + 1); // Remove the invalid start
        }
    }

    return std::nullopt;
}

void UART::listenForData() {
    uint8_t buffer[256];
    while (true) {
        if (uart_filestream != -1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(uart_filestream, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;  // 100ms timeout
            
            int select_result = select(uart_filestream + 1, &read_fds, NULL, NULL, &timeout);
            if (select_result == -1) {
                spdlog::error("UART error during select");
                continue;
            } else if (select_result == 0) {
                // No data available, continue waiting
                continue;
            }

            // Read data in chunks
            int length = read(uart_filestream, buffer, sizeof(buffer));
            if (length > 0) {
                std::lock_guard<std::mutex> lock(mtx);
                receivedData.insert(receivedData.end(), buffer, buffer + length);
                newDataReceived = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
