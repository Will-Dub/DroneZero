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

    std::atomic<bool> is_running;
    std::atomic<bool> is_client_connected;
    int client_socket;

    //Receive
    std::queue<DataPacket> receive_queue;
    std::mutex receive_queue_mutex;

    //Send
    std::queue<DataPacket> send_queue;
    std::mutex send_queue_mutex;
    std::condition_variable send_queue_condition;

public:
    Bluetooth();

    ~Bluetooth();

    void startServer();

    void stop();

    std::unique_ptr<DataPacket> getReceivedData();

    void sendData(DataPacket dataPacket);

    bool isRunning();

    bool isClientConnected();

    void clearSendQueue();
};

#endif