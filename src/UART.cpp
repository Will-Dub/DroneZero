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

int UART::writeMessage(const Message &data) {
    uint8_t buffer[256];
    size_t data_size = data.serialize(buffer, sizeof(buffer));
    
    if (uart_filestream != -1) {
        int count = ::write(uart_filestream, buffer, data_size);
        if(count <= 0){
            return 1;
        }
    }

    return 0;
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

std::optional<Message> UART::getReceivedMessage() {
    std::lock_guard<std::mutex> lock(mtx);

    const size_t MAX_BUFFER_SIZE = 2000; // Maximum size of received_data buffer

    while (received_data.size() >= 6) {
        // Find the start marker
        auto start_it = std::find(received_data.begin(), received_data.end(), Message::START_MARKER);
        if (start_it == received_data.end()) {
            // No start marker found, clear all data if incomplete message
            received_data.clear();
            return std::nullopt;
        }

        // Calculate the remaining data after the start marker
        size_t remaining_data = std::distance(start_it, received_data.end());
        if (remaining_data < 6) {  // Minimum size check
            return std::nullopt;
        }

        // Extract the message length
        uint16_t message_length;
        memcpy(&message_length, &*(start_it + 1), sizeof(uint16_t));

        //Verify message length is in the range
        if (message_length > MAX_BUFFER_SIZE) {
            // Message size exceeds buffer limit, discard all data
            auto next_start_it = std::find(start_it + 1, received_data.end(), Message::START_MARKER);
            received_data.erase(received_data.begin(), next_start_it);
            return std::nullopt;
        }

        // Ensure we have the complete message
        size_t total_message_size = 6 + message_length;
        if (remaining_data < total_message_size) {
            return std::nullopt;
        }

        // Check the end marker
        auto end_it = start_it + total_message_size - 1; // Adjust for inclusive end marker check
        if (*end_it != Message::END_MARKER) {
            // Invalid end marker, discard data up to next start marker
            auto next_start_it = std::find(start_it + 1, received_data.end(), Message::START_MARKER);
            if (next_start_it != received_data.end()) {
                received_data.erase(received_data.begin(), next_start_it); // Discard up to next start marker
            } else {
                received_data.clear(); // No more start marker found, clear all data
            }
            return std::nullopt;
        }

        // Extract and deserialize the message
        std::vector<uint8_t> buffer(start_it, end_it + 1);
        Message message;
        if (message.deserialize(buffer.data(), buffer.size())) {
            received_data.erase(received_data.begin(), end_it + 1); // Remove the processed message including the end marker
            return message;
        } else {
            received_data.erase(received_data.begin(), start_it + 1); // Move past the invalid start marker
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
                received_data.insert(received_data.end(), buffer, buffer + length);
                newDataReceived = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
