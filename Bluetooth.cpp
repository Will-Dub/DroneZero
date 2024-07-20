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
    send_queue_condition.notify_all();
}

void Bluetooth::bluetoothSendTask(int client_socket) {
    while (is_running && is_client_connected) {
        std::unique_lock<std::mutex> lock(send_queue_mutex);
        send_queue_condition.wait(lock, [this] { return !send_queue.empty() || !is_running; });

        while (!send_queue.empty()) {
            Data data = send_queue.front();
            send_queue.pop();
            spdlog::info("Sending data, remaining: {}", send_queue.size());
            lock.unlock();

            // Send data via Bluetooth
            int bytes_sent = send(client_socket, data.content.data(), data.content.size(), 0);
            if (bytes_sent < 0) {
                spdlog::warn("Bluetooth send failed");
                is_client_connected = false;
                break;
            }

            lock.lock();
        }
    }
}

void Bluetooth::bluetoothReceiveTask(int client_socket) {
    char buf[1024] = {0};
    while (is_running && is_client_connected) {
        memset(buf, 0, sizeof(buf));
        int bytes_read = recv(client_socket, buf, sizeof(buf), 0);
        if (bytes_read > 0) {
            spdlog::info("Bluetooth data received: {}", std::string(buf, bytes_read));

            Data data;
            data.content = std::vector<uint8_t>(buf, buf + bytes_read);
            data.type = "message";

            std::lock_guard<std::mutex> lock(receive_queue_mutex);
            receive_queue.push(data);
        } else if (bytes_read < 0) {
            spdlog::warn("Bluetooth receive failed");
            is_client_connected = false;
            send_queue_condition.notify_one();
            break;
        }
    }
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

        // Start sending and receiving tasks
        std::thread send_thread(&Bluetooth::bluetoothSendTask, this, client_socket);
        std::thread receive_thread(&Bluetooth::bluetoothReceiveTask, this, client_socket);

        send_thread.join();
        receive_thread.join();

        is_client_connected = false;
    }

    // Close socket
    close(server_socket);
}

std::unique_ptr<Data> Bluetooth::getReceivedData(){
    std::lock_guard<std::mutex> lock(receive_queue_mutex);
    if (!receive_queue.empty()) {
        std::unique_ptr<Data> received_data = std::make_unique<Data>(receive_queue.front());
        receive_queue.pop();

        return received_data;
    }
    return nullptr;
}

void Bluetooth::sendData(Data data){
    if(is_client_connected){
        {
            std::lock_guard<std::mutex> lock(send_queue_mutex);
            send_queue.push(data);
        }
        send_queue_condition.notify_one();
    }

    return;
}

bool Bluetooth::isRunning(){
    return is_running;
}

bool Bluetooth::isClientConnected(){
    return is_client_connected;
}
