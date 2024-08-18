#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <spdlog/spdlog.h>

enum DataType : uint8_t {
    IMAGE, // Client sends, server respond
    CONTROL, // Client sends
    INFO, // Client sends, server respond
    STATUS, // Client sends, server respond
    LOG, // Client sends, server respond
    SENSOR, // Client sends, server respond
    GPS,  // Client sends, server respond
    STOP, //Client or server sends
    TEST,
};

struct DataPacket {
    DataType type;
    uint32_t dataSize;
    std::vector<uint8_t> data;
};

class Bluetooth {
private:
    void bluetoothServerTask();
    void bluetoothSendTask(int client_socket);

    std::atomic<bool> isRunning;
    std::atomic<bool> isClientConnected;
    int clientSocket;

    //Receive
    std::queue<DataPacket> receiveQueue;
    std::mutex receiveQueueMutex;

    //Send
    std::queue<DataPacket> sendQueue;
    std::mutex sendQueueMutex;
    std::condition_variable sendQueueCondition;

public:
    Bluetooth();

    ~Bluetooth();

    void clearSendQueue();

    std::unique_ptr<DataPacket> getReceivedData();

    bool getIsClientConnected();
    
    bool getIsRunning();

    void sendData(DataPacket dataPacket);

    void startServer();

    void stop();

    std::vector<uint8_t> stringToVector(const std::string& str);
};

#endif