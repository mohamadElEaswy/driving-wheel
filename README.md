# ESP32-S3 MPU6050 steering wheel

A wooden wheel with an MPU-6050 GY-521 becomes one joystick axis. No force feedback, pedals, buttons, or Bluetooth.

Angle reading follows [datgai/ArduinoNano-GyroscopicSteering](https://github.com/datgai/ArduinoNano-GyroscopicSteering): `MPU6050_tockn`, `calcGyroOffsets(false)`, `update()`, then one `getAngle*` call.

Default output is a native USB HID gamepad, so Windows sees the wheel in `joy.cpl` without vJoy. Set `USE_USB_HID` to `0` in the sketch to fall back to IBus over serial, the same path as the reference project.

## Wiring


| GY-521 | ESP32-S3 SuperMini |
| ------ | ------------------ |
| SDA    | GPIO 8             |
| SCL    | GPIO 9             |
| VCC    | 3.3V               |
| GND    | GND                |


Leave the module's AD0 pin tied to GND (address `0x68`, which is what the library uses). Mount the board flat on the wheel face so turning the wheel spins the chip around its Z axis.

## Arduino IDE setup

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software).
2. Open settings, then paste the URL into **Additional boards manager URLs** at the bottom of the window. On macOS that is **Arduino IDE → Settings** (`Cmd+,`). On Windows it is **File → Preferences**.

   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

   If the field already has a URL, click the small icon at the right of the field and add this one on its own line. If **esp32 by Espressif Systems** already shows up in Boards Manager, skip this step.
3. Tools → Board → Boards Manager → install **esp32 by Espressif Systems**, version 3.x.
4. Tools → Manage Libraries → install **MPU6050_tockn** by tockn.

`USB.h` and `USBHIDGamepad.h` ship with the ESP32 core. No other libraries are required.

## Board settings (USB HID, the default)

Tools menu:


| Setting          | Value                |
| ---------------- | -------------------- |
| Board            | ESP32S3 Dev Module   |
| USB Mode         | USB-OTG (TinyUSB)    |
| USB CDC On Boot  | Enabled              |
| Upload Mode      | UART0 / Hardware CDC |
| USB DFU On Boot  | Disabled             |
| Flash Size       | 4MB (32Mb)           |
| PSRAM            | Disabled             |
| CPU Frequency    | 240 MHz              |
| Partition Scheme | Default              |




## Upload

1. Plug the board into the PC with a data-capable USB-C cable.
2. Tools → Port → the new COM port.
3. Open `driving-wheel.ino` and click Upload.

If the port never appears, or upload fails with "no serial data":

1. Hold **BOOT**.
2. Tap **RESET**.
3. Release **BOOT**.
4. Click Upload.
5. Press **RESET** when the upload finishes.

Keep the wheel still for a few seconds after every reset. `calcGyroOffsets(false)` blocks while it calibrates, and the angle at the end of that becomes steering center `0`.

## Test in Windows (`joy.cpl`)

1. Win+R, type `joy.cpl`, Enter.
2. Select the device named like **ESP32 Gamepad** (or similar) → Properties.
3. Turn the wheel left and right. The crosshair should move on the X axis only.
4. With `MAX_STEERING_ANGLE` at `90`, a 90° turn reaches the end of the axis. At `180`, you need a half turn each way.
5. Press **RESET** on the board, hold the wheel where you want center, and wait for calibration to finish. That position is `0`.

## Test in a game

Leave `USE_USB_HID` at `1`. Do not install VJoySerialFeeder for this. The game reads the same X axis that moves in `joy.cpl`. If that crosshair does not move, the game will not steer either.

1. Start the game and open its control settings.
2. Bind steering to the controller or joystick **X axis**. The device name is **ESP32 Gamepad**.
3. Turn the wheel. The game's steering should follow it.

Many new games only accept an Xbox controller, so they ignore this device even though `joy.cpl` sees it. If the game has no joystick binding:

1. Add the game to Steam if it is not already there.
2. Steam → Settings → Controller → enable generic gamepad support.
3. Right-click the game → Properties → Controller → Enable Steam Input.
4. Launch the game from Steam. It then sees an Xbox controller, and the wheel is the left stick's horizontal axis.

## Sketch options

At the top of `driving-wheel.ino`:

```cpp
#define MAX_STEERING_ANGLE 90   // 90 or 180 only
#define USE_USB_HID 1           // 0 = IBus serial fallback
#define STEERING_SOURCE 2       // 2 = whichever axis moves, 0 = Z, 1 = Y
#define DEADZONE_DEG 1.0f
```

- `MAX_STEERING_ANGLE` clamps the reading to `-limit`, `0`, `+limit`, then stretches that span across the full joystick X range (`-127` … `127`). The sketch will not compile with any other value.
- `STEERING_SOURCE 2` sends whichever of X, Y, or Z is farthest from center. Use this to find which way the chip has to turn.
- `STEERING_SOURCE 0` reads gyro Z (flat spin) and supports `±180`. It drifts slowly. Press RESET to re-center.
- `STEERING_SOURCE 1` reads `getAngleY()`, matching the reference sketch. The accelerometer holds it steady, but that axis only behaves when `MAX_STEERING_ANGLE` is `90`.



## Fallback: vJoy + VJoySerialFeeder

Use this only when the USB gamepad does not appear in `joy.cpl`. In the sketch set `USE_USB_HID` to `0`, and in Tools set **USB Mode** to **Hardware CDC and JTAG** (leave USB CDC On Boot enabled). Upload again.

1. Install [vJoy](https://sourceforge.net/projects/vjoystick/).
2. Open **Configure vJoy**. Enable device 1. Enable the X axis. Apply.
3. Download [VJoySerialFeeder for Windows](https://github.com/Cleric-K/vJoySerialFeeder/releases) (`vJoySerialFeeder_Windows_v1.7.1.zip`).
4. In VJoySerialFeeder:
  - Port: the ESP32 COM port
  - Baud rate: `115200`
  - Protocol: **IBUS**
  - Click **Connect**
5. Add an axis mapping:
  - vJoy device 1, axis **X**
  - Channel **1**
  - Setup: minimum `1000`, center `1500`, maximum `2000`
6. In `joy.cpl`, open **vJoy Device**. Turning the wheel moves X. Center of the wheel is `1500`.

Channel 1 is the only channel. Its value is `1500` at center and `1000` / `2000` at full lock.

## If something looks wrong

- **Axis goes the wrong way.** In `loop()`, change `angle` to `-angle` before the `constrain` call.
- **The crosshair slides left and right by itself.** The MPU-6050 was not found. Check 3.3V, GND, SDA to GPIO 8, SCL to GPIO 9, and AD0 tied to GND. Serial Monitor at 115200 prints `MPU6050 not found` in that case.
- **Axis never moves, and it does not slide by itself.** Open Serial Monitor at 115200 and turn the board. `x`, `y`, and `z` should change. If they change but `joy.cpl` stays still, unplug the USB cable and plug it back in.
- **Center walks away over several minutes.** That is gyro Z drift. Press RESET with the wheel straight, or set `STEERING_SOURCE` to `1` and `MAX_STEERING_ANGLE` to `90`.
- **No gamepad in** `joy.cpl`**.** USB Mode must be USB-OTG (TinyUSB), and the cable must carry data. Try another port.

