### Project 5: Ultrasonic Following

#### **1. Introduction:**

![A delivery robot following a person](media/intro_05.jpg){width=520px}

The car in Lesson 4 goes around an obstacle; this lesson flips that and makes it actively follow a person. Hold your hand 40 cm in front of its nose and it comes after you; pull your hand back and it keeps coming; bring it to within 10 cm and it backs away instead; take the hand away and it stops with a question mark popping up on the dot matrix.

The eye is still the ultrasonic module on the pan-tilt, firing a pulse forward every 100 ms and computing the distance from the echo. That one number is then split into four bands: 30 to 50 cm, follow; 10 to 30 cm, wait; under 10 cm, back up; beyond 50 cm or no valid reading, treat the target as lost.

Each band maps to one action and one expression — heart, smiley, down arrow, question mark. With no remote control and no line on the floor, the car decides whether to advance or retreat from this single number; push your hand toward the car and pull it back again, and you can watch it switch back and forth between the four bands.

#### **2. Required Parts:**

Parts used in this lesson:

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_05.png){width=170px}<br><span style="font-size:0.85em">Ultrasonic module (with casing) ×1</span> | ![](../media/kit_06.png){width=170px}<br><span style="font-size:0.85em">8×16 LED dot matrix display ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 motor driver expansion board ×1</span> |
| ![](../media/kit_02.png){width=170px}<br><span style="font-size:0.85em">Tracked chassis assembly (tracks + motors + wheels) ×1</span> | ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 development board ×1</span> |  |
> ⚠️ **These parts were already installed on the robot in Chapter 2. This table is for reference only — do not remove and reinstall them. The wiring notes below only show how the connections are made, for reference; no disassembly is needed. For the full parts list, see 1.4 Kit List.**
#### **3. Parameters:**

| Item | Parameter |
| --- | --- |
| Following range | 30–50 cm, move forward |
| Stop range | 10–30 cm, stop and show a smiling face |
| Back-up range | Less than 10 cm, back up |
| Target lost | Greater than 50 cm or invalid reading, stop and show a question mark |
| Distance measurement period | 100 ms |
| Forward speed | 150 for each wheel |
| Back-up speed | Left wheel 150, right wheel 105 (the robot drifts slightly when backing up) |
| Serial baud rate | 115200 |
| Dot matrix icons | Heart = following, smiling face = stop, down arrow = back up, question mark = lost |

#### **4. Wiring:**

| Module | Pin | Controller GPIO |
| --- | --- | :---: |
| Ultrasonic | Trig | IO13 |
| Ultrasonic | Echo | IO12 |
| Dot matrix | CLK | IO9 |
| Dot matrix | DIN | IO8 |
| Left motor | AIN (direction) | IO38 |
| Left motor | AEN (speed) | IO21 |
| Right motor | BIN (direction) | IO40 |
| Right motor | BEN (speed) | IO41 |

> **Note**: The components in the wiring diagram are shown only to illustrate the connections. When wiring the robot, you do not need to remove the casings of the components listed above.

![](media/PRO5.png)

#### **5. Flowchart:**

The main loop is a single chain of decisions, with one set of actions for each range:

![](media/follow_flow.png)

#### **6. Test Code:**

```cpp
// ===== Dot matrix display pins =====
#define CLK_Pin 9   // Clock, connect to dot matrix CLK
#define DIN_Pin 8   // Data, connect to dot matrix DIN

// ===== Ultrasonic pins =====
const int trigPin = 13;  // Trigger pin, connected to IO13
const int echoPin = 12;  // Echo pin, connected to IO12

// ===== Distance ranges (cm) =====
// Following is divided into ranges by three thresholds
const long safeDistance = 30;   // 30-50: following
const long closeDistance = 10;  // Less than 10: back up
const long maxDistance = 50;    // Greater than 50: target lost

// ===== Motor pins =====
#define MOTOR_AIN 38  // Left motor direction
#define MOTOR_AEN 21  // Left motor speed
#define MOTOR_BIN 40  // Right motor direction
#define MOTOR_BEN 41  // Right motor speed

// ===== Font bitmaps (16 columns x 8 rows) =====
// Heart: follow forward
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// Smiling face: distance OK, stop
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Down arrow: too close, back up
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7e, 0xfe,
  0xfe, 0x7e, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// Question mark: target lost
unsigned char question_mark[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x51,
  0x59, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Serial.println("Ultrasonic following started");

  // Initialize dot matrix pins and clear the display
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  pinMode(trigPin, OUTPUT);  // Trig output
  pinMode(echoPin, INPUT);   // Echo input

  // Direction pin output
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);

  // Speed pin PWM: 1 kHz, 8-bit
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  // Stop and show smiling face on power on
  stopMotor();
  matrix_display(smile);
}

void loop() {
  // Measure the distance ahead
  long distance = getDistance();

  Serial.print("Current distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > safeDistance && distance <= maxDistance) {
    // 30-50 cm: follow forward
    forward(150, 150);

  } else if (distance >= closeDistance && distance <= safeDistance) {
    // 10-30 cm: stop
    stopMotor();
    matrix_display(smile);

  } else if (distance < closeDistance && distance > 0) {
    // Less than 10 cm: back up
    // Differential left/right speed; the car drifts slightly when backing up
    back(105, 150);

  } else {
    // Greater than 50 cm or invalid reading: stop and show a question mark
    stopMotor();
    matrix_display(question_mark);
  }

  // Measure once every 100 ms
  delay(100);
}

// Measure distance, return centimeters
int getDistance() {
  long duration;  // Round-trip time (µs)
  int distance;   // Distance (cm)

  // Pull low first for a clean start level
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // 10 µs HIGH pulse triggers the wave
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Echo HIGH width is the round-trip time
  duration = pulseIn(echoPin, HIGH);

  // Speed of sound 0.034 cm/µs, divide by 2 to remove the return trip
  distance = duration * 0.034 / 2;

  // Out of 2-300 cm range, set to 0
  if (distance < 2 || distance > 300) {
    distance = 0;
  }

  return distance;
}

// ===== Motor control =====

// Directly write left/right wheel PWM (0-255)
void setMotor(int rightSpeed, int leftSpeed) {
  ledcWrite(MOTOR_BEN, rightSpeed);
  ledcWrite(MOTOR_AEN, leftSpeed);
}

// Forward: direction pin HIGH and show heart
void forward(int rightSpeed, int leftSpeed) {
  matrix_display(heart);
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(rightSpeed, leftSpeed);
}

// Backward: direction pin LOW and show down arrow
void back(int rightSpeed, int leftSpeed) {
  matrix_display(arrow_down);
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(rightSpeed, leftSpeed);
}

// Stop: set speed to 0
void stopMotor() {
  setMotor(0, 0);
}

// ===== Dot matrix driver (software-simulated I2C) =====

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
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(3);

    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(3);

    send_data = send_data >> 1;
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
```

#### **7. Code Explanation:**

**(1) Three thresholds define four ranges**

```cpp
const long safeDistance = 30;   // 30-50: following
const long closeDistance = 10;  // Less than 10: back up
const long maxDistance = 50;    // Greater than 50: target lost
```

Three numbers cut distance into four ranges: less than 10 backs up, 10–30 stops, 30–50 follows forward, and above 50 counts as target lost. The order of those numbers is the entire logic; change one and you have to rethink whether the neighbouring ranges still meet cleanly.

**(2) Distance measurement reuses the Lesson 3 approach**

```cpp
  // Measure the distance ahead
  long distance = getDistance();
```

This line sits at the top of `loop()` and runs once per cycle. Inside, `getDistance()` is word-for-word the same as in Lesson 3: pull low → 10 µs pulse → `pulseIn()` → ×0.034÷2 → out of range set to 0. A return of 0 means the same thing — either no echo or too close — and is equally untrustworthy.

**(3) Four `if` checks are tested top to bottom; only one runs**

```cpp
  if (distance > safeDistance && distance <= maxDistance) {
    // 30~50 cm: follow forward
    forward(150, 150);

  } else if (distance >= closeDistance && distance <= safeDistance) {
    // 10~30 cm: stop
    stopMotor();
    matrix_display(smile);

  } else if (distance < closeDistance && distance > 0) {
    // Less than 10 cm: back away
    // Different left/right speeds make the car drift slightly when backing
    back(105, 150);

  } else {
    // Greater than 50 cm or invalid reading: stop and show question mark
    stopMotor();
    matrix_display(question_mark);
  }
```

The four checks are linked into a chain by `else if`: as soon as an earlier condition is true, nothing below it is examined. So **the order cannot be changed** — move "less than 10" to the top and it will swallow part of the 30–50 range before that range ever gets a turn.

Every condition states both bounds (`> 30` paired with `<= 50`, not just an upper limit), so the value 30 falls only into the second range and never into two at once.

The final `else` catches two cases: a distance above 50 cm, or **a reading of 0**. Note the deliberate `distance > 0` in the third check — without it, 0 cm satisfies `distance < closeDistance`, and the robot would treat "open space with no echo" as "an obstacle right in its face" and jerk backwards. With `> 0` in place, 0 falls through to `else`, which shows the question mark and stays put — the most conservative way to handle untrustworthy data.

**(4) `setMotor()` only writes PWM; it never touches direction**

```cpp
void setMotor(int rightSpeed, int leftSpeed) {
  ledcWrite(MOTOR_BEN, rightSpeed);
  ledcWrite(MOTOR_AEN, leftSpeed);
}
```

This is the low-level layer with one job: write two absolute values to PWM. It never touches a direction pin, so both arguments must be positive — `ledcWrite()` given a negative number overflows into a very large value.

Compared with `driveMotors()` in Lesson 2, there is **no sign check** here. Direction is left to the higher level: `forward()` pulls both direction pins HIGH, `back()` pulls them LOW.

**(5) Direction pins are set inside `forward()` / `back()`**

```cpp
void forward(int rightSpeed, int leftSpeed) {
  matrix_display(heart);
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(rightSpeed, leftSpeed);
}

void back(int rightSpeed, int leftSpeed) {
  matrix_display(arrow_down);
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(rightSpeed, leftSpeed);
}
```

The order is "set direction first, then speed, and swap the icon along the way." Both direction pins move together, so the robot can only go straight forward or straight back — this lesson has no turning.

If you call only `setMotor()` while the direction pins still hold their previous values, the robot runs in last cycle's direction — the most common trap when debugging.

**(6) Left and right get different values when backing up**

```cpp
    back(105, 150);
```

The differential isn't a typo. With equal speeds the robot backs straight until the obstacle is completely out of sight, then stops abruptly. Giving 105 to the right and 150 to the left makes the nose drift to one side while it retreats, so it comes away at an angle and is more likely to see a clear path on the next reading.

Lesson 4's obstacle avoidance was "stop, look left, look right"; this lesson is "back away while veering." Both approaches are valid.

**(7) The 100 ms beat**

```cpp
  delay(100);
```

One cycle takes 100 ms, ten measurements per second. If a target approaches at 1 m/s, it moves only 10 cm between readings — smaller than the narrowest range (10 cm) — so it cannot skip a whole range. Push `delay` beyond 300 ms and a fast-approaching object jumps straight from "following" to "in your face", never stopping in the middle range.

#### **8. What You'll See:**

After power-on the dot matrix shows a smiling face first, and the Serial Monitor prints a line every 100 ms:

```text
Ultrasonic following started
Current distance: 42 cm
Current distance: 41 cm
Current distance: 18 cm
Current distance: 7 cm
Current distance: 0 cm
```

Make four moves in front of the robot and you'll see four different responses:

| Hand movement | Distance reading | Robot behaviour | Dot matrix |
| --- | --- | --- | --- |
| Held at 30–50 cm | 30–50 | Follows forward | Heart |
| Held at 10–30 cm | 10–30 | Stops | Smiling face |
| Brought within 10 cm | Less than 10 | Backs up | Down arrow |
| Hand removed | 0 or above 50 | Stops | Question mark |

> [Image to be added] A screenshot of the four distance readings in the Serial Monitor, showing the switch between following, stopping and backing up.

If the robot lunges forward as you approach, first check that the two ultrasonic signal wires aren't reversed: swap Trig and Echo and the reading stays at 0.

#### **9. Troubleshooting:**

(1) The closer your hand gets, the more the robot lunges forward: Trig and Echo are reversed, so the reading is always 0. Swap the two signal wires in the 4P connector according to the wiring table.

(2) Your hand hovers around 30 cm and the robot jerks between forward and stop: the thresholds are packed too tightly. Leave enough margin between `safeDistance` and `maxDistance`.

(3) The robot keeps showing a question mark and won't move: the distance in the Serial Monitor is 0 or above 50 cm, and both fall into the final `else`. Put your hand between 10 and 50 cm and try again; to follow from further away, raise `maxDistance` to 60.

(4) The robot always veers to one side when backing up: the left/right difference in `back(105, 150)` is deliberate, giving it an angle as it retreats. It's not a fault.
