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
        char buf[1024] = {0};
        while (is_client_connected) {
            memset(buf, 0, sizeof(buf));
            int bytes_read = recv(client_socket, buf, sizeof(buf), 0);
            if (bytes_read > 0) {
                spdlog::info("Bluetooth data received: {}", std::string(buf, bytes_read));

                DataPacket dataPacket;
                dataPacket.data = std::vector<uint8_t>(buf, buf + bytes_read);
                dataPacket.dataSize = bytes_read;
                dataPacket.type = DataType::IMAGE;

                std::lock_guard<std::mutex> lock(receive_queue_mutex);
                receive_queue.push(dataPacket);
            } else if (bytes_read < 0) {
                spdlog::warn("Bluetooth receive failed");
                is_client_connected = false;
                break;
            }
        }

        is_client_connected = false;
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

bool Bluetooth::sendData(DataPacket dataPacket){
    /*if(is_running && is_client_connected){
        spdlog::info("Sending data");

        // Send data via Bluetooth
        int bytes_sent = send(client_socket, dataPacket.data.data(), dataPacket.data.size(), 0);
        if (bytes_sent < 0) {
            spdlog::warn("Bluetooth send failed");
            is_client_connected = false;
            return false;
        }
    }*/

    return true;
}

void printBytes(uint32_t value) {
    unsigned char bytes[4];
    bytes[0] = (value >> 0) & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;

    spdlog::info("Bytes: {} {} {} {}", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void Bluetooth::sendImage(const std::vector<uint8_t>& bmpData) {
    DataPacket dataPacket;
    dataPacket.type = DataType::TEST;
    dataPacket.dataSize = bmpData.size();
    spdlog::info("-------------{}-------------", dataPacket.type);
    spdlog::info("-------------sizestorage: {}-------------", dataPacket.data.data());
    dataPacket.data = bmpData;

    send(client_socket, &dataPacket.type, sizeof(dataPacket.type), 0);
    printBytes(dataPacket.dataSize);
    send(client_socket, &dataPacket.dataSize, sizeof(dataPacket.dataSize), 0);

    std::vector<unsigned char> buffer;
    buffer.insert(buffer.end(), dataPacket.data.begin(), dataPacket.data.end());
    size_t totalBytesSent = 0;
    while (totalBytesSent < buffer.size()) {
        size_t bytesToSend = std::min(buffer.size() - totalBytesSent, size_t(1024));
        ssize_t bytesSent = send(client_socket, buffer.data() + totalBytesSent, bytesToSend, 0);
        if (bytesSent < 0) {
            spdlog::error("Failed to send data");
            return;
        }
        totalBytesSent += bytesSent;
    }
    //send(client_socket, dataPacket.data.data(), dataPacket.dataSize, 0);
}

bool Bluetooth::isRunning(){
    return is_running;
}

bool Bluetooth::isClientConnected(){
    return is_client_connected;
}