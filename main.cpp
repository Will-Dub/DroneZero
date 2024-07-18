#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <optional>
#include "message.cpp"
#include "uart.h"
#include <signal.h>

// Thread
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>
#include <mutex>

//Bluetooth
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

//Camera
#include <raspicam/raspicam.h>

struct Data {
    std::vector<uint8_t> content;
    std::string type;
};

std::atomic<bool> running(true);
std::queue<Data> send_queue;
std::queue<Data> receive_queue;
std::mutex send_queue_mutex;
std::mutex receive_queue_mutex;
std::condition_variable send_queue_condition;
int client_socket = -1;

/**
 * Listen for new data on the uart and store it
 */
void uartListenerTask(UART* uart) {
    uart->listenForData();
}

/*
*    Bluetooth
*/
void bluetoothSendTask(int client_socket) {
    while (running) {
        std::unique_lock<std::mutex> lock(send_queue_mutex);
        send_queue_condition.wait(lock, [] { return !send_queue.empty() || !running; });

        while (!send_queue.empty()) {
            Data data = send_queue.front();
            send_queue.pop();
            lock.unlock();

            // Send data via Bluetooth
            int bytes_sent = send(client_socket, data.content.data(), data.content.size(), 0);
            if (bytes_sent < 0) {
                perror("Send failed");
                break;
            }

            lock.lock();
        }
    }
}

void bluetoothReceiveTask(int client_socket) {
    char buf[1024] = {0};
    while (running) {
        memset(buf, 0, sizeof(buf));
        int bytes_read = recv(client_socket, buf, sizeof(buf), 0);
        if (bytes_read > 0) {
            std::cout << "Data: " << std::string(buf, bytes_read) << std::endl;
            
            Data data;
            data.content = std::vector<uint8_t>(buf, buf + bytes_read);
            data.type = "message";

            std::lock_guard<std::mutex> lock(receive_queue_mutex);
            receive_queue.push(data);
        } else if (bytes_read < 0) {
            perror("Receive failed");
            break;
        }
    }
}

void bluetoothServerTask() {
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int server_socket, client_socket;
    socklen_t opt = sizeof(rem_addr);

    //Allocate socket
    server_socket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return;
    }

    //Bind bluetooth socket
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = {{0, 0, 0, 0, 0, 0}};
    loc_addr.rc_channel = (uint8_t) 1;
    if (bind(server_socket, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return;
    }

    //Put bluetooth in listenning mode
    if (listen(server_socket, 1) < 0) { // Max 1 client
        perror("Listen failed");
        close(server_socket);
        return;
    }

    //Main loop
    while (running) {
        // Accept connection
        client_socket = accept(server_socket, (struct sockaddr *)&rem_addr, &opt);
        if (client_socket < 0) {
            if (running) {
                perror("Accept failed");
            }
            continue;
        }

        ba2str(&rem_addr.rc_bdaddr, buf);
        fprintf(stderr, "Accepted connection from %s\n", buf);

        // Start sending and receiving tasks
        std::thread send_thread(bluetoothSendTask, client_socket);
        std::thread receive_thread(bluetoothReceiveTask, client_socket);

        // Detach threads to handle multiple clients
        send_thread.detach();
        receive_thread.detach();
    }

    //Close socket
    close(server_socket);
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        running = false;
        send_queue_condition.notify_all();
    }
}

int main() {
    signal(SIGINT, signal_handler);

    //-------------------------------------------
    //Variable init
    UART uart("/dev/ttyS0", B230400);

    /*raspicam::RaspiCam Camera;
    Camera.open();
    if (!Camera.isOpened()) {
        std::cerr << "Error opening camera" << std::endl;
        return -1;
    }

    Camera.grab();
    unsigned char *data = new unsigned char[Camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_RGB)];
    Camera.retrieve(data, raspicam::RASPICAM_FORMAT_RGB);
    delete[] data;*/

    //-------------------------------------------
    //Start threads
    std::thread listenerThread(uartListenerTask, &uart);
    std::thread bluetoothTask(bluetoothServerTask);

    int messageCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    //-------------------------------------------
    //Main loop
    while (running) {
        //-------------------------------------------
        //Handle new data from the pico
        if (uart.isNewDataReceived()) {
            std::optional<Message> received_message_opt = uart.getReceiveMessage();
            
            if (received_message_opt.has_value()) {
                Message received_message = received_message_opt.value();
                //Handle new message
                switch(received_message.type){
                    case MessageType::SensorData:
                        //std::cout << "I2C: " << received_message_opt.value().data.sensor_data.i2c_connected << std::endl;
                        messageCount++;
                        break;
                    case MessageType::LogData:
                        std::cout << "Received data(pico): " << received_message.data.log_data.message << std::endl;
                        break;
                }
            }
        }

        //-------------------------------------------
        //Handle new data from the bluetooth
        {
            std::lock_guard<std::mutex> lock(receive_queue_mutex);
            while (!receive_queue.empty()) {
                Data received_data = receive_queue.front();
                receive_queue.pop();

                // Process the received data (e.g., print it)
                std::cout << "Received data(bluetooth): " << std::string(received_data.content.begin(), received_data.content.end()) << std::endl;
            }
        }

        //-------------------------------------------
        //Send new data bluetooth
        /*Data data;
        data.type = "message";
        std::string msg = "Test send";
        data.content = std::vector<uint8_t>(msg.begin(), msg.end());

        {
            std::lock_guard<std::mutex> lock(send_queue_mutex);
            send_queue.push(data);
        }
        send_queue_condition.notify_one();*/
        
        //-------------------------------------------
        //Calculate message per seconds
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        messageCount++;
        if (elapsedTime >= 1) {
            std::cout << "Message received(per s): " << messageCount << std::endl;
            messageCount = 0;
            startTime = std::chrono::steady_clock::now();
        }
    }

    //-------------------------------------------
    //Wait for thread to finish
    listenerThread.join();
    bluetoothTask.join();

    return 0;
}