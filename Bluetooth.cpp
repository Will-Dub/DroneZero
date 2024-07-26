#include "Bluetooth.h"

Bluetooth::Bluetooth()
    : is_running(false), is_client_connected(false), client_socket(-1) {}

Bluetooth::~Bluetooth() {
    stop();
}

void Bluetooth::startServer() {
    is_running = true;
    std::thread server_thread(&Bluetooth::bluetoothServerTask, this);
    server_thread.detach();
}

void Bluetooth::stop() {
    is_running = false;
}

void Bluetooth::bluetoothServerTask() {
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int server_socket;
    socklen_t opt = sizeof(rem_addr);

    // Allocate socket
    server_socket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (server_socket < 0) {
        spdlog::critical("Bluetooth socket creation failed");
        return;
    }

    // Bind bluetooth socket
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = {{0, 0, 0, 0, 0, 0}};
    loc_addr.rc_channel = (uint8_t)1;
    if (bind(server_socket, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        spdlog::critical("Bluetooth bind failed");
        close(server_socket);
        return;
    }

    // Put bluetooth in listening mode
    if (listen(server_socket, 1) < 0) { // Max 1 client
        spdlog::critical("Bluetooth listen failed");
        close(server_socket);
        return;
    }

    // Main loop
    while (is_running) {
        // Accept connection
        client_socket = accept(server_socket, (struct sockaddr *)&rem_addr, &opt);

        if(!is_running){
            break;
        }

        if (client_socket < 0) {
            spdlog::warn("Bluetooth accept failed");
            continue;
        }

        ba2str(&rem_addr.rc_bdaddr, buf);
        spdlog::info("New bluetooth connection from {}", buf);

        // Set client as connected
        is_client_connected = true;

        //Receive data
        while (is_client_connected) {
            // Read DataType (1 byte)
            uint8_t typeOrdinal;
            if (recv(client_socket, &typeOrdinal, sizeof(typeOrdinal), 0) <= 0) {
                spdlog::error("Bluetooth receive failed");
                is_client_connected = false;
                break;
            }
            DataType dataType = static_cast<DataType>(typeOrdinal);

            // Read Size (4 bytes) little endian
            uint32_t dataSize;
            if (recv(client_socket, &dataSize, sizeof(dataSize), 0) <= 0) {
                spdlog::error("Bluetooth receive failed");
                is_client_connected = false;
                break;
            }

            // Read Data
            std::vector<uint8_t> data(dataSize);
            size_t totalBytesReceived = 0;
            while (totalBytesReceived < dataSize) {
                ssize_t bytesReceived = recv(client_socket, data.data() + totalBytesReceived, dataSize - totalBytesReceived, 0);
                if (bytesReceived <= 0) {
                    spdlog::error("Failed to receive data");
                    is_client_connected = false;
                    break; // Return an empty DataPacket
                }
                totalBytesReceived += bytesReceived;
            }

            if (totalBytesReceived == dataSize) {
                // Handle the received data
                DataPacket dataPacket;
                dataPacket.type = DataType::IMAGE;
                dataPacket.dataSize = dataSize;
                dataPacket.data = data;
                std::lock_guard<std::mutex> lock(receive_queue_mutex);
                receive_queue.push(dataPacket);
            } else {
                spdlog::error("Bluetooth incomplete read");
                is_client_connected = false;
                break;
            }
        }
    }

    // Close socket
    close(server_socket);
}

std::unique_ptr<DataPacket> Bluetooth::getReceivedData(){
    std::lock_guard<std::mutex> lock(receive_queue_mutex);
    if (!receive_queue.empty()) {
        std::unique_ptr<DataPacket> received_data = std::make_unique<DataPacket>(receive_queue.front());
        receive_queue.pop();

        return received_data;
    }
    return nullptr;
}

void Bluetooth::sendData(DataPacket dataPacket) {
    if(send(client_socket, &dataPacket.type, sizeof(dataPacket.type), 0) < 0){
        spdlog::error("Failed to send data");
        is_client_connected = false;
        return;
    }
    
    if(send(client_socket, &dataPacket.dataSize, sizeof(dataPacket.dataSize), 0) < 0){
        spdlog::error("Failed to send data");
        is_client_connected = false;
        return;
    }

    std::vector<unsigned char> buffer;
    buffer.insert(buffer.end(), dataPacket.data.begin(), dataPacket.data.end());
    size_t totalBytesSent = 0;
    while (totalBytesSent < buffer.size()) {
        size_t bytesToSend = std::min(buffer.size() - totalBytesSent, size_t(1024));
        ssize_t bytesSent = send(client_socket, buffer.data() + totalBytesSent, bytesToSend, 0);
        if (bytesSent < 0) {
            spdlog::error("Failed to send data");
            is_client_connected = false;
            return;
        }
        totalBytesSent += bytesSent;
    }
}

bool Bluetooth::isRunning(){
    return is_running;
}

bool Bluetooth::isClientConnected(){
    return is_client_connected;
}