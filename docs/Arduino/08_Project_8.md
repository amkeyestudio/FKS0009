### Project 8: Servo Control

#### **1. Introduction:**

![A micro servo and its accessories](media/intro_08.jpg){width=520px}

During the obstacle-avoidance lesson, the Mini Tracked Tank V4.0 turned its head left and right before deciding where to go. That head turn is the work of a small servo: the ultrasonic module sits on a pan-tilt bracket above it, so when the servo turns, the sensor sweeps with it. It was centred before being screwed down so that "straight ahead" would match the middle of its range in the program.

A servo is not an ordinary motor. Give a motor power and it spins; cut the power and it stops. A servo measures its own angle with a potentiometer, compares it with a target, and fights to stay there — so you'll feel real resistance if you try to turn the horn by hand, and it springs back when you let go. The target doesn't arrive as a voltage level but as a pulse whose width carries the meaning, which is why getting the timing right matters more than getting the wiring right.

Here we sweep it from one end of its travel to the other and back, stepping smoothly instead of jumping. The tempting shortcut is to command the far end in one go: the pan-tilt still carries the ultrasonic module, and the sudden move makes the whole bracket wobble. Equally, don't assume the middle position points straight ahead — that depends on how the horn was fitted. If it's off, re-centre the mechanism rather than fudging the angle in code, or the distance readings from the earlier lesson will stay biased.

At power-on the servo gives a click as it snaps to one end, then sweeps steadily to the other, pauses briefly and sweeps back, over and over.

#### **2. Required Parts:**

Parts used in this lesson:

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_13.png){width=170px}<br><span style="font-size:0.85em">9G servo (180°, with cross horn) ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 motor driver expansion board ×1</span> | ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 development board ×1</span> |
> ⚠️ **The servo was already centred at 90° and installed in Chapter 2. This table is for reference only — do not remove it. The wiring notes below only show how the connections are made, for reference; no disassembly is needed. For the full parts list, see 1.4 Kit List.**
#### **3. Parameters:**

| Item | Parameter |
| --- | --- |
| Model | 9G micro servo, 180° travel |
| Signal pin | IO42 |
| Control frequency | 50 Hz (20 ms period) |
| Pulse width range | 500–2500 µs |
| Angle mapping | 500 µs ≈ 0°, 1500 µs ≈ 90°, 2500 µs ≈ 180° |
| Sweep step | One step per degree, 15 ms between steps |
| Full-travel time | About 2.7 s (180 steps × 15 ms) |

#### **4. Wiring:**

| Servo wire | Colour (typical) | Connected to |
| --- | --- | --- |
| Signal | Orange / yellow | IO42 |
| Power | Red | 5V |
| Ground | Brown / black | GND |

![](media/7201.png)

![](media/PRO8.png)


#### **5. Flowchart:**

A servo's **angle** isn't a number sent over the wire — it's the width of a HIGH pulse. One pulse arrives every 20 ms, staying HIGH for 0.5–2.5 ms, and the servo reads that duration as the target angle:

![](media/servo_pwm.png)

#### **6. Test Code:**

```cpp
#include <ESP32Servo.h>

const int PAN_SERVO_PIN = 42; // Pan-tilt servo signal
const int STEP_DELAY    = 15; // Delay per degree (ms); smaller is faster

Servo panServo;

void setup() {
  Serial.begin(115200);

  // Allocate hardware timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // 50 Hz, pulse width 500~2500 µs
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);

  // Return to zero
  panServo.write(0);
  Serial.println("Pan-tilt servo started, sweeping back and forth between 0° and 180°...");
  delay(1000);
}

void loop() {
  // 0° → 180°
  for (int pos = 0; pos <= 180; pos += 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // Pause 0.5 s

  // 180° → 0°
  for (int pos = 180; pos >= 0; pos -= 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // Pause 0.5 s
}
```

#### **7. Code Explanation:**

**(1) Two constants and a servo object**

```cpp
const int PAN_SERVO_PIN = 42; // Pan-tilt servo signal
const int STEP_DELAY    = 15; // Delay per degree (ms); smaller is faster

Servo panServo;
```

Pin 42 carries the pan-tilt servo's signal. `STEP_DELAY` is how long each step pauses during the degree-by-degree sweep — 15 ms was found by trial, and both loops below use it. `Servo panServo;` declares the servo object; every command goes through it.

**(2) The four `allocateTimer` lines are not optional**

```cpp
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
```

These four lines sit in `setup()`, before `attach()`. ESP32 PWM comes from hardware timers, and the servo wants 50 Hz while the motors want 1 kHz — both compete for the same pool. `allocateTimer()` reserves all four timers for the servo library; without these lines `attach()` may fail, showing up as a servo that never moves with no error in the Serial Monitor.

Handing over all four looks wasteful, but later two servos (pan-tilt + gripper) run together, each consuming a timer, so reserving them up front saves trouble.

**(3) `setPeriodHertz(50)` and the two numbers in `attach()`**

```cpp
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
```

50 Hz means a 20 ms period, the standard for all analogue servos. The last two arguments of `attach()` are the lower and upper pulse widths in microseconds: 0° maps to 500 µs, 180° to 2500 µs, with linear interpolation in between. The library does the maths, so `write()` takes only an angle.

Those two numbers depend on the servo model. With a 9G servo, 500/2500 sometimes hits the mechanical stop and produces a "tick-tick" stall sound; narrow them to 600/2400 and try again.

**(4) Return to zero at power-on**

```cpp
  panServo.write(0);
```

`write(0)` moves the pan-tilt to one end first, giving the sweep a defined starting point. Leave it out and the servo stays where it was when power was cut, so the first sweep starts from an unknown position.

Note that `write()` gives a target, it doesn't wait for arrival — it returns immediately while the servo moves there on its own. So after every large jump you have to add a `delay()`, or the next command will overwrite a move that hasn't finished.

One more thing: `write(90)` only moves the servo to the pulse-width midpoint. Whether that is "straight ahead" on the robot depends on how the horn was fitted. If the assembly is off, re-centre it as described in Chapter 2 instead of fudging the angle in code — otherwise the left and right distances measured in Lesson 4 will stay biased.

**(5) 0° → 180°: sweeping one degree at a time**

```cpp
  for (int pos = 0; pos <= 180; pos += 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
```

A direct `write(180)` sends the servo racing to the target at full speed; with the ultrasonic module still on the pan-tilt, the inertia makes the bracket wobble badly. One degree per step with 15 ms pauses takes about 2.7 seconds for the full 180°, runs smoothly, and makes it easy to see how angle and direction correspond by eye.

Lowering `STEP_DELAY` speeds it up, but below 10 ms the servo can't keep up and starts to stutter. Its real speed is set by the internal motor; changing the delay only changes how densely you issue commands, it can't actually make the servo faster.

**(6) 180° → 0°: sweeping back**

```cpp
  for (int pos = 180; pos >= 0; pos -= 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
```

The return is exactly symmetrical with the outbound sweep, only reversed. Together the two loops complete one round trip.

**(7) Two 500 ms pauses**

```cpp
  delay(500); // Pause 0.5 s
```

Half a second at each end lets the servo finish its last step and lets the ultrasonic module stop swinging. One cycle = 2.7 + 0.5 + 2.7 + 0.5 ≈ 6.4 seconds.

#### **8. What You'll See:**

At power-on the servo gives a **click** as it snaps to 0°, then begins its steady sweep toward 180°, and the Serial Monitor prints:

```text
Pan-tilt servo started, sweeping back and forth between 0 and 180 degrees...
```

| Stage | What happens |
| --- | --- |
| At power-on | Snaps to 0° (facing right), settles after a slight jitter |
| 0° → 180° | Sweeps in about 2.7 s, the ultrasonic sensor moving from right to left |
| At 180° | Pauses for 0.5 s |
| 180° → 0° | Sweeps back in the same 2.7 s |
| Loop | Repeats continuously |

> [Image to be added] One photo of the servo at each end of its travel.

**Note**: If the servo keeps buzzing at either end, cut the power immediately. Stall current can exceed 700 mA, dragging down the whole robot's 5V rail and resetting the controller. Change the 500/2500 in `attach()` to 600/2400 and try again.

See section 2.1 for how to re-centre it (needed before mounting or after replacing the servo); you can use `Servo_return_to_center_90.ino` or the corresponding `.py` to hold it at 90°.

#### **9. Troubleshooting:**

(1) The servo doesn't move at all at power-on: first check that the signal wire is on IO42. Wrong pin or no connection means no PWM, so no movement. If the Serial Monitor keeps restarting too, `allocateTimer()` is most likely missing and the timers are competing with the motor PWM.

(2) It moves in jerks: `STEP_DELAY` is too small and the servo can't keep up. Set it to at least 15 ms; below 10 ms it will stutter.

(3) It turns back after only half the sweep: a loop condition is wrong. If the 180 in `for (int pos = 0; pos <= 180; pos += 1)` was changed to 90, you only get half a sweep.

(4) It buzzes continuously at either end: it's stalled against a mechanical stop. Current can exceed 700 mA and drag down the whole robot's 5V. Change the 500/2500 in `attach()` to 600/2400 and try again.
