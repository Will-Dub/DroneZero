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
    IMAGE,
    TEXT,
};

struct DataPacket {
    DataType type;
    uint32_t dataSize;
    std::vector<uint8_t> data;
};

class Bluetooth {
private:
    void bluetoothServerTask();

    std::atomic<bool> is_running;
    std::atomic<bool> is_client_connected;
    std::queue<DataPacket> receive_queue;
    std::mutex receive_queue_mutex;
    int client_socket;

public:
    Bluetooth();

    ~Bluetooth();

    void startServer();

    void stop();

    std::unique_ptr<DataPacket> getReceivedData();

    void sendData(DataPacket data);

    bool isRunning();

    bool isClientConnected();
};

#endif