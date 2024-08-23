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

std::atomic<bool> isRunning(true);
Bluetooth bluetooth;

/**
 * Listen for new data on the uart and store it
 */
void uartListenerTask(UART* uart) {
    uart->listenForData();
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        isRunning = false;
    }
}

void sendCaptureTask(Camera& camera, Bluetooth& bluetooth, int quality){
    if(quality > 100 || quality < 0){
        return;
    }

    size_t imageSize = camera.getImageBufferSize();

    std::vector<uint8_t> imageData = camera.captureImage();

    DataPacket dataPacket;
    dataPacket.droneId = 3;
    dataPacket.packetId = 4;
    dataPacket.type = DataType::IMAGE;
    dataPacket.data = camera.convertToJpeg(imageData, camera.getWidth(), camera.getHeight(), quality);
    dataPacket.dataSize = dataPacket.data.size();

    bluetooth.sendData(dataPacket);
    return;
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
    std::unique_ptr<PositionData> latestPositionData = std::make_unique<PositionData>();

    int messageCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    spdlog::info("-------------START-------------");
    
    while (isRunning) {
        //-------------------------------------------
        //Handle new data from the pico
        if (uart.isNewDataReceived()) {
            std::optional<Message> receivedMessageOpt = uart.getReceivedMessage();
            
            if (receivedMessageOpt.has_value()) {
                Message receivedMessage = receivedMessageOpt.value();
                //Handle new message
                switch(receivedMessage.type){
                    case MessageType::PositionData:
                        *latestPositionData = receivedMessage.data.positionData;
                        std::cout << "Long: " << latestPositionData->gpsLongitude << " Lat: " << latestPositionData->gpsLatitude << " Alt: " << latestPositionData->gpsAltitude << std::endl;
                        break;
                    case MessageType::SensorData:
                        std::cout << "Accel x: " << receivedMessage.data.sensorData.accelX << " Accel y: " << receivedMessage.data.sensorData.accelY << " Accel z: " << receivedMessage.data.sensorData.accelZ << std::endl;
                        std::cout << "Gyro x: " << receivedMessage.data.sensorData.gyroX << " Gyro y: " << receivedMessage.data.sensorData.gyroY << " Gyro z: " << receivedMessage.data.sensorData.gyroZ << std::endl;
                        std::cout << "Mag x: " << receivedMessage.data.sensorData.magX << " Mag y: " << receivedMessage.data.sensorData.magY << " Mag z: " << receivedMessage.data.sensorData.magZ << std::endl;
                        break;
                    case MessageType::LogData:
                        if(receivedMessage.data.logData.type == LogType::LOG_INFO){
                            spdlog::info("Received log(pico): {}", receivedMessage.data.logData.message);
                        }else if(receivedMessage.data.logData.type == LogType::LOG_ERROR){
                            spdlog::error("Received log(pico): {}", receivedMessage.data.logData.message);
                        }else{
                            spdlog::critical("Received log(pico): {}", receivedMessage.data.logData.message);
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
                    if(!latestPositionData){
                        break;
                    }
                    
                    std::ostringstream oss;
                    oss << latestPositionData->gpsLatitude << ",";
                    oss << latestPositionData->gpsLongitude << ",";
                    oss << latestPositionData->gpsAltitude << ",";
                    oss << latestPositionData->gpsKmph << ",";
                    oss << latestPositionData->gpsCourseDeg;
                    std::string dataStr = oss.str();
                    std::vector<uint8_t> dataVector = bluetooth.stringToVector(dataStr);

                    DataPacket dataPacket;
                    dataPacket.droneId = 3;
                    dataPacket.packetId = 4;
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
        if (elapsedTime >= 5) {
            std::cout << "Message taken(per 5s): "<< messageCount << std::endl;
            Message message;
            message.type = MessageType::RequestData;
            message.data.requestData.requestType = RequestType::POSITION_REQUEST;
            uart.writeMessage(message);
            message.data.requestData.requestType = RequestType::SENSOR_REQUEST;
            uart.writeMessage(message);
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