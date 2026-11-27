### Project 11: IR Remote Control Car

#### **1. Introduction:**

![RC car clearing an obstacle](media/intro_11.jpg)

Give the car a remote: press a direction key and it moves, release and it stops. But infrared is one-way — the car can't answer back — so how do you know it still hears you? Add a 1.5 s watchdog: every valid key restarts the timer, and if no new command arrives before it runs out, the car stops by itself.

Hold a key down and the remote doesn't send fresh commands. It sends short repeat markers that only say "still this key", with no hint of how long you've held it. The car uses those markers for one thing only — refreshing the timer — and never repeats the action.

The four direction keys map to four motions: up and down drive both tracks at the same speed forward or backward; left and right drive the two sides in opposite directions to spin in place; OK stops immediately.

Press up and the car moves forward; let go and about 1.5 s later it stops on its own, and the serial output goes quiet too.

#### **2. Parts List:**
Parts used in this lesson:

| 17-key IR remote control (JMFP-4) ×1 | IR receiver ×1 | DRV8835 motor expansion board ×1 |
| :---: | :---: | :---: |
| ![](../media/kit_12.png) | ![](../media/ir_receiver.png) | ![](../media/kit_04.png) |
| ESP32-S3 development board ×1 | Mini tracked tank robot bottom assembly ×1 |  |
| ![](../media/kit_03.png) | ![](../media/kit_02.png) |  |
> ⚠️ **All parts were installed back in Chapter 2; this table is for reference only — there is nothing to take apart. The wiring notes below only show how the connections are made, for reference; no assembly or disassembly is required. For the complete parts list, see 1.4 Kit Parts List.**
#### **3. Parameters:**
| Item | Parameter |
| --- | --- |
| Receiver pin | IO48 |
| Straight travel speed | 180 |
| Spin-in-place speed | 180 |
| Command timeout | 1500 ms |
| Key codes | Up 70, Down 21, Left 68, Right 67, OK 64 |
| Loop period | No delay; throttled by `decode()` itself |

#### **4. Wiring:**

| Signal | Controller GPIO |
| --- | :---: |
| IR receiver OUT | IO48 |
| Left wheel AIN / AEN | IO38 / IO21 |
| Right wheel BIN / BEN | IO40 / IO41 |

#### **5. Flowchart:**

The main loop does two independent things: handle a signal if there is one, and check the timeout if there isn't. Neither blocks the other, so waiting for a key never freezes the car's response:

![](media/ir_car_flow.png)

#### **6. Test Code:**
```cpp
#include <IRremote.hpp>  // Infrared (IR) decoding library

// ===== Pins =====
#define IR_RECEIVE_PIN 48  // IR receiver, IO48

#define MOTOR_AIN 38  // Channel A direction
#define MOTOR_AEN 21  // Channel A speed control
#define MOTOR_BIN 40  // Channel B direction
#define MOTOR_BEN 41  // Channel B speed control

// ===== Speed =====
#define SPEED_RUN  180  // Straight travel
#define SPEED_TURN 180  // Spin in place

// ===== Remote control key values =====
// After changing the remote control, update these values to match the Serial Monitor output
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64

// Auto-stop after command timeout (ms) to avoid losing control if the remote disconnects
#define STOP_AFTER 1500

unsigned long lastKeyTime = 0;  // Timestamp of the last valid key press

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("IR remote control car started; control it with the direction pad");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  stopCar();  // Stop on power on
}

void loop() {
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

    if (holding) {
      // A long press only refreshes the timeout timer
      lastKeyTime = millis();
    } else if (isKnownKey(cmd)) {
      handleKey(cmd);
      lastKeyTime = millis();
    }

    IrReceiver.resume();
  }

  // Stop if no valid key press for more than 1.5 s
  if (millis() - lastKeyTime > STOP_AFTER) {
    stopCar();
  }
}

// Key handling
void handleKey(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:
      Serial.println("Forward");
      goForward(SPEED_RUN);
      break;
    case KEY_DOWN:
      Serial.println("Backward");
      goBackward(SPEED_RUN);
      break;
    case KEY_LEFT:
      Serial.println("Turn left");
      spinLeft(SPEED_TURN);
      break;
    case KEY_RIGHT:
      Serial.println("Turn right");
      spinRight(SPEED_TURN);
      break;
    case KEY_STOP:
      Serial.println("Stop");
      stopCar();
      break;
    default:
      break;
  }
}

// Whether the key is a defined key
bool isKnownKey(uint8_t cmd) {
  return cmd == KEY_UP || cmd == KEY_DOWN || cmd == KEY_LEFT ||
         cmd == KEY_RIGHT || cmd == KEY_STOP;
}

// ===== Motor driver =====
// Channel A drives the left wheel, channel B drives the right wheel, same as Lesson 7
// If left and right are reversed, swap the two sections below
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // Channel B: right wheel
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);
  }

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels rotate forward at the same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels rotate in reverse at the same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Turn left: left wheel backward, right wheel forward
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Turn right: left wheel forward, right wheel backward
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle set to zero
```

#### **7. Code Explanation:**
**(1) Pins, speed and key codes**

```cpp
// ===== Pins =====
#define IR_RECEIVE_PIN 48  // IR receiver, IO48

#define MOTOR_AIN 38  // Channel A direction
#define MOTOR_AEN 21  // Channel A speed control
#define MOTOR_BIN 40  // Channel B direction
#define MOTOR_BEN 41  // Channel B speed control

// ===== Speed =====
#define SPEED_RUN  180  // Straight travel
#define SPEED_TURN 180  // Spin in place

// ===== Remote control key values =====
// After changing the remote control, update these values to match the Serial Monitor output
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64
```

A value of 180 is roughly a 71% duty cycle — higher than the 115 used for line tracking, because remote control wants "move the instant you press" rather than the fine corrective margin line tracking needs.

The five key codes are exactly the same as in Project 10; they were copied from that lesson's serial output. Change the remote and you'll have to copy them again.

**(2) Two independent things in `loop()`**

```cpp
void loop() {
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

    if (holding) {
      // A long press only refreshes the timeout timer
      lastKeyTime = millis();
    } else if (isKnownKey(cmd)) {
      handleKey(cmd);
      lastKeyTime = millis();
    }

    IrReceiver.resume();
  }

  // Stop if no valid key press for more than 1.5 s
  if (millis() - lastKeyTime > STOP_AFTER) {
    stopCar();
  }
}
```

The two `if` statements are siblings, not nested. When no IR arrives the first `if` is simply skipped and the second still runs — that's what "non-blocking" means.

If you moved the timeout check inside `if (decode())`, it would never run whenever no key is pressed and the car would keep charging ahead with its last command. Structurally, this is the most important point in the lesson.

**(3) A long press only refreshes the timer, never repeats the action**

```cpp
    if (holding) {
      // A long press only refreshes the timeout timer
      lastKeyTime = millis();
    } else if (isKnownKey(cmd)) {
      handleKey(cmd);
      lastKeyTime = millis();
    }
```

While you hold a direction key, the remote sends a repeat frame every 108 ms. Calling `handleKey()` for every frame would re-issue "forward" nine times a second, flooding the serial output and resetting the motor command over and over.

Here a long press does exactly one thing: push `lastKeyTime` forward so the timeout doesn't fire. The action itself was already executed on the frame when the key went down.

**(4) Why `isKnownKey()` is its own function**

```cpp
bool isKnownKey(uint8_t cmd) {
  return cmd == KEY_UP || cmd == KEY_DOWN || cmd == KEY_LEFT ||
         cmd == KEY_RIGHT || cmd == KEY_STOP;
}
```

The remote also has number keys, volume keys and a bunch of undefined buttons. Pressing those makes `handleKey()` fall through to `default: break;` and do nothing — but they **should not keep the car alive**.

That's why the timestamp is refreshed only when `isKnownKey()` is true: press a useless key and the car still stops after 1.5 seconds. This is a safety design, not a redundant check.

**(5) The five `case`s in `handleKey()`**

```cpp
void handleKey(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:
      Serial.println("Forward");
      goForward(SPEED_RUN);
      break;
```

One key maps to one action, and every `case` ends with `break`. Forget the `break` and execution "falls through" into the next `case` — press forward and you'd end up running the backward code.

`default: break;` catches every undefined code — a second layer of protection, since `isKnownKey()` has already filtered those keys out.

**(6) `lastKeyTime` and the 1.5 seconds**

```cpp
unsigned long lastKeyTime = 0;  // Timestamp of the last valid key press
```

`millis()` returns milliseconds since power on as an `unsigned long`, so this variable must use the same type; an `int` would overflow into negatives in roughly 24 days.

```cpp
#define STOP_AFTER 1500
```

IR is one-way communication: the remote sends and that's the end of it, so the car has no idea whether the key arrived. Releasing the button, a dead remote, walking out of range and strong sunlight interference all look identical from the car — no more frames.

1.5 seconds is a compromise between "a delay a person can feel" and "the car won't run too far". A real car could shorten it to 300 ms to be safer, but you'd feel a stutter when changing direction.

Holding a direction key works for the same reason: each frame restarts the timer, frames arrive every 108 ms so the counter can never reach 1500 ms; once you let go there are no more frames, the timer fills up and the car stops.

#### **8. Expected Behavior:**

After power-on the serial port prints the startup message for the IR remote control car. Pressing a direction key prints:

| Key | Serial | Car |
| --- | --- | --- |
| ↑ | Forward | Both wheels rotate forward at the same speed |
| ↓ | Backward | Both wheels rotate in reverse at the same speed |
| ← | Turn left | Left wheel reverses, right wheel goes forward — spin in place |
| → | Turn right | Left wheel goes forward, right wheel reverses |
| OK | Stop | Stops immediately |

About 1.5 s after you release the key, the car stops by itself (nothing more is printed, because `stopCar()` is called silently in the timeout branch).

> [Image to be added] Continuous screenshots of the serial output for the five key actions.

#### **9. Troubleshooting:**

(1) Nothing happens at all and the serial port prints no action: the key codes don't match. Macros such as `KEY_UP = 70` were set for the supplied remote; a different remote sends different codes. First read the actual values printed in Project 10, then edit these macros to match.

(2) The actions don't match the keys — press **Up** and the car goes backward: the left and right motor wires are reversed, or the two arguments of `goForward()` were swapped. Test the motors individually using the method from Project 2 and confirm that channel A drives the left wheel.

(3) The range gets shorter and some presses are missed: the receiver is partly hidden by the car shell. It senses from the side, so mounting it at the front edge of the chassis, facing forward and unobstructed, works best.

(4) The car keeps running after you release the key: the timeout check has failed. `STOP_AFTER` is 1500 ms; check that the `if (millis() - lastKeyTime > STOP_AFTER)` line in `loop()` hasn't been commented out.

(5) One press moves the car once and then nothing: `IrReceiver.resume()` isn't being called. After the library receives a frame it stops and waits for you to process it; without `resume()` it only ever receives the first frame.
