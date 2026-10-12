// Minimal steering wheel: ESP32-S3 SuperMini + MPU-6050 GY-521.
// Angle reading follows datgai/ArduinoNano-GyroscopicSteering (MPU6050_tockn).
//
// Default output is a native USB HID gamepad (Windows joy.cpl, no vJoy).
// Set USE_USB_HID to 0 to send one IBus channel over USB serial instead
// (VJoySerialFeeder + vJoy, same idea as the reference sketch).

#include <Wire.h>
#include <MPU6050_tockn.h>

// 90 or 180. The wheel is clamped to +/- this many degrees.
#define MAX_STEERING_ANGLE 90

// 1 = USB HID gamepad. 0 = IBus frames on Serial for VJoySerialFeeder.
#define USE_USB_HID 1

// 0 = gyro Z (wheel rotation, supports +/-180). 1 = getAngleY(), like the
// reference sketch (accelerometer-stabilized, only reliable to +/-90).
#define STEERING_AXIS_Y 0

// Ignore jitter smaller than this, in degrees. Set to 0 to disable.
#define DEADZONE_DEG 1.0f

#if MAX_STEERING_ANGLE != 90 && MAX_STEERING_ANGLE != 180
#error "MAX_STEERING_ANGLE must be 90 or 180"
#endif

#if USE_USB_HID
#include "USB.h"
#include "USBHIDGamepad.h"
USBHIDGamepad gamepad;
#endif

MPU6050 mpu6050(Wire);
float center = 0;

float readAngle() {
#if STEERING_AXIS_Y
  return mpu6050.getAngleY();
#else
  return mpu6050.getAngleZ();
#endif
}

#if !USE_USB_HID
// Same frame as the reference ibus.cpp: length, 0x40, little-endian channels,
// checksum = 0xFFFF minus every byte already sent.
void sendIbus(unsigned short value) {
  const byte len = 6;  // 4 + 1 channel * 2
  unsigned short checksum = 0xFFFF - len - 0x40;
  Serial.write(len);
  Serial.write((byte)0x40);
  byte lo = value & 0xFF;
  byte hi = value >> 8;
  Serial.write(lo);
  Serial.write(hi);
  checksum -= lo;
  checksum -= hi;
  Serial.write(checksum & 0xFF);
  Serial.write(checksum >> 8);
}
#endif

void setup() {
#if USE_USB_HID
  gamepad.begin();
  USB.begin();
#else
  Serial.begin(115200);
#endif

  // SDA = GPIO 8, SCL = GPIO 9.
  Wire.begin(8, 9);
  mpu6050.begin();
  // Keep the wheel still. This blocks for a few seconds.
  mpu6050.calcGyroOffsets(false);

  mpu6050.update();
  center = readAngle();
}

void loop() {
  mpu6050.update();

  float angle = readAngle() - center;
  if (fabs(angle) < DEADZONE_DEG) {
    angle = 0;
  }
  angle = constrain(angle, -(float)MAX_STEERING_ANGLE, (float)MAX_STEERING_ANGLE);

#if USE_USB_HID
  int axis = (int)(angle * 127.0f / MAX_STEERING_ANGLE);
  axis = constrain(axis, -127, 127);
  gamepad.leftStick((int8_t)axis, 0);
#else
  int channel = 1500 + (int)(angle * 500.0f / MAX_STEERING_ANGLE);
  channel = constrain(channel, 1000, 2000);
  sendIbus((unsigned short)channel);
#endif

  delay(10);
}
