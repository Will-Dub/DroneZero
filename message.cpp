#include <cstring>
#include <stdio.h>
#include <sstream>

#ifndef MESSAGE_H
#define MESSAGE_H


// Message types
enum class MessageType {
    SensorData,
    Command,
    StatusUpdate,
    Unknown
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

// Unified message structure
struct Message {
    static constexpr uint8_t START_MARKER = 0x7E;
    static constexpr uint8_t END_MARKER = 0x7E;

    MessageType type;
    union {
        SensorData sensor_data;
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
        if (buffer_size < sizeof(data) + 7) return 0;

        //Byte 0
        buffer[0] = START_MARKER;

        //Calcul longueur du message(type est cast a 1 byte ensuite)
        uint16_t message_length = sizeof(uint8_t) + sizeof(data);

        //Byte 1 et 2
        memcpy(buffer + 1, &message_length, sizeof(uint16_t));

        //Byte 3
        buffer[1 + sizeof(uint16_t)] = static_cast<uint8_t>(type);
        
        //Byte 4-99
        memcpy(buffer + 1 + sizeof(uint16_t) + sizeof(uint8_t), &data, sizeof(data));

        uint16_t checksum = calculateChecksum(buffer + 3, sizeof(uint8_t) + sizeof(data));
        //Byte 100, 101
        memcpy(buffer + 1 + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(data), &checksum, sizeof(uint16_t));

        //Byte 102
        buffer[1 + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(data) + sizeof(uint16_t)] = END_MARKER;

        return 1 + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(data) + sizeof(uint16_t) + 1;
    }

    bool deserialize(const uint8_t* buffer, size_t buffer_size) {
        if (buffer[0] != START_MARKER || buffer[buffer_size - 1] != END_MARKER) return false;

        uint16_t message_length;
        memcpy(&message_length, buffer + 1, sizeof(uint16_t));
        
        if (buffer_size < 3 + message_length + sizeof(uint16_t) + 1) return false;

        uint16_t checksum;
        memcpy(&checksum, buffer + 3 + message_length, sizeof(uint16_t));

        uint16_t calculated_checksum = calculateChecksum(buffer + 3, message_length);
        if (checksum != calculated_checksum) return false;

        memcpy(&type, buffer + 3, sizeof(uint8_t));
        memcpy(&data, buffer + 4, sizeof(data));
        
        return true;
    }
};
#endif