### Project 4: Ultrasonic Obstacle Avoidance

#### **1. Introduction:**

![Car vacuum cleaner](media/intro_04.jpg)

The ultrasonic sensor as eyes, the servo as a neck, the motors as legs, and the dot matrix as a face — for the first time all four components appear together: the car looks at the road itself and decides for itself which way to go around.

There is only one rule: anything within 15 cm ahead counts as an obstacle. Beyond that line the car does not push on; it stops, turns the pan-tilt to each side to measure once, and heads toward whichever side is more open. Between the two measurements it waits 300 milliseconds so the servo can settle, and only then does the reading count.

Eight actions follow one after another: drive straight, measure, stop, look left, look right, compare, turn, return to center. Once the order is clear, writing the code is straightforward.

A single book standing in front of the car is enough to try it: when the way ahead is clear the car drives straight and the dot matrix shows a heart; push it to within 15 cm of the book and it stops, the exclamation mark flashes once, the pan-tilt first turns to one side and the dot matrix switches to the look-left icon, then it turns to the other side and switches to look-right. After comparing both sides it turns for half a second toward the side with more space, returns to center, and keeps going. Move the book to the other side of the car and it will go around in the opposite direction.

#### **2. Parts List:**
Components used in this lesson:

| Ultrasonic module ×1 | 9G servo (180°, with cross arm) ×1 | DRV8835 motor expansion board ×1 |
| :---: | :---: | :---: |
| ![](../media/kit_05.png) | ![](../media/kit_13.png) | ![](../media/kit_04.png) |
| Mini Tracked Tank Robot bottom assembly ×1 | 8×16 LED dot matrix display ×1 | ESP32-S3 development board ×1 |
| ![](../media/kit_02.png) | ![](../media/kit_06.png) | ![](../media/kit_03.png) |
> ⚠️ **These components were already installed on the car in Chapter 2. This table is for reference only, so there is no need to remove and reinstall them; the wiring notes below only describe the wiring method for reference and do not require disassembly. See the kit list in 1.4 for the full bill of materials.**

#### **3. Parameters:**
| Item | Parameter |
| --- | --- |
| Ultrasonic | Trig = IO13, Echo = IO12 |
| Pan-tilt servo | IO42, 50 Hz, pulse width 500–2500 µs corresponds to 0–180° |
| Obstacle threshold | CLOSE_DISTANCE = 15 (≤ 15 cm counts as an obstacle) |
| Left/right ranging angles | Left 180°, right 0°, return to center 90° after measuring |
| Servo settling delay | 300 ms after each turn |
| Straight-line speed | `car.forward(150, 150)` |
| Turning speed | `car.left(255, 255)` / `car.right(255, 255)` for 500 ms |
| Dot matrix pins | CLK = IO9, DIN = IO8 |
| Debug output | Thonny Shell (USB REPL, 115200) |

#### **4. Wiring:**

| Module | Pin | Controller GPIO |
| --- | --- | :---: |
| Ultrasonic | Trig / Echo | IO13 / IO12 |
| Pan-tilt servo | Signal | IO42 |
| Left motor (channel A AIN/AEN) | Direction / speed | IO38 / IO21 |
| Right motor (channel B BIN/BEN) | Direction / speed | IO40 / IO41 |
| Dot matrix | CLK / DIN | IO9 / IO8 |

> **Note**: the components in the wiring diagram are only there to explain the wiring relationship; when you wire it up, there is no need to take apart a module that has an enclosure.

> **Note**: MicroPython's `PWM` uses hardware channels, so the servo and the motors can output at the same time and there is no need to allocate a separate timer as in Arduino; after `car.Servo_init(42)` creates the channel, `car.Servo_set_angle()` can be called directly.

![](media/PRO4.png)

#### **5. Flowchart:**

The main loop has only one decision chain: first measure ahead, and if it is far enough, drive on; if not, stop, take a look left and right, turn toward the wider side, and go back to ranging.

![](media/avoid_flow.png)

#### **6. Test Code:**
> ⚠️ **Before uploading the code, upload `mini_tracked_tank_robot.py` to the `lib/` directory on the board** (see Section 5.5), otherwise importing the library raises `ImportError`.

```python
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Ultrasonic_init(13, 12)
car.Servo_init(42)
car.Matrix_init(9, 8)

CLOSE_DISTANCE = 15        # Anything closer than 15 cm counts as an obstacle

heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])
alert = bytes([
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F,
    0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
])
look_left = bytes([
    0x00, 0x00, 0x3C, 0x4E, 0x4E, 0x42, 0x3C, 0x00,
    0x00, 0x3C, 0x4E, 0x4E, 0x42, 0x3C, 0x00, 0x00
])
look_right = bytes([
    0x00, 0x00, 0x3C, 0x42, 0x4E, 0x4E, 0x3C, 0x00,
    0x00, 0x3C, 0x42, 0x4E, 0x4E, 0x3C, 0x00, 0x00
])

car.stop_motor()
car.Matrix_display(heart)

while True:
    distance = car.Ultrasonic_distance()
    print('Distance ahead: ', distance, 'cm')

    if distance <= CLOSE_DISTANCE:
        # Obstacle: stop first and show the exclamation mark
        car.stop_motor()
        car.Matrix_display(alert)
        time.sleep_ms(150)

        # Swing the pan-tilt servo to one side and measure that side
        car.Matrix_display(look_left)
        car.Servo_set_angle(180)
        time.sleep_ms(300)
        left_distance = car.Ultrasonic_distance()
        time.sleep_ms(100)

        # Swing the pan-tilt servo to the other side and measure that side
        car.Matrix_display(look_right)
        car.Servo_set_angle(0)
        time.sleep_ms(300)
        right_distance = car.Ultrasonic_distance()
        time.sleep_ms(100)

        car.Servo_set_angle(90)

        # Turn towards the side with more space
        if right_distance > left_distance:
            car.right(255, 255)
        else:
            car.left(255, 255)
        time.sleep_ms(500)
    else:
        # No obstacle: keep driving straight
        car.Matrix_display(heart)
        car.forward(150, 150)

    time.sleep_ms(100)
```

#### **7. Code Explanation:**
**(1) Importing the board-level library and initializing four paths**

```python
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Ultrasonic_init(13, 12)
car.Servo_init(42)
car.Matrix_init(9, 8)
```

After creating the `car` object, four components are initialized in turn: `car.Motor_init()` handles the wheels, `car.Ultrasonic_init(13, 12)` sets Trig/Echo, `car.Servo_init(42)` puts the pan-tilt servo signal pin on IO42, and `car.Matrix_init(9, 8)` sets the dot matrix CLK/DIN. This is the first lesson where all four appear together, and any component you forget to initialize simply will not move.

**(2) Obstacle threshold and font bitmaps**

```python
CLOSE_DISTANCE = 15        # Anything closer than 15 cm counts as an obstacle
```

How close **counts as an obstacle** is promoted to a constant, and changing it changes the sensitivity: a larger value makes the car dodge earlier, a smaller one makes it react only when closer. The four `bytes` font bitmaps below — heart, alert (exclamation mark), look_left, and look_right — correspond to the four expressions for driving straight, hitting an obstacle, and looking left and right, in the same format as Lesson 1. `car.stop_motor()` and `car.Matrix_display(heart)` before the loop make the car stop and show the heart first.

**(3) Ranging and branching**

```python
while True:
    distance = car.Ultrasonic_distance()
    print('Distance ahead: ', distance, 'cm')

    if distance <= CLOSE_DISTANCE:
```

Each round first reads the front distance with `car.Ultrasonic_distance()`, stores it in `distance`, and prints it to the Shell. Then `if distance <= CLOSE_DISTANCE` branches: far enough falls through to the `else` below and drives straight, close enough enters this branch and avoids the obstacle. The threshold is kept in a constant, so this line only compares; changing the sensitivity does not require touching the condition.

**(4) Stopping with an alert, and turning the pan-tilt to one side**

```python
        car.stop_motor()
        car.Matrix_display(alert)
        time.sleep_ms(150)

        # Swing the pan-tilt servo to one side and measure that side
        car.Matrix_display(look_left)
        car.Servo_set_angle(180)
        time.sleep_ms(300)
        left_distance = car.Ultrasonic_distance()
```

Once an obstacle is confirmed, `car.stop_motor()` stops the car and `car.Matrix_display(alert)` shows the exclamation mark. Then `car.Servo_set_angle(180)` turns the pan-tilt to the left side of the car, and `sleep_ms(300)` waits for the servo to reach the position before measuring — without that wait the servo is still turning and you measure the diagonal direction instead. The reading on this side is stored in `left_distance`, so the angle and the variable name correspond one-to-one and the side selection below can read it directly by name.

**(5) Measuring the other side and returning to center**

```python
        car.Matrix_display(look_right)
        car.Servo_set_angle(0)
        time.sleep_ms(300)
        right_distance = car.Ultrasonic_distance()
        time.sleep_ms(100)

        car.Servo_set_angle(90)
```

`car.Servo_set_angle(0)` turns to the other side to measure `right_distance`, again waiting 300 ms. `time.sleep_ms(100)` leaves an interval between the two ultrasonic measurements. After measuring, `car.Servo_set_angle(90)` returns the pan-tilt to center so that the ultrasonic sensor does not look sideways during the next straight run.

**(6) Comparing the two sides and turning towards the wider one**

```python
        if right_distance > left_distance:
            car.right(255, 255)
        else:
            car.left(255, 255)
        time.sleep_ms(500)
```

Whichever side reads larger is the more open one, so the car turns that way: `car.right(255, 255)` / `car.left(255, 255)` spin in place at full speed for 500 ms, and the longer the time the more it turns. The other `else` branch is `car.forward(150, 150)` for driving straight when there is no obstacle, with the heart kept on the dot matrix. `time.sleep_ms(100)` at the end of the loop sets the measurement rhythm.

#### **8. Expected Behavior:**
After power-on the pan-tilt returns to center, the dot matrix shows a heart, the car drives straight, and the Shell prints **Front distance: XX cm** every round:

1. **No obstacle** (distance > 15 cm): keep driving straight, heart on the dot matrix.
2. **Obstacle detected** (distance ≤ 15 cm): stop, the exclamation mark flashes on the dot matrix; the pan-tilt first turns to one side, the dot matrix shows the **look-left** icon and a distance is measured, then it turns to the other side, shows the **look-right** icon and measures again; after comparing the two distances it turns for 500 ms toward the side with more space, then returns to center and continues straight.

If the car can go around the obstacle ahead and does not keep pushing against it, and the dot matrix state changes with each action, it is working correctly.

> [Image to be added] Photos of the actual running behavior: one each for driving straight, stopping at an obstacle, looking left to measure, looking right to measure, and turning to go around.

> [Image to be added] A screenshot of the distance output in the Shell below Thonny, showing how the value changes before and after encountering an obstacle.

#### **9. Troubleshooting:**

(1) The way ahead is clearly open, yet the car stops, shows the exclamation mark, and looks left and right. This is because `car.Ultrasonic_distance()` returns 0 when the distance is out of range, and 0 also satisfies `distance <= CLOSE_DISTANCE` (15 cm), so it is treated as an obstacle. Change the condition to `0 < distance <= CLOSE_DISTANCE` to keep 0 out.

(2) The pan-tilt turned, but the measurement is not of the side. In most cases the distance was measured before the servo reached the position after `car.Servo_set_angle(180)` or `car.Servo_set_angle(0)`. Do not omit the `time.sleep_ms(300)` after each turn in the code: wait 300 ms for the pan-tilt to settle before reading, and then the reading matches the direction.

(3) The car tilts its head while driving straight and the ultrasonic sensor looks sideways, because `car.Servo_set_angle(90)` was forgotten after measuring the two sides. Add that line after comparing the two distances, and the next straight run looks straight ahead.

(4) A Shell `ImportError` means the library was not saved to the `lib/` directory on the board; if a pin-related `OSError` is reported, check whether the initialization pins IO13/IO12, IO42, and IO9/IO8 are being used twice, so that two components do not fight over the same line.
