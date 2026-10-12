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

// 0 = gyro Z (flat spin, supports +/-180).
// 1 = getAngleY(), like the reference sketch (tilt, only reliable to +/-90).
// 2 = whichever of X/Y/Z is moving. Use this while testing the mount.
#define STEERING_SOURCE 2

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
float centerX = 0;
float centerY = 0;
float centerZ = 0;
bool mpuFound = false;

float pickAngle() {
  float x = mpu6050.getAngleX() - centerX;
  float y = mpu6050.getAngleY() - centerY;
  float z = mpu6050.getAngleZ() - centerZ;
#if STEERING_SOURCE == 1
  return y;
#elif STEERING_SOURCE == 0
  return z;
#else
  float angle = z;
  if (fabs(x) > fabs(angle)) angle = x;
  if (fabs(y) > fabs(angle)) angle = y;
  return angle;
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
#endif
  Serial.begin(115200);

  // SDA = GPIO 8, SCL = GPIO 9.
  Wire.begin(8, 9);
  Wire.beginTransmission(0x68);
  mpuFound = (Wire.endTransmission() == 0);

  if (!mpuFound) {
    Serial.println("MPU6050 not found at 0x68. Check 3.3V, GND, SDA=GPIO8, SCL=GPIO9.");
    return;
  }

  mpu6050.begin();
  // Keep the wheel still. This blocks for a few seconds.
  mpu6050.calcGyroOffsets(false);
  mpu6050.update();
  centerX = mpu6050.getAngleX();
  centerY = mpu6050.getAngleY();
  centerZ = mpu6050.getAngleZ();
  Serial.println("MPU6050 ready. Turn the wheel.");
}

void loop() {
  int axis;

  if (!mpuFound) {
    // Sensor missing: slide the stick so joy.cpl shows the gamepad itself works.
    axis = (int)((millis() / 8) % 254) - 127;
  } else {
    mpu6050.update();
    float angle = pickAngle();
    if (fabs(angle) < DEADZONE_DEG) {
      angle = 0;
    }
    angle = constrain(angle, -(float)MAX_STEERING_ANGLE, (float)MAX_STEERING_ANGLE);
    axis = (int)(angle * 127.0f / MAX_STEERING_ANGLE);
    axis = constrain(axis, -127, 127);

    static unsigned long lastLog = 0;
    if (millis() - lastLog > 200) {
      lastLog = millis();
      Serial.print("x ");
      Serial.print(mpu6050.getAngleX() - centerX);
      Serial.print("  y ");
      Serial.print(mpu6050.getAngleY() - centerY);
      Serial.print("  z ");
      Serial.print(mpu6050.getAngleZ() - centerZ);
      Serial.print("  axis ");
      Serial.println(axis);
    }
  }

#if USE_USB_HID
  gamepad.leftStick((int8_t)axis, 0);
#else
  int channel = 1500 + axis * 500 / 127;
  channel = constrain(channel, 1000, 2000);
  sendIbus((unsigned short)channel);
#endif

  delay(10);
}
