#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include <cstdio>
#include <sstream>

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

    bool useMpu6050, useQmc5883l, useGps, useLog;
};

struct SensorData {
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
    int16_t magX, magY, magZ;
    float pitch, roll, yaw;
};

// Unified message structure
/*
MARKER

LENGTH

LENGTH

TYPE * 4

DATA * 96

CHECKSUM

CHECKSUM

END_MARKER

Total: 103 bytes
*/
struct Message {
    static constexpr uint8_t START_MARKER = 0x7E;
    static constexpr uint8_t END_MARKER = 0x7E;

    MessageType type;

    union {
        ControlData controlData;
        LogData logData;
        ModeData modeData;
        PositionData positionData;
        RequestData requestData;
        StatusData statusData;
        SensorData sensorData;
    } data;

    uint16_t calculateChecksum(const uint8_t* data, size_t length) const {
        uint16_t checksum = 0;
        for (size_t i = 0; i < length; ++i) {
            checksum += data[i];
        }
        return checksum;
    }

    size_t serialize(uint8_t* buffer, size_t buffer_size) const {
        size_t dataSize = 0;

        switch (type) {
            case MessageType::ControlData:
                dataSize = sizeof(ControlData);
                break;
            case MessageType::LogData:
                dataSize = sizeof(LogData);
                break;
            case MessageType::ModeData:
                dataSize = sizeof(ModeData);
                break;
            case MessageType::PositionData:
                dataSize = sizeof(PositionData);
                break;
            case MessageType::RequestData:
                dataSize = sizeof(RequestData);
                break;
            case MessageType::StatusData:
                dataSize = sizeof(StatusData);
                break;
            case MessageType::SensorData:
                dataSize = sizeof(SensorData);
                break;
        }

        if (buffer_size < 7 + dataSize) return 0;

        // Byte 0
        buffer[0] = START_MARKER;

        // Calculate message length (type + data size)
        uint16_t messageLength = sizeof(uint8_t) + dataSize;

        // Bytes 1-2
        memcpy(buffer + 1, &messageLength, sizeof(uint16_t));

        // Byte 3
        buffer[3] = static_cast<uint8_t>(type);

        // Bytes 4-(3+dataSize)
        memcpy(buffer + 4, &data, dataSize);

        uint16_t checksum = calculateChecksum(buffer + 3, messageLength);

        // Bytes (4+dataSize)-(5+dataSize)
        memcpy(buffer + 4 + dataSize, &checksum, sizeof(uint16_t));

        // Byte (6+dataSize)
        buffer[6 + dataSize] = END_MARKER;

        return 7 + dataSize;
    }

    bool deserialize(const uint8_t* buffer, size_t buffer_size) {
        if (buffer[0] != START_MARKER || buffer[buffer_size - 1] != END_MARKER) return false;

        uint16_t messageLength;
        memcpy(&messageLength, buffer + 1, sizeof(uint16_t));

        if (buffer_size < 6 + messageLength) return false;

        uint16_t checksum;
        memcpy(&checksum, buffer + 3 + messageLength, sizeof(uint16_t));

        uint16_t calculated_checksum = calculateChecksum(buffer + 3, messageLength);
        if (checksum != calculated_checksum) return false;

        type = static_cast<MessageType>(*(buffer + 3));

        size_t dataSize = 0;
        switch (type) {
            case MessageType::ControlData:
                dataSize = sizeof(ControlData);
                break;
            case MessageType::LogData:
                dataSize = sizeof(LogData);
                break;
            case MessageType::ModeData:
                dataSize = sizeof(ModeData);
                break;
            case MessageType::PositionData:
                dataSize = sizeof(PositionData);
                break;
            case MessageType::RequestData:
                dataSize = sizeof(RequestData);
                break;
            case MessageType::StatusData:
                dataSize = sizeof(StatusData);
                break;
            case MessageType::SensorData:
                dataSize = sizeof(SensorData);
                break;
        }

        memcpy(&data, buffer + 4, dataSize);

        return true;
    }
};

#endif