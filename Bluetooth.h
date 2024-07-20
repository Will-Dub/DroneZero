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

struct Data {
    std::vector<uint8_t> content;
    std::string type;
};

class Bluetooth {
private:
    void bluetoothSendTask(int client_socket);
    void bluetoothReceiveTask(int client_socket);
    void bluetoothServerTask();

    std::atomic<bool> is_running;
    std::atomic<bool> is_client_connected;
    std::queue<Data> send_queue;
    std::queue<Data> receive_queue;
    std::mutex send_queue_mutex;
    std::mutex receive_queue_mutex;
    std::condition_variable send_queue_condition;
    int client_socket;

public:
    Bluetooth();

    ~Bluetooth();

    void startServer();

    void stop();

    std::unique_ptr<Data> getReceivedData();

    void sendData(Data data);

    bool isRunning();

    bool isClientConnected();
};

#endif