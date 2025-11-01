#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>
// ===== Data Message Structures =====

/**
 * @struct Message_from_Pad
 * @brief Structure for joystick data sent from remote controller to platform.
 */
typedef struct Message_from_Pad {
    uint32_t timeStamp = 0;            // Timestamp (millis) for heartbeat
    uint32_t messageSequenceNumber = 0;// Sequence number of messages sent

    int16_t L_Joystick_x_message = 0;  // Left joystick X-axis value
    int16_t L_Joystick_y_message = 0;  // Left joystick Y-axis value
    int16_t R_Joystick_x_message = 0;  // Right joystick X-axis value
    int16_t R_Joystick_y_message = 0;  // Right joystick Y-axis value

    uint32_t L_Joystick_buttons_message = 0; // Left joystick button states
    uint32_t R_Joystick_buttons_message = 0; // Right joystick button states
    
    int16_t L_Joystick_raw_x = 0;      // Raw sensor X-value for calibration
    int16_t L_Joystick_raw_y = 0;      // Raw sensor Y-value for calibration
    int16_t R_Joystick_raw_x = 0;      // Raw sensor X-value for calibration
    int16_t R_Joystick_raw_y = 0;      // Raw sensor Y-value for calibration
} Message_from_Pad;

/**
 * @struct Message_from_Platform_Mecanum
 * @brief Structure for telemetry data sent from platform to monitor.
 */
typedef struct Message_from_Platform_Mecanum {
    uint32_t timestamp = 0;            // Timestamp (millis) for heartbeat
    uint32_t totalMessages = 0;        // Total number of messages sent
    float frontLeftSpeedRPM = 0;       // Measured front-left wheel RPM
    float frontRightSpeedRPM = 0;      // Measured front-right wheel RPM
    float rearLeftSpeedRPM = 0;        // Measured rear-left wheel RPM
    float rearRightSpeedRPM = 0;       // Measured rear-right wheel RPM
    int64_t frontLeftEncoder = 0;      // Raw pulse count front-left encoder
    int64_t frontRightEncoder = 0;     // Raw pulse count front-right encoder
    int64_t rearLeftEncoder = 0;       // Raw pulse count rear-left encoder
    int64_t rearRightEncoder = 0;      // Raw pulse count rear-right encoder
    float pitch = 0;                   // IMU pitch angle (future use)
    float roll = 0;                    // IMU roll angle (future use)
    float yaw = 0;                     // IMU yaw angle (future use)
    float batteryVoltage = 0;          // Battery voltage reading (future use)
} Message_from_Platform_Mecanum;

#endif // MESSAGES_H