#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <optional>
#include "message.cpp"
#include "uart.h"
#include <signal.h>
#include <atomic>
#include <condition_variable>
#include <queue>

//Bluetooth
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

std::atomic<bool> running(true);
std::queue<std::string> data_queue;
std::mutex queue_mutex;
int client_socket = -1;

/*
* Bluetooth
*/
void signal_handler(int sig) {
    running = false;
}

void data_received(int client_socket, char *buf, int bytes_read) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    data_queue.push(std::string(buf, bytes_read));
}

/**
 * Listen for new data on the uart and store it
 */
void uartListenerTask(UART* uart) {
    uart->listenForData();
}

void uartSenderTask(UART* uart) {
    int count = 0;
    while (true) {
        std::string message = "Salut " + std::to_string(count);
        count++;

        std::cout << "Out: " << message << std::endl;

        uart->writeLine(message);
        uart->flush();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void bluetoothServerTask() {
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int server_socket, client_socket, bytes_read;
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
    loc_addr.rc_channel = (uint8_t)1;
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
        //Accept connection
        int new_client_socket = accept(server_socket, (struct sockaddr *)&rem_addr, &opt);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        ba2str(&rem_addr.rc_bdaddr, buf);
        fprintf(stderr, "Accepted connection from %s\n", buf);

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            client_socket = new_client_socket;
        }

        //Loop for receiving and sending data
        while (running) {
            memset(buf, 0, sizeof(buf));

            //Read data from the client
            bytes_read = recv(client_socket, buf, sizeof(buf), 0);
            if (bytes_read <= 0) {
                break;
            }

            //Process received data
            data_received(client_socket, buf, bytes_read);
        }

        //Close connection
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            close(client_socket);
            client_socket = -1;
        }
    }

    //Close socket
    close(server_socket);
}

int main() {
    signal(SIGINT, signal_handler);

    //-------------------------------------------
    //Variable declaration
    UART uart("/dev/ttyS0", B230400);

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
                        std::cout << "New message: " << received_message.data.log_data.message << std::endl;
                        break;
                }
            }
        }

        //-------------------------------------------
        //Handle new data from the bluetooth
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!data_queue.empty()) {
            std::string data = data_queue.front();
            data_queue.pop();

            std::cout << "Data received: " << data << std::endl;

            if (client_socket >= 0) {
                ssize_t bytes_sent = send(client_socket, data.c_str(), data.size(), 0);
                if (bytes_sent < 0) {
                    perror("Failed to send data");
                } else {
                    std::cout << "Sent data: " << data << std::endl;
                }
            }
        }
        

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedTime >= 1) {
            std::cout << "Messages received per second: " << messageCount << std::endl;
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