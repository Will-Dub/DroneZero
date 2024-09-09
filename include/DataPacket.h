#ifndef DATAPACKET_H
#define DATAPACKET_H

#include <stdio.h>
#include <string.h>
#include <cstdint>

// Message types
enum class MessageType {
    ControlData,
    ModeData,
    PositionData,
    RequestData,
    StatusData,
    SensorData,
    LogData,
};

//-----------------------------
//All enum used by message type

enum LogType : uint8_t {
    LOG_INFO,
    LOG_ERROR,
    LOG_CRITICAL
};

// Flight mode enumeration
enum FlightMode {
    MANUAL,
    STABILIZE,
    ALT_HOLD,
    AUTO
};

enum RequestType {
    STATUS_REQUEST,
    POSITION_REQUEST,
    MODE_REQUEST,
    CONTROL_REQUEST,
    SENSOR_REQUEST,
};

//-----------------------------
//All message type
struct ControlData {
    //Each motor control
    uint8_t motor_pwm[4];
};

struct LogData {
    // Log data
    LogType type;
    char message[30];
};

struct ModeData {
    // State information
    FlightMode mode;
    bool failSafeTriggered;
    double desiredLatitude, desiredLongitude, desiredAltitude, desiredSpeed;

    // Control parameters
    float desiredPitch, desiredRoll, desiredYaw;
};

struct PositionData {
    double gpsLatitude, gpsLongitude, gpsAltitude, gpsKmph, gpsCourseDeg;
};

struct RequestData{
    RequestType requestType;
};

struct StatusData {
    bool uartZeroConnected, uartGpsConnected, i2cConnected;

    bool useMotor, useMpu6050, useQmc5883l, useGps, useLog;
};

struct SensorData {
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
    int16_t magX, magY, magZ;
    float pitch, roll, yaw;
};

enum class DataType : uint8_t {
    UNDEFINED,
    IMAGE,
    CONTROL,
    INFO,
    STATUS,
    LOG,
    SENSOR,
    GPS,
    STOP,
    STOP_SPECIFIC,
    START,
    START_SPECIFIC
};

struct DataPacket {
    static constexpr uint8_t START_MARKER = 0x7E;
    static constexpr uint8_t END_MARKER = 0x7E;

    uint8_t droneId;
    uint32_t packetId;
    DataType type;
    uint32_t dataSize;
    std::vector<uint8_t> data;

    DataPacket() : droneId(0), packetId(0), type(DataType::UNDEFINED), dataSize(0) {}

    DataPacket(uint8_t drone, uint32_t packet, DataType t, const std::vector<uint8_t>& d) 
        : droneId(drone), packetId(packet), type(t), data(d), dataSize(d.size()) {}

    std::vector<uint8_t> toByteArray() const {
        std::vector<uint8_t> buffer(10 + dataSize);
        buffer[0] = droneId;
        std::memcpy(buffer.data() + 1, &packetId, sizeof(packetId));
        buffer[5] = static_cast<uint8_t>(type);
        std::memcpy(buffer.data() + 6, &dataSize, sizeof(dataSize));
        std::memcpy(buffer.data() + 10, data.data(), dataSize);
        return buffer;
    }

    static DataPacket fromByteArray(const uint8_t* byteArray, uint32_t totalSize) {
        uint8_t droneId = byteArray[0];
        uint32_t packetId;
        std::memcpy(&packetId, byteArray + 1, sizeof(packetId));
        DataType type = static_cast<DataType>(byteArray[5]);
        uint32_t dataSize;
        std::memcpy(&dataSize, byteArray + 6, sizeof(dataSize));
        std::vector<uint8_t> data(dataSize);
        std::memcpy(data.data(), byteArray + 10, dataSize);
        return DataPacket(droneId, packetId, type, data);
    }

    size_t serialize(uint8_t* buffer, size_t buffer_size) const {
        size_t required_size = 12 + dataSize;
        if (buffer_size < required_size) {
            return 0;
        }
        std::vector<uint8_t> byteArray = toByteArray();
        buffer[0] = START_MARKER;
        std::memcpy(buffer + 1, byteArray.data(), 10 + dataSize);
        buffer[required_size - 1] = END_MARKER;
        return required_size;
    }

    bool deserialize(const uint8_t* buffer, size_t buffer_size) {
        if (buffer_size < 12 || buffer[0] != START_MARKER || buffer[buffer_size - 1] != END_MARKER) {
            return false;
        }
        std::vector<uint8_t> byteArray(buffer_size - 2);
        std::memcpy(byteArray.data(), buffer + 1, buffer_size - 2);
        *this = fromByteArray(byteArray.data(), buffer_size - 2);
        return true;
    }
};

#endif