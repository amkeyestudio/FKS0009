# 2. Product Setup

- Servo centering code:
   - [Arduino centering code: Servo_return_to_center_90.ino](./Servo_return_to_center_90.7z)
   - [MicroPython centering code: Servo_return_to_center_90.py](./Servo_return_to_center_90.7z)

## 2.1 Before You Start

Before assembling, read the following carefully — it will save you rework and damaged parts:

1. **Center the servos first**: Power the pan-tilt and gripper servos and return them to the middle position (90°) before fitting the horns and linkages. If you install them while powered but off-center, the horn angle may not match the mechanical center, and the mechanism will either fall short of its travel or bind.

> If you are an experienced Arduino user, you can download the servo centering code directly, connect it following the wiring diagram, and upload the code.

![](media/PRO8.png)

> If you are new to Arduino, read 4.2 Installing the Arduino IDE first.
> Make sure the servos are centered before installing them.

2. **Peel off all protective film**: Remove the film from both sides of the acrylic plates, the surface of the dot matrix display, and the motors and sensors. Left in place, it spoils the look and makes adhesive pads stick poorly.

![](./media/image-20250709092645945.png)

3. **Check the parts**: Compare what you have against the kit list in 1.4. Sort screws and standoffs by size so you don't mix them up and strip the threads.
4. **Leave the batteries out**: Do not install batteries during assembly and wiring. Fit them only after everything has been checked. The kit does not include batteries — you need to buy them yourself.

> ⚠️ The kit does not include batteries — you need to buy them yourself!

## 2.2 Assembly Steps

### STEP 1:

![](media/step02_plate_parts.jpg)

![](media/step03_plate_assembly.jpg)

![](media/step04_plate_done.jpg)

### STEP 2:

![](media/step05_chassis_parts.jpg)

![](media/step06_chassis_assembly.jpg)

![](media/step07_chassis_done.jpg)

### STEP 3:

![](media/step08_gripper_parts.jpg)

![](media/step09_gripper_assembly.jpg)

![](media/step10_gripper_done.jpg)

### STEP 4:

![](media/step11_battery_box_layout.jpg)

![](media/step12_battery_box_panel.jpg)

![](media/step13_battery_box_combined.jpg)

![](media/step14_battery_box_mount.jpg)

![](media/step15_battery_box_fixed.jpg)

### STEP 5:

![](media/step16_body_assembly.jpg)

![](media/step16a_body_top.jpg)

![](media/step16b_servo_side.jpg)

![](media/step17_body_done.jpg)

### STEP 6:

![](media/step18_matrix_parts.jpg)

![](media/step19_matrix_align.jpg)

![](media/step20_matrix_done.jpg)

### STEP 7:

![](media/step21_servo_parts.jpg)

![](media/step22_servo_assembly.jpg)

![](media/step23_servo_done.jpg)

### STEP 8:

![](media/step24_battery_case_parts.jpg)

![](media/step25_battery_case_assembly.jpg)

![](media/step26_battery_case_done.jpg)

### STEP 9:

![](media/step26a_bracket_parts.jpg)

![](media/step26b_standoff_assembly.jpg)

![](media/step26c_standoff_servo.jpg)

### STEP 10:

![](media/step27_top_prep.jpg)

![](media/step28_top_assembly.jpg)

![](media/step29_top_done.jpg)

### STEP 11:

![](media/step30_sensor_prep.jpg)

![](media/step31_camera_align.jpg)

![](media/step32_camera_done.jpg)

### STEP 12:

![](media/step36_control_parts.jpg)

![](media/step37_control_assembly.jpg)

![](media/step38_control_done.jpg)

![](media/step39_control_parts2.jpg)

![](media/step40_control_assembly2.jpg)

![](media/step41_control_camera_done.jpg)

> Assembling up to here completes Projects 1–15 of the course. The steps below are the additional assembly required by the different projects from 16 to 20.

### STEP 13 Project 16: DIY Soccer Competition Car Assembly

![](media/step42_arm_assembly.jpg)

![](media/step43_arm_done.jpg)

### STEP 14 Method A Assembly for Projects 17 and 18

![](media/step44_full_overview.jpg)

![](media/step45_screen_parts.jpg)

![](media/step46_screen_semi.jpg)

### STEP 15 Method B Assembly for Projects 19 and 20

![](media/step49_body_side.jpg)

![](media/step50_camera_module.jpg)

![](media/step51_final_with_camera.jpg)

## 2.3 Wiring Notes

> Pin assignments follow the silkscreen on the actual boards. Always disconnect power before wiring, and check everything before powering on.

### 2.3.1 Motors

Connect the left and right motors to channels A and B of the expansion board respectively. Each channel has a direction pin and a speed (PWM) pin. Left and right are defined as seen standing behind the car looking forward: left hand is the left wheel, right hand the right wheel; channels A/B follow the expansion board silkscreen (AIN/AEN, BIN/BEN). The mapping:

| Motor | Channel (silkscreen) | Direction pin | Speed pin (PWM) |
| --- | :---: | :---: | :---: |
| Left motor (physical left wheel) | A (AIN/AEN) | IO38 | IO21 |
| Right motor (physical right wheel) | B (BIN/BEN) | IO40 | IO41 |

> If forward becomes backward or the steering is reversed, check channels A/B and the wheel wiring against the table before changing any names in code. You can flash `00_Motor_Mapping_Test` to drive one channel at a time and see which wheel turns, confirming the mapping.

### 2.3.2 Power

Connect the battery holder's red wire (positive) to the expansion board's positive power input and the black wire (negative) to the negative input — never reverse them. The whole robot is powered through the expansion board: motors and servos draw from it directly, while the controller is fed through its regulator.

### 2.3.3 8×16 Dot Matrix

The display needs only two signal lines and uses a two-wire (I2C-like) driver:

| Display pin | Controller GPIO |
| --- | :---: |
| CLK | IO9 |
| DIN | IO8 |

### 2.3.4 Line-Tracking Sensor

The five-channel tracking module carries a PCF8574 that packs the five sensor results into one byte, so the controller needs only two signal lines to read them all, over I2C:

| Tracking module | Controller GPIO |
| --- | :---: |
| SDA | IO8 |
| SCL | IO9 |

Plug one end of the four-wire cable into the module and the other into the expansion board's right-side `I2C` socket. The silkscreen beside it reads `SDA:IO8` and `SCL:IO9`; match V and G on the plug to V and G on the socket. The plug is keyed — if it won't go in, you have it backwards, so don't force it.

These two pins are shared with the dot matrix; only one of the two can use them at a time (see 1.5).

### 2.3.5 Ultrasonic Module

Plug the ultrasonic module into the 4P socket at the top of the expansion board, silkscreened `G` `IO12` `IO13` `V`. The supplied 4P double-ended cable goes from the module to this socket; the plug is keyed, so if it won't fit, reverse it.

| Ultrasonic pin | Controller GPIO |
| --- | :---: |
| Trig | IO13 |
| Echo | IO12 |

### 2.3.6 Pan-Tilt Servo

The pan-tilt servo signal wire goes to controller IO42, powered from the expansion board's 5 V. Servos are controlled by 50 Hz PWM, with 0–180° corresponding to roughly 500–2400 µs pulse width. If both servos stall at once, the 5 V rail can sag; avoid starting both servos simultaneously in your program.

### 2.3.7 Gripper Servo

The gripper servo signal wire goes to controller IO11, also powered from the expansion board's 5 V. Motion angles: 90° closed, 140° open, pulse width 500–2500 µs.

### 2.3.8 AI Vision Module (UART)

The vision module talks to the controller over hardware serial 2, cross-connected (controller RX to module TX, controller TX to module RX), with a common ground:

| Vision module | Controller GPIO |
| --- | :---: |
| TX | RX18 |
| RX | TX17 |
| GND | GND (common) |

Serial parameters are 9600 8N1. Hardware serial 2 is used to keep the USB programming port separate, so downloading code doesn't compete with the vision module for the serial port.

## 2.4 Installing the Batteries

Use six AA alkaline or Ni-MH rechargeable batteries, inserted according to the polarity marked in the holder. Don't mix old and new cells or different brands, to avoid leakage. Remove the batteries when not in use.

- Six alkaline cells in series give about 9 V, Ni-MH about 7.2 V — both within the expansion board's input range.
- Don't use carbon-zinc cells: their high internal resistance causes a marked voltage drop when the motors and servos stall together, which can reset the controller from undervoltage.
- Take the batteries out for long-term storage so they can't leak and corrode the holder's contacts.
