#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <optional>
#include "message.cpp"
#include "UART.h"
#include <signal.h>
#include "Camera.h"
#include "Bluetooth.h"

// Thread
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>
#include <mutex>

//Logs
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

std::atomic<bool> is_running(true);
Bluetooth bluetooth;

/**
 * Listen for new data on the uart and store it
 */
void uartListenerTask(UART* uart) {
    uart->listenForData();
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        is_running = false;
    }
}

int main() {
    signal(SIGINT, signal_handler);

    //-------------------------------------------
    //Variable init
    //Uart
    UART uart("/dev/ttyS0", B230400);

    //Camera
    Camera camera;
    camera.init();

    //Logger
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/basic_log.txt", true);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks { file_sink, console_sink };
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

    spdlog::set_default_logger(logger);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e %l %n: %v");

    spdlog::info("-------------START-------------");

    //Bluetooth
    Bluetooth bluetooth;
    bluetooth.startServer();

    //-------------------------------------------
    //Start threads
    std::thread listenerThread(uartListenerTask, &uart);

    int messageCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    //-------------------------------------------
    //Main loop
    while (is_running) {
        //-------------------------------------------
        //Handle new data from the pico
        if (uart.isNewDataReceived()) {
            std::optional<Message> received_message_opt = uart.getReceivedMessage();
            
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
        std::unique_ptr<Data> new_received_data = bluetooth.getReceivedData();
        if(new_received_data){
            std::cout << "Received data(bluetooth): " << std::string(new_received_data->content.begin(), new_received_data->content.end()) << std::endl;
        }

        //-------------------------------------------
        //Send new data bluetooth
        
        //-------------------------------------------
        //Calculate message per seconds
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        messageCount++;
        if (elapsedTime >= 1) {
            Data data;
            data.type = "message";
            std::string msg = "Test send";
            data.content = std::vector<uint8_t>(msg.begin(), msg.end());
            bluetooth.sendData(data);

            std::cout << "Message received(per s): "<< messageCount << std::endl;
            messageCount = 0;
            startTime = std::chrono::steady_clock::now();
        }
    }

    bluetooth.stop();

    //-------------------------------------------
    //Wait for thread to finish
    listenerThread.join();

    spdlog::info("-------------END-------------");
    
    return 0;
}