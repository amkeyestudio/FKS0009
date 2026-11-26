### Project 7: Line Tracking

#### **1. Introduction:**

Put the Mini Tracked Tank V4.0 on a black track, press reset, and let go. The mini tracked tank robot completes a lap on its own: quick on the straights, easing the inner wheel back into line through gentle curves, and pivoting in place when a right-angle corner demands it. The five readings you only watched in the last lesson now actually drive the motors.

Which way the line lies depends entirely on the five sensors under the car. The five 0s and 1s combine into 32 patterns, of which you usually meet about a dozen in real running, and each maps to one set of wheel speeds: on the center line both wheels at the same speed; slightly off, slow the inner wheel and let the outer one run; further off, reverse the inner wheel to twist the body back.

Losing the line has a remedy too. A seam, a reflection, or the nose lifting in a corner can make all five sensors read white at once; the car remembers whether the line was last on its left or its right and searches in that direction for a further 1.5 s — if it finds the line it carries on, and only if it does not does it stop.

With the car running straight along the line, push it sideways and it steers itself back; cut a section out of the line and it charges on in the old direction, only stopping after 1.5 s.

#### **2. Parts List:**
Parts used in this lesson:

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_02.png)<br><span style="font-size:0.85em">Mini tracked tank robot bottom assembly ×1</span> | ![](../media/kit_04.png)<br><span style="font-size:0.85em">DRV8835 motor expansion board ×1</span> | ![](../media/kit_03.png)<br><span style="font-size:0.85em">ESP32-S3 development board ×1</span> |
| ![](../media/kit_39.png)<br><span style="font-size:0.85em">5-way IIC line-tracking sensor module ×1</span> |  |  |
> ⚠️ **This lesson adds no new parts — everything was already installed in Chapter 2, so there's nothing to remove. The wiring notes below only show how the connections are made, for reference; no disassembly is needed. For the full parts list, see 1.4 Kit List.**
#### **3. Parameters:**
| Item | Parameter |
| --- | --- |
| Straight-line speed | 115 |
| Fine-adjustment speed | Inner wheel 85, outer wheel 115 |
| Sharp-turn speed | Inner wheel 45, outer wheel 125 |
| Spin-in-place speed | Inner wheel -95 (reverse), outer wheel 125 (forward) |
| Line-loss memory timeout | 1500 ms |
| Control period | 5 ms |
| Speed range | −255 to +255 (negative means reverse) |

#### **4. Wiring:**

| Signal | Controller GPIO |
| --- | :---: |
| SDA (line tracking) | IO8 |
| SCL (line tracking) | IO9 |
| Left wheel AIN / AEN | IO38 / IO21 |
| Right wheel BIN / BEN | IO40 / IO41 |

#### **5. Flowchart:**

The five-bit `state` value is the fork in the road for the whole program — look at this diagram before reading the `switch`:

![](media/track_car_flow.png)

#### **6. Test Code:**
```cpp
#include <Wire.h>
#include <PCF8574.h>

PCF8574 pcf8574(0x20);

#define OUTA P4  // Far left
#define OUTB P0  // Left
#define OUTC P1  // Center
#define OUTD P2  // Right
#define OUTE P3  // Far right

#define MOTOR_AIN 38  // Channel A direction
#define MOTOR_AEN 21  // Channel A PWM
#define MOTOR_BIN 40  // Channel B direction
#define MOTOR_BEN 41  // Channel B PWM

// ===== Line tracking speed =====
#define BASE_SPEED   115  // Base speed for straight travel
#define RECOVER_IN   95   // Inner wheel reverse speed during sharp turns
#define RECOVER_OUT  125  // Outer wheel forward speed during sharp turns

// ===== State memory =====
uint8_t lastDir = 0;            // 0 straight, 1 line on left, 2 line on right
unsigned long lostStartTime = 0;// Timestamp when the line was lost

// Left/right motor mapping: channel A drives the left wheel, channel B drives the right wheel
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);
  }

  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  pinMode(MOTOR_BEN, OUTPUT);
  pinMode(MOTOR_AEN, OUTPUT);

  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  pcf8574.begin();
  pcf8574.pinMode(OUTA, INPUT);
  pcf8574.pinMode(OUTB, INPUT);
  pcf8574.pinMode(OUTC, INPUT);
  pcf8574.pinMode(OUTD, INPUT);
  pcf8574.pinMode(OUTE, INPUT);

  driveMotors(0, 0);
  delay(2000);
}

void loop() {
  PCF8574::DigitalInput val = pcf8574.digitalReadAll();

  int A = !val.p4; // Far left
  int B = !val.p0; // Left
  int C = !val.p1; // Center
  int D = !val.p2; // Right
  int E = !val.p3; // Far right

  uint8_t state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E;

  // Reset the lost-line timer as soon as a sensor is on the line
  if (state != 0b00000) {
    lostStartTime = 0;
  }

  switch (state) {
    // Centered, go straight
    case 0b00100:
    case 0b01110:
    case 0b11111:
      driveMotors(BASE_SPEED, BASE_SPEED);
      lastDir = 0;
      break;

    // Slightly left of the line: reduce left wheel speed for fine adjustment
    case 0b01100: // B and C on the line
      driveMotors(85, BASE_SPEED); // Slow the left wheel to steer back onto the black line
      lastDir = 1;
      break;

    // Left of the line: major correction
    case 0b01000: // Only B on the line
    case 0b11100: // Sharp left turn
      driveMotors(45, RECOVER_OUT);
      lastDir = 1;
      break;

    // Far left: spin in place to the left
    case 0b11000:
    case 0b10000:
      driveMotors(-RECOVER_IN, RECOVER_OUT);
      lastDir = 1;
      break;

    // Slightly right of the line: reduce right wheel speed for fine adjustment
    case 0b00110: // C and D on the line
      driveMotors(BASE_SPEED, 85); // Slow the right wheel to steer back onto the black line
      lastDir = 2;
      break;

    // Right of the line: major correction
    case 0b00010: // Only D on the line
    case 0b00111: // Sharp right turn
      driveMotors(RECOVER_OUT, 45);
      lastDir = 2;
      break;

    // Far right: spin in place to the right
    case 0b00011:
    case 0b00001:
      driveMotors(RECOVER_OUT, -RECOVER_IN);
      lastDir = 2;
      break;

    // All sensors off the line: search in the remembered direction
    case 0b00000:
      if (lostStartTime == 0) {
        lostStartTime = millis(); // Record the moment the line was lost
      }
      // Search in the remembered direction within 1.5 s
      if (millis() - lostStartTime < 1500) {
        if (lastDir == 1) {
          driveMotors(-RECOVER_IN, RECOVER_OUT); // Line was on the left, turn left to find it
        } else if (lastDir == 2) {
          driveMotors(RECOVER_OUT, -RECOVER_IN); // Line was on the right, turn right to find it
        } else {
          driveMotors(0, 0); // Was going straight, stop
        }
      } else {
        // Timeout with no line found, stop
        driveMotors(0, 0);
      }
      break;

    default:
      break;
  }

  delay(5);
}
```

#### **7. Code Explanation:**
**(1) Read five channels and pack them into one byte**

```cpp
  int A = !val.p4; // Far left
  int B = !val.p0; // Left
  int C = !val.p1; // Center
  int D = !val.p2; // Right
  int E = !val.p3; // Far right

  uint8_t state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E;
```

`A` is the far left and `E` the far right. The left shifts arrange the five channels as the bits `ABCDE`, and `|` packs them into a single byte. "Which channels see the line" now becomes one number from 0 to 31, so `switch` can branch directly on it instead of writing five nested `if`s.

For example, `0b00100` means only the center channel C is on the line (normal straight travel), while `0b11000` means the far-left and second channels are both on it (well off to the left).

**(2) Three speed constants**

```cpp
#define BASE_SPEED   115  // Base speed for straight travel
#define RECOVER_IN   95   // Inner wheel reverse speed during sharp turns
#define RECOVER_OUT  125  // Outer wheel forward speed during sharp turns
```

`BASE_SPEED 115` is roughly a 45% duty cycle — the normal line-following speed. On a sharp turn the inner wheel gets `-95` (reverse) and the outer `125` (forward); that 30-point gap pulls the nose back. All three numbers were tuned by trial and depend on the car's weight, the floor and the battery level.

**(3) `driveMotors()` takes its arguments in the opposite order from the last lesson**

```cpp
void driveMotors(int leftSpeed, int rightSpeed) {
```

Lesson 2 used `driveMotors(rightSpeed, leftSpeed)`; **here the left wheel comes first**. The two functions look almost identical but their argument order is reversed. If you copy the code and change only the body without checking the signature, left and right swap: going straight still works (both values are the same), but the first turn goes the wrong way. Always glance at the signature before calling it.

**(4) Clear the line-loss timer as soon as there's a signal**

```cpp
  if (state != 0b00000) {
    lostStartTime = 0;
  }
```

As long as any channel still sees the line, the timer is reset. Only when all five are 0 and execution reaches `case 0b00000` is the current moment stored in `lostStartTime` — so that variable always records when this particular loss of the line began.

The other memory variable, `lastDir`, isn't set here; it's assigned at the end of each `switch` branch below and exists solely to decide which way to search when all five channels read 0.

**(5) Centered and fine adjustment**

```cpp
    case 0b00100:
    case 0b01110:
    case 0b11111:
      driveMotors(BASE_SPEED, BASE_SPEED);
      lastDir = 0;
      break;

    case 0b01100: // B and C on the line
      driveMotors(85, BASE_SPEED); // Slow the left wheel to steer back onto the black line
      lastDir = 1;
      break;
```

Three "centered" states are merged into one case: only C on the line, B+C on the line, and all five on the line (crossing a wide strip of black tape) are all treated as straight travel.

When it drifts slightly left (B and C on the line), the left wheel drops from 115 to 85 while the right stays put, so the nose naturally swings back left onto the line. The differential here is only 30 — a gentle correction.

**(6) Sharp turns: reverse the inner wheel**

```cpp
    case 0b01000: // Only B on the line
    case 0b11100: // Sharp left turn
      driveMotors(45, RECOVER_OUT);
      lastDir = 1;
      break;

    case 0b11000:
    case 0b10000:
      driveMotors(-RECOVER_IN, RECOVER_OUT);
      lastDir = 1;
      break;
```

As the error grows, two stages take over: first the left wheel drops to 45 and the right rises to 125; if it gets worse (channel A is on the line too), the left wheel reverses at 95 while the right runs forward at 125, spinning the car left on the spot.

The bigger the error, the harder the correction. A single fixed differential causes two problems: too small and the car sails off the bend; too large and the slightest wobble on a straight makes it yank the wheel, tracing an "S". Three levels strike a balance between those extremes.

The three right-side groups mirror the left side: swap the two wheels and the numbers stay the same.

**(7) All five at 0: search in the remembered direction for 1.5 seconds**

```cpp
    case 0b00000:
      if (lostStartTime == 0) {
        lostStartTime = millis(); // Record the moment the line was lost
      }
      // Search in the remembered direction within 1.5 s
      if (millis() - lostStartTime < 1500) {
        if (lastDir == 1) {
          driveMotors(-RECOVER_IN, RECOVER_OUT); // Line was on the left, turn left to find it
        } else if (lastDir == 2) {
          driveMotors(RECOVER_OUT, -RECOVER_IN); // Line was on the right, turn right to find it
        } else {
          driveMotors(0, 0); // Was going straight, stop
        }
      } else {
        // Timeout with no line found, stop
        driveMotors(0, 0);
      }
      break;
```

Losing the line is common on a track — a seam, a reflection, or the nose lifting in a corner can make all five channels read 0 for an instant. Stop immediately and the car lurches along, starting and stopping repeatedly. Give it 1.5 seconds to keep turning in the direction it was heading and it usually finds the line itself; only when that time runs out with no line found has it really left the track, and then it stops.

That's where `lastDir` comes in: 1 means the line was just on the left, 2 means on the right, and 0 means it had been going straight. Without it, the car has no idea which way to turn after losing the line and can only stop where it stands.

That 1.5 seconds was tuned by trial: too short and a normal corner is mistaken for a lost line; too long and the car spins uselessly for ages after it has run off before finally stopping.

**(8) One cycle every 5 ms**

```cpp
  delay(5);
```

Two hundred decisions per second is far faster than the car can actually turn, so corrections never lag. Remove the delay and the I2C bus gets saturated with reads, which slows the overall rhythm instead.

#### **8. Expected Behavior:**

This lesson's code does not print anything to the Serial Monitor while running (there is no `Serial.print` in `setup()` or `loop()`, so the 5 ms rhythm is left entirely to the motors); simply observe the car following the line.

| Situation | Behavior |
| --- | --- |
| Centered on the line | Straight ahead at speed 115 |
| Gentle curve | Inner wheel slows, the body eases smoothly back onto the line |
| Right-angle turn | Inner wheel briefly reverses, the car pivots on the spot |
| Ran off the end of the line | Turns on the spot to search; if it finds the line within 1.5 s it carries on |
| Completely off the line | Stops after 1.5 s |

> [Image to be added] A photo of the car completing a full lap on the A2 line-tracking track (one shot on a curve and one on a straight would be ideal).

#### **9. Troubleshooting:**

(1) The car weaves on straights: lower `BASE_SPEED` from 115, try 100 for example, and check that the sensors aren't too far off the ground — mounted high, the reflection is weak and the direction drifts.

(2) It cuts the inside and runs off on a gentle curve: raise `RECOVER_OUT` from 125 to 135; the faster outer wheel brings the body back onto the line.

(3) It pivots on a right-angle turn but can't complete it: raise `RECOVER_IN` from 95 to 110 for more power on the reversing side.

(4) The whole car turns the wrong way: first check whether the A and B motor wires are reversed — swapping the two motors' 2P plugs is enough, with no code change. If the wiring is fine, look at the two arguments of `driveMotors()`, which puts the left wheel first.

(5) After a corner the car spins aimlessly and can't find the line: when all five channels read 0 it searches for 1.5 seconds in the direction remembered in `lastDir`, then stops. If this section often loses the line, raise the search time from 1500 to 2000.
