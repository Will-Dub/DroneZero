#include "Bluetooth.h"

Bluetooth::Bluetooth()
    : is_running(false), is_client_connected(false), client_socket(-1) {}

Bluetooth::~Bluetooth() {
    stop();
    clearSendQueue();
}

/*
* Start bluetooth
*/
void Bluetooth::startServer() {
    is_running = true;
    std::thread server_thread(&Bluetooth::bluetoothServerTask, this);
    server_thread.detach();
}

/*
* Stop all bluetooth thread
*/
void Bluetooth::stop() {
    is_running = false;
    send_queue_condition.notify_all();
    clearSendQueue();
}

/*
* Task that handle sending data
*/
void Bluetooth::bluetoothSendTask(int client_socket) {
    while (is_running && is_client_connected) {
        DataPacket dataPacket;

        {
            std::unique_lock<std::mutex> lock(send_queue_mutex);
            send_queue_condition.wait(lock, [this] { return !send_queue.empty() || !is_running; });

            if (!is_running) {
                break;
            }

            if (send_queue.empty()) {
                continue;
            }

            dataPacket = send_queue.front();
            send_queue.pop();
        }

        spdlog::info("Sending data, remaining: {}", send_queue.size());

        // Send the data
        //Send the type(1 byte)
        if(send(client_socket, &dataPacket.type, sizeof(dataPacket.type), 0) < 0){
            spdlog::error("Failed to send data");
            is_client_connected = false;
            return;
        }
        
        //Send the size(4 byte)
        if(send(client_socket, &dataPacket.dataSize, sizeof(dataPacket.dataSize), 0) < 0){
            spdlog::error("Failed to send data");
            is_client_connected = false;
            return;
        }

        //Send the data(dataSize byte)
        const size_t maxBufferSize = 8192;
        const std::vector<uint8_t>& data = dataPacket.data;
        size_t totalBytesSent = 0;
        while (totalBytesSent < data.size()) {
            size_t bytesToSend = std::min(data.size() - totalBytesSent, maxBufferSize);
            ssize_t bytesSent = send(client_socket, data.data() + totalBytesSent, bytesToSend, 0);
            if (bytesSent < 0) {
                spdlog::error("Failed to send data");
                is_client_connected = false;
                return;
            }
            totalBytesSent += bytesSent;
        }
    }
}

/*
* Main bluetooth task(accept and handle the connection)
*/
void Bluetooth::bluetoothServerTask() {
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
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
    // Max 1 client
    if (listen(server_socket, 1) < 0) {
        spdlog::critical("Bluetooth listen failed");
        close(server_socket);
        return;
    }

    // Main loop
    while (is_running) {
        // Accept connection
        client_socket = accept(server_socket, (struct sockaddr *)&rem_addr, &opt);
        if (client_socket < 0) {
            spdlog::warn("Bluetooth accept failed");
            continue;
        }

        char addr_str[18];
        ba2str(&rem_addr.rc_bdaddr, addr_str);
        spdlog::info("New bluetooth connection from {}", addr_str);

        // Set client as connected
        is_client_connected = true;

        std::thread send_thread(&Bluetooth::bluetoothSendTask, this, client_socket);

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
                    break;
                }
                totalBytesReceived += bytesReceived;
            }

            if (totalBytesReceived == dataSize) {
                // Handle the received data
                DataPacket dataPacket;
                dataPacket.type = dataType;
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

        //Stop the send thread
        send_queue_condition.notify_all();
        send_thread.join();

        //Clear the send queue
        clearSendQueue();
    }

    // Close socket
    close(server_socket);
}

/*
* Get the data from the receive queue if any
*/
std::unique_ptr<DataPacket> Bluetooth::getReceivedData(){
    std::lock_guard<std::mutex> lock(receive_queue_mutex);
    if (!receive_queue.empty()) {
        std::unique_ptr<DataPacket> received_data = std::make_unique<DataPacket>(receive_queue.front());
        receive_queue.pop();

        return received_data;
    }
    return nullptr;
}

/*
* Sends a datapacket to the client
*/
void Bluetooth::sendData(DataPacket dataPacket){
    if(is_client_connected){
        {
            std::lock_guard<std::mutex> lock(send_queue_mutex);
            send_queue.push(dataPacket);
        }
        send_queue_condition.notify_one();
    }

    return;
}

/*
* Return if the bluetooth thread is running
*/
bool Bluetooth::isRunning(){
    return is_running;
}

/*
* Returns if a client is connected
*/
bool Bluetooth::isClientConnected(){
    return is_client_connected;
}

/*
* Clears the queue of data to send
*/
void Bluetooth::clearSendQueue() {
    std::lock_guard<std::mutex> lock(send_queue_mutex);
    while (!send_queue.empty()) {
        send_queue.pop();
    }
}