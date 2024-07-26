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

void saveImage(const std::vector<uint8_t>& bmpData, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);

    if (!file) {
        std::cerr << "Unable to open file for writing." << std::endl;
        return;
    }

    file.write(reinterpret_cast<const char*>(bmpData.data()), bmpData.size());
    file.close();
}


int main() {
    signal(SIGINT, signal_handler);

    //-------------------------------------------
    //Variable init
    //Logger
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/basic_log.txt", true);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks { file_sink, console_sink };
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

    spdlog::set_default_logger(logger);

    spdlog::info("-------------START-------------");

    //Uart
    UART uart("/dev/ttyS0", B230400);

    std::thread listenerThread(uartListenerTask, &uart);

    //Camera
    Camera camera;
    camera.init();

    //Bluetooth
    Bluetooth bluetooth;
    bluetooth.startServer();

    //-------------------------------------------
    //Main loop

    int messageCount = 0;
    auto startTime = std::chrono::steady_clock::now();
    
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
                        spdlog::info("Received data(pico): {}", received_message.data.log_data.message);
                        break;
                }
            }
        }

        //-------------------------------------------
        //Handle new data from the bluetooth
        std::unique_ptr<DataPacket> new_received_data = bluetooth.getReceivedData();
        if(new_received_data){
            spdlog::info("Received data(bluetooth): {}", std::string(new_received_data->data.begin(), new_received_data->data.end()));
            spdlog::info("Received data size(bluetooth): {}", new_received_data->dataSize);
            spdlog::info("Received data type(bluetooth): {}", new_received_data->type);
        }

        //-------------------------------------------
        //Send new data bluetooth

        if(bluetooth.isClientConnected()){
            size_t imageSize = camera.getImageBufferSize();

            std::vector<uint8_t> imageData = camera.captureImage();

            DataPacket dataPacket;
            dataPacket.type = DataType::IMAGE;
            dataPacket.data = camera.convertToJpeg(imageData, camera.getWidth(), camera.getHeight(), 30);
            dataPacket.dataSize = dataPacket.data.size();

            bluetooth.sendData(dataPacket);
            messageCount++;
        } 
        
        //-------------------------------------------
        //Calculate message per seconds
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedTime >= 1) {
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