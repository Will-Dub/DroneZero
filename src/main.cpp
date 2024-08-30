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

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> elements;
    std::string element;
    std::stringstream ss(str);

    while (std::getline(ss, element, delimiter)) {
        elements.push_back(element);
    }

    return elements;
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
    std::unique_ptr<SensorData> latestSensorData = std::make_unique<SensorData>();

    int messageCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    spdlog::info("-------------START-------------");
    
    while (isRunning) {
        //-------------------------------------------
        //Handle new data from the pico
        if (uart.isNewDataReceived()) {
            std::optional<DataPacket> dataPacketUartOpt = uart.getReceivedDataPacket();
            
            if (dataPacketUartOpt.has_value()) {
                    DataPacket dataPacketUart = dataPacketUartOpt.value();

                    std::string dataString(dataPacketUart.data.begin(), dataPacketUart.data.end());

                    std::vector<std::string> dataElements = split(dataString, ';');
                    // TODO Refactor that shit haha
                    switch(dataPacketUart.type){
                        case DataType::GPS:
                            latestPositionData->gpsLongitude = std::stod(dataElements[0]);
                            latestPositionData->gpsLatitude = std::stod(dataElements[1]);
                            latestPositionData->gpsAltitude = std::stod(dataElements[2]);
                            latestPositionData->gpsKmph = std::stod(dataElements[3]);
                            latestPositionData->gpsCourseDeg = std::stod(dataElements[4]);
                            
                            std::cout << "Long: " << latestPositionData->gpsLongitude << " Lat: " << latestPositionData->gpsLatitude << " Alt: " << latestPositionData->gpsAltitude << std::endl;
                            break;
                        case DataType::SENSOR:
                            latestSensorData->accelX = std::stof(dataElements[0]);
                            latestSensorData->accelY = std::stof(dataElements[1]);
                            latestSensorData->accelZ = std::stof(dataElements[2]);
                            latestSensorData->gyroX = std::stof(dataElements[3]);
                            latestSensorData->gyroY = std::stof(dataElements[4]);
                            latestSensorData->gyroZ = std::stof(dataElements[5]);
                            latestSensorData->magX = static_cast<uint16_t>(std::stoul(dataElements[6]));
                            latestSensorData->magY = static_cast<uint16_t>(std::stoul(dataElements[7]));
                            latestSensorData->magZ = static_cast<uint16_t>(std::stoul(dataElements[8]));
                            latestSensorData->pitch = std::stof(dataElements[9]);
                            latestSensorData->roll = std::stof(dataElements[10]);
                            latestSensorData->yaw = std::stof(dataElements[11]);


                            std::cout << "Accel x: " << latestSensorData->accelX << " Accel y: " << latestSensorData->accelY << " Accel z: " << latestSensorData->accelZ << std::endl;
                            std::cout << "Gyro x: " << latestSensorData->gyroX << " Gyro y: " << latestSensorData->gyroY << " Gyro z: " << latestSensorData->gyroZ << std::endl;
                            std::cout << "Mag x: " << latestSensorData->magX << " Mag y: " << latestSensorData->magY << " Mag z: " << latestSensorData->magZ << std::endl;
                            break;
                        case DataType::LOG:
                            if(dataElements[0] == "info"){
                                spdlog::info("Received log(pico): {}", dataElements[1]);
                            }else if(dataElements[0] == "error"){
                                spdlog::error("Received log(pico): {}", dataElements[1]);
                            }else{
                                spdlog::critical("Received log(pico): {}", dataElements[1]);
                            }
                            break;
                }
            }
        }

        //-------------------------------------------
        //Handle new data from the bluetooth
        std::unique_ptr<DataPacket> dataPacketBluetooth = bluetooth.getReceivedData();
        if(dataPacketBluetooth){
            std::string data_str(dataPacketBluetooth->data.begin(), dataPacketBluetooth->data.end());
            switch (dataPacketBluetooth->type) {
                case DataType::IMAGE: {
                    int received_int = 10;
                    if (dataPacketBluetooth->data.size() >= 1) {
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
                case DataType::GPS: {
                    if(!latestPositionData){
                        break;
                    }
                    
                    std::ostringstream oss;
                    oss << latestPositionData->gpsLatitude << ";";
                    oss << latestPositionData->gpsLongitude << ";";
                    oss << latestPositionData->gpsAltitude << ";";
                    oss << latestPositionData->gpsKmph << ";";
                    oss << latestPositionData->gpsCourseDeg;
                    std::string dataStr = oss.str();
                    std::vector<uint8_t> dataVector = bluetooth.stringToVector(dataStr);

                    DataPacket dataPacket;
                    dataPacket.droneId = 3;
                    dataPacket.packetId = 4;
                    dataPacket.type = DataType::GPS;
                    dataPacket.data = dataVector;
                    dataPacket.dataSize = dataPacket.data.size();
                    uart.writeDataPacket(*dataPacketBluetooth);
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
            messageCount = 0;
            startTime = std::chrono::steady_clock::now();
        }
        sleep(0.02);
    }

    bluetooth.stop();

    //-------------------------------------------
    //Wait for thread to finish
    listenerThread.join();

    spdlog::info("-------------END-------------");
    
    return 0;
}