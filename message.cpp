#include <cstring>
#include <stdio.h>
#include <sstream>

#ifndef MESSAGE_H
#define MESSAGE_H


// Message types
enum class MessageType {
    SensorData,
    LogData,
};

// Flight mode enumeration
enum FlightMode {
    MANUAL,
    STABILIZE,
    ALT_HOLD,
    AUTO
};

struct FlightControllerData {
    // Configuration data
    float pid_kp, pid_ki, pid_kd;

    // State information
    FlightMode mode;
    bool fail_safe_triggered;

    // Control parameters
    float desired_pitch, desired_roll, desired_yaw;
    uint16_t motor_pwm[4];
};

struct SensorData {
    // Sensor data
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    int16_t mag_x, mag_y, mag_z;
    float pitch, roll, yaw;
    double gps_latitude, gps_longitude, gps_altitude;
    float baro_pressure, baro_temperature;

    float battery_voltage, battery_current;

    // Status flags
    bool uart_zero_connected, uart_gps_connected, i2c_connected;
};

enum LogType : uint8_t {
    Error,
    Info
};

struct LogData {
    //Log data
    LogType type;
    char message[30];
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

Total: 103 byte
*/
struct Message {
    static constexpr uint8_t START_MARKER = 0x7E;
    static constexpr uint8_t END_MARKER = 0x7E;

    MessageType type;

    union {
        SensorData sensor_data;
        LogData log_data;
        // Add other data structures here
    } data;

    uint16_t calculateChecksum(const uint8_t* data, size_t length) const {
        uint16_t checksum = 0;
        for (size_t i = 0; i < length; ++i) {
            checksum += data[i];
        }
        return checksum;
    }

    size_t serialize(uint8_t* buffer, size_t buffer_size) const {
        size_t data_size = 0;

        switch (type) {
            case MessageType::SensorData:
                data_size = sizeof(SensorData);
                break;
            case MessageType::LogData:
                data_size = sizeof(LogData);
                break;
            // Handle other message types here
        }

        if (buffer_size < 7 + data_size) return 0;

        // Byte 0
        buffer[0] = START_MARKER;

        // Calculate message length (type + data size)
        uint16_t message_length = sizeof(uint8_t) + data_size;

        // Bytes 1-2
        memcpy(buffer + 1, &message_length, sizeof(uint16_t));

        // Byte 3
        buffer[3] = static_cast<uint8_t>(type);

        // Bytes 4-(3+data_size)
        memcpy(buffer + 4, &data, data_size);

        uint16_t checksum = calculateChecksum(buffer + 3, message_length);

        // Bytes (4+data_size)-(5+data_size)
        memcpy(buffer + 4 + data_size, &checksum, sizeof(uint16_t));

        // Byte (6+data_size)
        buffer[6 + data_size] = END_MARKER;

        return 7 + data_size;
    }

    bool deserialize(const uint8_t* buffer, size_t buffer_size) {
        if (buffer[0] != START_MARKER || buffer[buffer_size - 1] != END_MARKER) return false;

        uint16_t message_length;
        memcpy(&message_length, buffer + 1, sizeof(uint16_t));

        if (buffer_size < 6 + message_length) return false;

        uint16_t checksum;
        memcpy(&checksum, buffer + 3 + message_length, sizeof(uint16_t));

        uint16_t calculated_checksum = calculateChecksum(buffer + 3, message_length);
        if (checksum != calculated_checksum) return false;

        memcpy(&type, buffer + 3, sizeof(uint8_t));

        size_t data_size = 0;
        switch (type) {
            case MessageType::SensorData:
                data_size = sizeof(SensorData);
                break;
            case MessageType::LogData:
                data_size = sizeof(LogData);
                break;
        }

        memcpy(&data, buffer + 4, data_size);

        return true;
    }
};
#endif