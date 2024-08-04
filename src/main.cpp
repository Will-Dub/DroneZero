#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <optional>
#include <signal.h>

// Local
#include "Camera.h"
#include "Bluetooth.h"
#include "Message.h"
#include "UART.h"

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

void sendCaptureTask(Camera& camera, Bluetooth& bluetooth, int quality){
    if(quality > 100 || quality < 0){
        return;
    }

    size_t imageSize = camera.getImageBufferSize();

    std::vector<uint8_t> imageData = camera.captureImage();

    DataPacket dataPacket;
    dataPacket.type = DataType::IMAGE;
    dataPacket.data = camera.convertToJpeg(imageData, camera.getWidth(), camera.getHeight(), quality);
    dataPacket.dataSize = dataPacket.data.size();

    bluetooth.sendData(dataPacket);
    return;
}

std::vector<uint8_t> stringToVector(const std::string& str) {
    std::vector<uint8_t> byteVector;
    byteVector.reserve(str.size());

    for (char c : str) {
        byteVector.push_back(static_cast<uint8_t>(c));
    }

    return byteVector;
}

int main() {
    signal(SIGINT, signal_handler);

    //-------------------------------------------
    //Variable init
    //Logger
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(".logs/basic_log.txt", true);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks { file_sink, console_sink };
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

    spdlog::set_default_logger(logger);

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
    std::unique_ptr<SensorData> latestSensorData = std::make_unique<SensorData>();

    int messageCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    spdlog::info("-------------START-------------");
    
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
                        if(latestSensorData){
                            *latestSensorData = received_message_opt.value().data.sensor_data;
                            std::cout << "Long: " << latestSensorData->gps_longitude << " Lat: " << latestSensorData->gps_latitude << " Alt: " << latestSensorData->gps_altitude << std::endl;
                        }

                        messageCount++;
                        break;
                    case MessageType::LogData:
                        if(received_message.data.log_data.type == LogType::LOG_INFO){
                            spdlog::info("Received log(pico): {}", received_message.data.log_data.message);
                        }else if(received_message.data.log_data.type == LogType::LOG_ERROR){
                            spdlog::error("Received log(pico): {}", received_message.data.log_data.message);
                        }else{
                            spdlog::critical("Received log(pico): {}", received_message.data.log_data.message);
                        }
                        break;
                }
            }
        }

        //-------------------------------------------
        //Handle new data from the bluetooth
        std::unique_ptr<DataPacket> new_received_data = bluetooth.getReceivedData();
        if(new_received_data){
            std::string data_str(new_received_data->data.begin(), new_received_data->data.end());
            switch (new_received_data->type) {
                case IMAGE: {
                    int received_int = 10;
                    if (new_received_data->data.size() >= 1) {
                        try {
                            received_int = std::stoi(data_str);
                        } catch (const std::exception& e) {
                            spdlog::error("Error converting int");
                        }
                    }

                    std::thread imageThread(sendCaptureTask, std::ref(camera), std::ref(bluetooth), received_int);
                    imageThread.detach();
                    break;
                }
                case GPS: {
                    if(!latestSensorData){
                        break;
                    }
                    
                    std::ostringstream oss;
                    oss << latestSensorData->gps_latitude << ",";
                    oss << latestSensorData->gps_longitude << ",";
                    oss << latestSensorData->gps_altitude << ",";
                    oss << latestSensorData->gps_kmph << ",";
                    oss << latestSensorData->gps_course_deg;
                    std::string dataStr = oss.str();
                    std::vector<uint8_t> dataVector = stringToVector(dataStr);

                    DataPacket dataPacket;
                    dataPacket.type = DataType::GPS;
                    dataPacket.data = dataVector;
                    dataPacket.dataSize = dataPacket.data.size();
                    bluetooth.sendData(dataPacket);
                    break;
                }
                default: {
                    spdlog::warn("Command not yet implemented");
                }
            }
            messageCount++;
        }

        //-------------------------------------------
        //Send new data bluetooth

        
        //-------------------------------------------
        //Calculate message per seconds
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedTime >= 1) {
            std::cout << "Image taken(per s): "<< messageCount << std::endl;
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