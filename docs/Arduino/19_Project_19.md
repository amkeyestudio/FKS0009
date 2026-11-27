### Project 19: AI Vision Module: Face Following

#### **1. Introduction:**

![A face marked with a calibration grid](media/intro_19.jpg)

Stand in front of the car and it turns to look at you; step left and it turns left; walk away and it comes after you. Lesson 5 used ultrasound for following, which only knew whether something was ahead and how far away. With a camera instead, it can tell which side of the frame the person is on.

Face mode reports one more thing than color mode: coordinates. The module gives the horizontal position of the face center in the frame, so it is immediately clear whether the person is off to the left or the right, and the differential speed of the left and right wheels is calculated from that offset.

The trouble is the scale. Different firmware reports coordinates over different ranges — some up to 100, others up to 320 pixels — so a hard-coded center works for one module and misses with the next. The fix is to stand directly in front of the lens for one second after power-on: the car averages the horizontal coordinates of the first twenty frames, treats that average as "straight ahead", and computes all later offsets relative to it.

Stand straight ahead and the car drives toward you; move left and the left wheel slows while the right speeds up, turning the car with you; step out of frame and after 300 ms it stops, the dot matrix changing from a smiley back to eyes.

#### **2. Parts List:**
Parts used in this lesson:

| TinyVision AI vision camera ×1 | 9G servo (180°) ×1 | DRV8835 motor expansion board ×1 |
| :---: | :---: | :---: |
| ![](../media/kit_09.jpg) | ![](../media/kit_13.png) | ![](../media/kit_04.png) |
| Mini tracked tank robot bottom assembly ×1 | 8×16 LED dot matrix display ×1 | ESP32-S3 development board ×1 |
| ![](../media/kit_02.png) | ![](../media/kit_06.png) | ![](../media/kit_03.png) |
#### **3. Parameters:**
| Item | Parameter |
| --- | --- |
| Recognition mode | `face` |
| Report format | `FACE:x,y` |
| Following speed | 100 |
| Calibration frames | 20 |
| Lost-target stop delay | 300 ms |
| Pan-tilt angle | 90° (fixed, not part of following) |

#### **4. Wiring:**

This lesson returns to mounting B: the module sits on the pan-tilt with the lens facing straight ahead, locked to the bracket as described in 2.2.9. `setup()` turns the pan-tilt to 90° and leaves it there; left/right following is handled by the differential speed of the two wheels — whichever side of the frame the target is on, the car corrects toward that side.

| Vision module | Controller | Description |
| --- | :---: | --- |
| TX | IO18 | Reports face coordinates |
| RX | IO17 | Sends mode commands |
| VCC / GND | 5V / GND | Power and common ground |

| Signal | Controller GPIO |
| --- | :---: |
| Pan-tilt servo | IO42 (fixed at 90°) |
| Left wheel AIN / AEN | IO38 / IO21 |
| Right wheel BIN / BEN | IO40 / IO41 |
| Dot matrix CLK / DIN | IO9 / IO8 |

**Mounting B:**

![](media/A-1.png)

![](media/B-2.png)

![](media/IMG_9095.jpg)

![](media/B-3.png)

#### **5. Flowchart:**

Calibrate for 20 frames at power-on; following begins only after calibration:

![](media/face_follow_flow.png)

#### **6. Test Code:**
> **This lesson's project has three files**: `19_Face_Following.ino`, `AiCam.h`, and `AiCam.cpp`. All serial traffic and string parsing for the vision module live in `AiCam.cpp`; put all three files in the same folder — compilation fails if any is missing.

```cpp
#include <ESP32Servo.h>
#include "AiCam.h"  // Vision module driver

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Pan-tilt servo =====
#define PAN_SERVO_PIN 42  // Lesson 8

// ===== Vision module serial port =====
// Controller RX <- module TX, controller TX -> module RX, common ground required
#define CAM_RX 18  // Controller RX pin
#define CAM_TX 17  // Controller TX pin

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connect to module CLK
#define DIN_Pin 8  // Data, connect to module DIN

// ===== Following parameters =====
#define PAN_CENTER     90  // Pan-tilt fixed facing straight ahead
#define FOLLOW_SPEED  100  // Forward speed while following
#define KP            0.5  // Proportional coefficient
#define TURN_MAX       80  // Maximum one-side correction
#define DEAD_ZONE       8  // Alignment dead zone
#define CALI_FRAMES    20  // Number of calibration frames
#define LOST_STOP     300  // Loss timeout (ms)

// ===== Debug switches =====
// 1 prints the calibrated center value, 0 silences it
#define DEBUG_OUT 1

// ===== Expression numbers =====
#define FACE_EYES  0  // No person: eyes
#define FACE_SMILE 1  // Person seen: smiley

// 16 bytes correspond to 16 columns, each byte's 8 bits correspond to 8 rows in one column

// Standby: two square eyes
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smiley: person seen
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // Vision module instance
Servo panServo;               // Pan-tilt servo

int centerX = -1;             // Calibrated center X, -1 means not done
long sumX = 0;                // Calibration accumulator
int caliCount = 0;            // Calibration frame counter
unsigned long lastSeen = 0;   // Last time a person was detected
int curFace = -1;             // Current expression, -1 means not shown

void setup() {
#if DEBUG_OUT
  Serial.begin(115200);
#endif

  // Pull both lines HIGH to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // Show standby expression

  // Pan-tilt: 50 Hz, pulse width 500-2500 µs
  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(PAN_CENTER);  // Face straight ahead, no rotation in this lesson
  delay(500);

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // Start the vision module in face detection mode
  aiCam.begin();
  aiCam.setMode("face");

  delay(500);  // Wait for the mode switch to complete
}

void loop() {
  aiCam.readSerial();  // Read the serial port each cycle

  if (!aiCam.isFaceValid()) {
    // No person detected: stop on timeout
    showFace(FACE_EYES);
    if (millis() - lastSeen > LOST_STOP) {
      stopCar();  // Stop on timeout
    }
    delay(20);
    return;
  }

  // Person detected
  lastSeen = millis();   // Refresh detection time
  showFace(FACE_SMILE);  // Show smiley

  int x = aiCam.getFaceX();  // Face center X

  // Calibrate straight ahead
  // The module's X range is unknown (0-100 or 0-320 pixels),
  // Instead average the first 20 frames, do not hardcode the center
  if (centerX < 0) {
    sumX += x;
    caliCount++;
    stopCar();  // Stop during calibration

    if (caliCount >= CALI_FRAMES) {
      centerX = (int)(sumX / caliCount);  // Average is the straight-ahead X
#if DEBUG_OUT
      Serial.print("Frame center locked: ");
      Serial.println(centerX);
#endif
    }

    delay(20);
    return;
  }

  // Follow
  int err = x - centerX;  // Error: positive means right, negative means left

  if (abs(err) <= DEAD_ZONE) {
    // Aligned: move straight forward
    goForward(FOLLOW_SPEED);
  } else {
    // Not aligned: turn with differential speed
    // When err is positive (person on the right), left fast and right slow
    int correction = constrain((int)(err * KP), -TURN_MAX, TURN_MAX);
    driveMotors(FOLLOW_SPEED + correction, FOLLOW_SPEED - correction);
  }

  delay(20);  // 20 ms per cycle
}

// ===== Dot matrix display =====
// Only refresh the display when the expression changes
void showFace(int face) {
  if (face == curFace) return;  // Expression unchanged
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);   // Standby
  } else {
    matrix_display(smile);  // Smiley
  }
}

// Write the 16-byte font bitmap into the dot matrix display buffer
void matrix_display(unsigned char matrix_value[]) {
  // 0x40: write data mode, auto-increment address
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0: display buffer start address 0
  IIC_start();
  IIC_send(0xC0);

  // Write 16 columns consecutively
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8A: display on, maximum brightness
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// Start condition: falling edge of DIN while CLK is HIGH
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// Send one byte, LSB first: set data while CLK is LOW, sample on rising edge
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);  // Pull clock LOW to prepare data
    delayMicroseconds(3);

    // Send bit by bit starting from bit0
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // Pull clock HIGH for the module to sample
    delayMicroseconds(3);

    send_data = send_data >> 1;  // Shift right to get the next bit
  }
}

// Stop condition: rising edge of DIN while CLK is HIGH
void IIC_end() {
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
}

// ===== Motor driver =====
// Channel A drives the left wheel, channel B drives the right wheel (same as Lesson 7)
// leftSpeed goes to channel A, rightSpeed goes to channel B
// First limit to ±255, then take the absolute value for the duty cycle
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // Left wheel limit
  rightSpeed = constrain(rightSpeed, -255, 255);  // Right wheel limit

  // Channel B: right wheel
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // Pull HIGH: forward rotation
    ledcWrite(MOTOR_BEN, rightSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // Pull LOW: reverse rotation
    ledcWrite(MOTOR_BEN, -rightSpeed); // Take absolute value
  }

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // Pull HIGH: forward rotation
    ledcWrite(MOTOR_AEN, leftSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // Pull LOW: reverse rotation
    ledcWrite(MOTOR_AEN, -leftSpeed); // Take absolute value
  }
}

// Basic actions
void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels forward at the same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels reverse at the same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Turn left: left wheel back, right wheel forward
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Turn right: left wheel forward, right wheel back
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle to zero
```

#### **7. Code Explanation:**
**(1) Six parameters**

```cpp
#define FOLLOW_SPEED  100  // Forward speed while following
#define KP            0.5  // Proportional coefficient
#define TURN_MAX       80  // Maximum one-side correction
#define DEAD_ZONE       8  // Alignment dead zone
#define CALI_FRAMES    20  // Number of calibration frames
#define LOST_STOP     300  // Loss timeout (ms)
```

`FOLLOW_SPEED 100` is slower than the 180 of earlier lessons — following a person does not need speed. `KP 0.5` has only a proportional term, no derivative — following a person is simpler than line tracking because the error itself is continuous, so there is no overshoot to suppress. The remaining four are safeguards and calibration, explained one by one below.

**(2) Five global states**

```cpp
int centerX = -1;             // Calibrated center X, -1 means not done
long sumX = 0;                // Calibration accumulator
int caliCount = 0;            // Calibration frame counter
unsigned long lastSeen = 0;   // Last time a person was detected
int curFace = -1;             // Current expression, -1 means not shown
```

The initial `centerX = -1` doubles as the "not calibrated yet" flag — which is why the test is `centerX < 0`, not a specific value. `sumX` and `caliCount` are used only during calibration and ignored afterwards. `lastSeen` is the timing origin for the lost-target grace period.

**(3) At the top of `loop()`: handle "no face seen" first**

```cpp
  if (!aiCam.isFaceValid()) {
    // No person detected: stop on timeout
    showFace(FACE_EYES);
    if (millis() - lastSeen > LOST_STOP) {
      stopCar();  // Stop on timeout
    }
    delay(20);
    return;
  }
```

When no face is detected it does **not** stop immediately — instead it checks how long it has been since the last sighting. The `return` ends the round right there, without touching the calibration or following logic below.

`showFace(FACE_EYES)` sits inside that test but outside the timeout check, so the expression changes as soon as the face is lost, without waiting for the timeout.

**(4) A 300 ms grace period: a hand wave should not stop the car**

```cpp
    if (millis() - lastSeen > LOST_STOP) {
      stopCar();  // Stop on timeout
    }
```

During those 300 ms it keeps moving at the previous round's speed (no command is issued this round, so the motors simply hold their state), and only then stops.

The grace period matters: the module occasionally misses a frame or two when the head turns slightly or the lighting changes. Stopping instantly would make the car jerk forward in stops and starts. 300 ms is about 15 frames, enough to filter out almost all momentary misses.

**(5) Refresh the timestamp whenever a face is seen**

```cpp
  lastSeen = millis();   // Refresh detection time
  showFace(FACE_SMILE);  // Show smiley

  int x = aiCam.getFaceX();  // Face center X
```

`lastSeen` is refreshed on every sighting, so the 300 ms means "how long since the face was last seen", not "how long since it was first lost". This line comes after the validity check and runs only when a face really is visible in this frame.

**(6) Calibration: average the first 20 frames**

```cpp
  if (centerX < 0) {
    sumX += x;
    caliCount++;
    stopCar();  // Stop during calibration

    if (caliCount >= CALI_FRAMES) {
      centerX = (int)(sumX / caliCount);  // Average is the straight-ahead X
```

The `x` returned by the module is "the face center's coordinate in the frame", but **there is no unified scale** — some firmware reports 0–100, others 0–320 pixels. Hard-coding `centerX = 160` breaks completely with a different module.

So instead, at startup the operator stands directly in front of the lens and the average of the first 20 frames becomes the center. Twenty frames is roughly 0.4 seconds — enough to filter single-frame jitter, while not making you stand and wait too long.

Both `stopCar()` and `return` are inside the calibration branch, so the car stays still during calibration and the rest of the following logic is skipped this round.

Why can't this step be skipped? The camera sits on the pan-tilt, and 90° is not necessarily the middle of the frame; add lens distortion and the "centered" position of a face is often several pixels away from 160. Hard-coding 160 means that even with a person standing straight ahead, the car keeps drifting to one side.

**(7) Dead zone: once aligned, stop correcting**

```cpp
  // Follow
  int err = x - centerX;  // Error: positive means right, negative means left

  if (abs(err) <= DEAD_ZONE) {
    // Aligned: move straight forward
    goForward(FOLLOW_SPEED);
```

`DEAD_ZONE = 8`: an error within ±8 counts as aligned, so the car just drives forward.

What happens without a dead zone? Face coordinates jump every frame — even with a person standing still, `x` fluctuates by ±3. The car would keep nudging left and right, rocking in place instead of moving. The dead zone absorbs those few pixels of noise.

**(8) Differential speed and limits**

```cpp
  } else {
    // Not aligned: turn with differential speed
    // When err is positive (person on the right), left fast and right slow
    int correction = constrain((int)(err * KP), -TURN_MAX, TURN_MAX);
    driveMotors(FOLLOW_SPEED + correction, FOLLOW_SPEED - correction);
  }
```

A person on the right of the frame (`err` positive) means the nose is pointing left, so turn right — speed up the left wheel, slow the right; `+correction` to the left wheel is exactly that direction.

`TURN_MAX = 80` follows the same reasoning as the ±90 in the previous lesson: `FOLLOW_SPEED` is 100, so a correction reaching 100 would produce a negative speed and one wheel reversing. Limiting to 80 leaves 20 of margin, keeping both wheels turning forward — the car "corners" rather than "spins in place".

**(9) `delay(20)`: 50 Hz**

```cpp
  delay(20);  // 20 ms per cycle
}
```

Twenty milliseconds per round is 50 Hz, close to the actual frame rate of face detection. Going faster only re-reads the same frame and wastes CPU; going slower cannot keep up with the person, and the car chases in jerks.

The skeleton of this lesson is almost the same as Lesson 18's line tracking: `readSerial()` → check validity → compute the error → differential speed → `delay()`. The differences are in three places:

| | Lesson 18 line tracking | Lesson 19 following |
| --- | --- | --- |
| Valid flag | `valid == 5` | `isFaceValid()` |
| Center value | Module gives the offset directly | Calibrate 20 frames at startup and average |
| Lost target | Search in the remembered direction, 600 ms | Keep going straight for 300 ms, then stop |

Lesson 18 has to "search" when the line is lost because the line does not move; Lesson 19 has to "stop" when the person is lost because you cannot catch someone who has walked away — chasing them would be dangerous.

#### **8. Expected Behavior:**

> The tests for this project used an AI face image

![](media/face.jpg)

With `DEBUG_OUT` enabled, the Serial Monitor prints:

```text
Vision module serial port started
Send -> GET:MODE
Send -> GET:OUTPUT
Send -> MODE:FACE
Frame center locked: 78
```

`78` is the "straight ahead" coordinate for this particular module in its current position.

| What you do | Dot matrix | Car |
| --- | --- | --- |
| Stand in front of the lens for 1 s | Smiley | Stopped (calibration) |
| Stand straight ahead | Smiley | Drives straight toward you |
| Move to the left | Smiley | Left wheel slower, right wheel faster, turns left |
| Move to the right | Smiley | Turns right |
| Leave the frame | Eyes | Stops after 300 ms |

> [Image to be added] A screenshot of the Serial Monitor printing **Frame center locked**.

#### **9. Troubleshooting:**

(1) The car follows too eagerly and weaves: lower `KP` from 0.5 to 0.3, or raise `DEAD_ZONE` from 8 to 15.

(2) The car is sluggish and only moves after the person has gone: raise `KP` to 0.8, or increase `FOLLOW_SPEED` from 100 to 130.

(3) Do not skip calibration: stand still in front of the lens and wait for **Frame center locked** before moving. If you move around during calibration the center value is skewed, and the car will keep looking at you sideways.
