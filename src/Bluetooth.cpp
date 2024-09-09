#include "Bluetooth.h"

Bluetooth::Bluetooth()
    : isRunning(false), isClientConnected(false), clientSocket(-1) {}

Bluetooth::~Bluetooth() {
    stop();
    clearSendQueue();
}

/*
* Start bluetooth
*/
void Bluetooth::startServer() {
    isRunning = true;
    std::thread serverThread(&Bluetooth::bluetoothServerTask, this);
    serverThread.detach();
}

/*
* Stop all bluetooth thread
*/
void Bluetooth::stop() {
    isRunning.store(false);
    sendQueueCondition.notify_all();
    clearSendQueue();
}

/*
* Task that handle sending data
*/
void Bluetooth::bluetoothSendTask(int clientSocket) {
    while (isRunning.load() && isClientConnected.load()) {
        DataPacket dataPacket;

        {
            std::unique_lock<std::mutex> lock(sendQueueMutex);
            sendQueueCondition.wait(lock, [this] { return !sendQueue.empty() || !isRunning || !isClientConnected; });

            if (!isRunning.load() || !isClientConnected.load()) {
                spdlog::info("Send task stopped");
                break;
            }

            if (sendQueue.empty()) {
                continue;
            }

            dataPacket = sendQueue.front();
            sendQueue.pop();

            spdlog::info("Sending data, remaining: {}", sendQueue.size());
        }

        // Send the data
        //Send the drone id(1 byte)
        if(send(clientSocket, &dataPacket.droneId, sizeof(dataPacket.droneId), 0) < 0){
            spdlog::error("Failed to send data");
            isClientConnected.store(false);
            return;
        }

         //Send the packet id(4 byte)
        if(send(clientSocket, &dataPacket.packetId, sizeof(dataPacket.packetId), 0) < 0){
            spdlog::error("Failed to send data");
            isClientConnected.store(false);
            return;
        }

        //Send the type(1 byte)
        if(send(clientSocket, &dataPacket.type, sizeof(dataPacket.type), 0) < 0){
            spdlog::error("Failed to send data");
            isClientConnected.store(false);
            return;
        }
        
        //Send the size(4 byte)
        if(send(clientSocket, &dataPacket.dataSize, sizeof(dataPacket.dataSize), 0) < 0){
            spdlog::error("Failed to send data");
            isClientConnected.store(false);
            return;
        }

        //Send the data(dataSize byte)
        const size_t maxBufferSize = 8192;
        const std::vector<uint8_t>& data = dataPacket.data;
        size_t totalBytesSent = 0;
        while (totalBytesSent < data.size()) {
            size_t bytesToSend = std::min(data.size() - totalBytesSent, maxBufferSize);
            ssize_t bytesSent = send(clientSocket, data.data() + totalBytesSent, bytesToSend, 0);
            if (bytesSent < 0) {
                spdlog::error("Failed to send data");
                isClientConnected.store(false);
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
    struct sockaddr_rc locAddr = { 0 }, remAddr = { 0 };
    int serverSocket;
    socklen_t opt = sizeof(remAddr);

    // Allocate socket
    serverSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (serverSocket < 0) {
        spdlog::critical("Bluetooth socket creation failed");
        return;
    }

    // Bind bluetooth socket
    locAddr.rc_family = AF_BLUETOOTH;
    locAddr.rc_bdaddr = {{0, 0, 0, 0, 0, 0}};
    locAddr.rc_channel = (uint8_t)1;
    if (bind(serverSocket, (struct sockaddr *)&locAddr, sizeof(locAddr)) < 0) {
        spdlog::critical("Bluetooth bind failed");
        close(serverSocket);
        return;
    }

    // Put bluetooth in listening mode
    // Max 1 client
    if (listen(serverSocket, 1) < 0) {
        spdlog::critical("Bluetooth listen failed");
        close(serverSocket);
        return;
    }

    // Main loop
    while (isRunning.load()) {
        // Accept connection
        clientSocket = accept(serverSocket, (struct sockaddr *)&remAddr, &opt);
        if (clientSocket < 0) {
            spdlog::warn("Bluetooth accept failed");
            continue;
        }

        char addr_str[18];
        ba2str(&remAddr.rc_bdaddr, addr_str);
        spdlog::info("New bluetooth connection from {}", addr_str);

        // Set client as connected
        isClientConnected.store(true);

        std::thread send_thread(&Bluetooth::bluetoothSendTask, this, clientSocket);

        //Receive data
        while (isClientConnected.load()) {
            // Read Drone id (1 byte)
            uint8_t droneId;
            ssize_t bytesReceived = recv(clientSocket, &droneId, sizeof(droneId), 0);
            if (bytesReceived <= 0) {
                spdlog::error("Failed to receive data. Bytes received: {}", bytesReceived);
                break;
            }

            // Read packet id (4 bytes) little endian
            uint32_t packetId;
            bytesReceived = recv(clientSocket, &packetId, sizeof(packetId), 0);
            if (bytesReceived <= 0) {
                spdlog::error("Failed to receive data. Bytes received: {}", bytesReceived);
                break;
            }

            // Read DataType (1 byte)
            uint8_t typeOrdinal;
            bytesReceived = recv(clientSocket, &typeOrdinal, sizeof(typeOrdinal), 0);
            if (bytesReceived <= 0) {
                spdlog::error("Failed to receive data. Bytes received: {}", bytesReceived);
                break;
            }
            DataType dataType = static_cast<DataType>(typeOrdinal);

            // Read Size (4 bytes) little endian
            uint32_t dataSize;
            bytesReceived = recv(clientSocket, &dataSize, sizeof(dataSize), 0);
            if (bytesReceived <= 0) {
                spdlog::error("Failed to receive data. Bytes received: {}", bytesReceived);
                break;
            }

            // Read Data
            std::vector<uint8_t> data(dataSize);
            size_t totalBytesReceived = 0;
            while (totalBytesReceived < dataSize) {
                bytesReceived = recv(clientSocket, data.data() + totalBytesReceived, dataSize - totalBytesReceived, 0);
                if (bytesReceived <= 0) {
                    spdlog::error("Failed to receive data. Bytes received: {}", bytesReceived);
                    break;
                }
                totalBytesReceived += bytesReceived;
            }

            if (totalBytesReceived == dataSize) {
                // Handle the received data
                DataPacket dataPacket;
                dataPacket.droneId = droneId;
                dataPacket.packetId = packetId;
                dataPacket.type = dataType;
                dataPacket.dataSize = dataSize;
                dataPacket.data = data;
                {
                    std::lock_guard<std::mutex> lock(receiveQueueMutex);
                    receiveQueue.push(dataPacket);
                }
            } else {
                spdlog::error("Incomplete data read. Expected: {}, received: {}", dataSize, totalBytesReceived);
                break;
            }
        }

        isClientConnected.store(false);
        close(clientSocket);

        spdlog::info("Connection closed or error occurred. Cleaning up.");

        //Stop the send thread
        sendQueueCondition.notify_one();
        send_thread.join();

        //Clear the send queue
        clearSendQueue();
    }

    // Close socket
    close(serverSocket);
}

/*
* Get the data from the receive queue if any
*/
std::unique_ptr<DataPacket> Bluetooth::getReceivedData(){
    std::lock_guard<std::mutex> lock(receiveQueueMutex);
    if (!receiveQueue.empty()) {
        std::unique_ptr<DataPacket> received_data = std::make_unique<DataPacket>(receiveQueue.front());
        receiveQueue.pop();

        return received_data;
    }
    return nullptr;
}

/*
* Sends a datapacket to the client
*/
void Bluetooth::sendDataPacket(DataPacket dataPacket){
    if(isClientConnected.load()){
        {
            std::lock_guard<std::mutex> lock(sendQueueMutex);
            sendQueue.push(dataPacket);
        }
        sendQueueCondition.notify_one();
    }

    return;
}

/*
* Return if the bluetooth thread is running
*/
bool Bluetooth::getIsRunning(){
    return isRunning.load();
}

/*
* Returns if a client is connected
*/
bool Bluetooth::getIsClientConnected(){
    return isClientConnected.load();
}

/*
* Clears the queue of data to send
*/
void Bluetooth::clearSendQueue() {
    std::lock_guard<std::mutex> lock(sendQueueMutex);
    while (!sendQueue.empty()) {
        sendQueue.pop();
    }
}

/**
 * Convert a string to a vector of uint
 */
std::vector<uint8_t> Bluetooth::stringToVector(const std::string& str) {
    std::vector<uint8_t> byteVector;
    byteVector.reserve(str.size());

    for (char c : str) {
        byteVector.push_back(static_cast<uint8_t>(c));
    }

    return byteVector;
}