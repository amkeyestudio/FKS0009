# 2. Product Setup

- Servo centering code:
   - [Arduino centering code: Servo_return_to_center_90.ino](./Servo_return_to_center_90.7z)
   - [MicroPython centering code: Servo_return_to_center_90.py](./Servo_return_to_center_90.7z)

## 2.1 Before You Start

Before assembling, read the following carefully — it will save you rework and damaged parts:

1. **Center the servos first**: Power the pan-tilt and gripper servos and return them to the middle position (90°) before fitting the horns and linkages. If you install them while powered but off-center, the horn angle may not match the mechanical center, and the mechanism will either fall short of its travel or bind.
2. **Peel off all protective film**: Remove the film from both sides of the acrylic plates, the surface of the dot matrix display, and the motors and sensors. Left in place, it spoils the look and makes adhesive pads stick poorly.
3. **Watch for static**: The ESP32-S3 controller and the AI vision module are static-sensitive. Touch a grounded metal object first to discharge yourself, and handle boards by the edges — avoid the chips and header pins.
4. **Check the parts**: Compare what you have against the kit list in 1.4. Sort screws and standoffs by size so you don't mix them up and strip the threads.
5. **Leave the batteries out**: Do not install batteries during assembly and wiring. Fit them only after everything has been checked. No batteries are supplied with the kit — you need to buy them yourself.

**How to center a servo**: Turning the horn by hand won't get it exactly to 90°. Use the centering code below to let the servo hold itself at the midpoint; it keeps applying torque while you press the horn on, so the result is truly straight.

> [Image to be added] A flat-lay top view of all parts sorted into groups, as a starting reference for assembly.

> [Image to be added] A close-up of a servo connected to its debug cable with the horn at the center position.

## 2.2 Assembly Steps

> The tracked chassis comes with the tracks, motors, wheels and line-tracking sensors already fitted, so this chapter starts with the dot matrix display. Before your first line run, check the gap between the tracking sensors and the ground: too high and they can't see the black line, too low and they scrape along the surface.

> **Fit the AI vision module last**: Lessons 1–16 don't use it, so you can start the course once you've done step 8 (wiring and tidying) and step 10 (power-on self-check). If you want to jump ahead to the last four lessons, fit it at step 9.

### 2.2.1 Step 1: Dot Matrix Display and Acrylic Plate

Fit the 8×16 dot matrix display into the window of the acrylic plate, facing out of the car with the character patterns facing up, and secure it from the back with the supplied screws. Then route the display's CLK and DIN wires out from behind the plate; they will go to controller IO9 and IO8 later.

> [Image to be added] Front view of the dot matrix fitted in the acrylic window, with its orientation clearly visible.

> [Image to be added] Close-up of the screws holding the display at the back, showing the screw positions and wire exit direction.

### 2.2.2 Step 2: Standoffs on the Acrylic Plate

Screw the standoffs into the holes on the acrylic plate. Their height sets how far the controller sits above the plate. Tighten by hand, then give each a light turn with the wrench — the acrylic can't take much force, and over-tightening will chip or crack it.

> [Image to be added] Close-up of standoffs screwed into the acrylic holes, showing their size and positions.

### 2.2.3 Step 3: Front Gripper

Fit the gripper servo at the front of the car, then attach the linkage and jaws. Before mounting, use the centering code from 2.1 to hold the servo at 90°, then set the gripper half-open while you press the horn on; otherwise the finished jaws won't open fully or close completely. Gripper travel: 90° closed, 140° open, pulse width 500–2500 µs.

> [Image to be added] Side view of the gripper servo fitted at the front of the car.

> [Image to be added] Comparison of the gripper open (140°) and closed (90°) states.

### 2.2.4 Step 4: Standoffs Between the Base and the Acrylic Plate

Screw the standoffs into the mounting holes of the chassis, press the acrylic plate down onto them, and lock it from the other side with screws. Tighten the four corners in rotation — don't fully tighten one corner first, or the plate will warp and the holes will drift out of alignment.

> [Image to be added] Side view of the standoffs between the chassis and the acrylic plate, showing their number and positions.

> [Image to be added] Top view illustrating the cross-wise tightening of the four corners.

### 2.2.5 Step 5: Controller and Expansion Board

First screw the ESP32-S3 controller onto the acrylic standoffs, with the USB ports facing a convenient direction for plugging in cables. Then align the DRV8835 expansion board with the controller's header pins and press it down evenly — don't force one side, or you'll bend pins that are hard to straighten.

> [Image to be added] Top view of the controller screwed onto the acrylic standoffs.

> [Image to be added] Angled side view of the expansion board aligned with the header pins, ready to be stacked.

> [Image to be added] Top view of the assembled robot after stacking, showing the orientation of the USB port and power jack.

### 2.2.6 Step 6: Ultrasonic Pan-Tilt and Servo

Before mounting, use the centering code from 2.1 to hold the pan-tilt servo at its 90° midpoint. The horn spline is stepped; if you press it on off-center, then later sweep 0–180° in code, the ultrasonic sensor won't face straight ahead and the left/right swing will be uneven. Once centered, fix the servo at the front of the car, fit the horn and pan-tilt bracket, then mount the ultrasonic module on the pan-tilt with the transducers pointing straight forward and level.

If you are fitting the AI vision module at this step too (method A or B, see 2.2.9), don't tighten the pan-tilt bracket yet — wait until the module is fixed, then lock everything together.

> [Image to be added] Screenshot of the servo returning to 90° with its debug cable connected, or a close-up of the centered horn.

> [Image to be added] Side view of the pan-tilt servo and bracket assembly.

> [Image to be added] Front view of the ultrasonic module fitted on the pan-tilt, transducers facing forward.

### 2.2.7 Step 7: Battery Holder

Fix the 6-cell AA holder at the rear of the chassis, with the wires pointing toward the expansion board's power input. Six AA cells in series give about 9 V alkaline or 7.2 V Ni-MH, both within the expansion board's input range. Keeping the batteries at the rear also lowers the center of gravity and adds grip to the rear wheels.

> [Image to be added] Top view of the battery holder fixed at the rear of the chassis, with the wire direction visible.

> [Image to be added] Diagram of battery orientation (positive and negative ends), showing how all six cells should sit.

### 2.2.8 Step 8: Wiring and Tidying

Connect all modules following the wiring notes in 2.3, then tidy the cables: run motor wires along the inside of the chassis, fix signal wires along the body, and leave slack in the moving harnesses for the pan-tilt and gripper so they aren't caught by the tracks or wheels. Cable ties can be used lightly, but don't pull them too tight.

> [Image to be added] Top view of the finished wiring, showing the harness routes and how they are tied.

> [Image to be added] Close-up of the slack left in the pan-tilt harness.

### 2.2.9 Step 9: AI Vision Module (Optional)

The AI vision module is optional. Lessons 1–16 don't use it, so you can complete the whole course without it; conversely, if you want to start with the last four lessons, fit it at this step. Both mounting methods put the module on the pan-tilt, so the lens turns with the servo — the only difference is where the lens points:

- **Method A (lens toward the ground)**: The module sits on the pan-tilt with the lens pointing down at the stretch of line in front of the car. Lesson 18 (camera line tracking) uses this — the lens must see the line for the car to know how far off it is.
- **Method B (lens straight ahead)**: The module sits on the pan-tilt with the lens level and facing forward, looking at things placed in front of the car. Lessons 17 (color tracking and grabbing), 19 (face following) and 20 (QR code tasks) use this — objects, faces and cards are all straight ahead.

Both methods use the pan-tilt, so when you fixed the bracket in 2.2.6 you left it loose — tighten it only after the module is in place. Leave slack in the harness so the wiring isn't pulled when the pan-tilt reaches its limit. Once mounted, connect the module's UART wires to the `IO18RX` (module TX) and `IO17TX` (module RX) pins on the expansion board's right-side `UART` header; see 2.3.8 for wiring.

> [Image to be added] Side view of method A: module on the pan-tilt with the lens pointing at the ground, showing how far ahead it looks.

> [Image to be added] Side view of method B: module on the pan-tilt with the lens facing forward, showing wire routing and the slack for pan-tilt rotation.

> [Image to be added] Side-by-side comparison of the two mounting methods on one car, labeling lens direction and the lessons each suits.

### 2.2.10 Step 10: Power-On and Self-Check

Before fitting the batteries, do a final visual check: correct power polarity, all plugs seated, no short circuits or frayed wires. Then power on and check that the dot matrix lights up, the servos return to center, and the motors stay stopped. Only then move on to 2.3 to verify each connection.

> [Image to be added] Photo of the robot in a healthy powered-on state, with the dot matrix lit and readable.

## 2.3 Wiring Notes

> Pin assignments follow the silkscreen on the actual boards. Always disconnect power before wiring, and check everything before powering on.

### 2.3.1 Motors

Connect the left and right motors to channels A and B of the expansion board respectively. Each channel has a direction pin and a speed (PWM) pin. Left and right are defined as seen standing behind the car looking forward: left hand is the left wheel, right hand the right wheel; channels A/B follow the expansion board silkscreen (AIN/AEN, BIN/BEN). The mapping:

| Motor | Channel (silkscreen) | Direction pin | Speed pin (PWM) |
| --- | :---: | :---: | :---: |
| Left motor (physical left wheel) | A (AIN/AEN) | IO38 | IO21 |
| Right motor (physical right wheel) | B (BIN/BEN) | IO40 | IO41 |

> If forward becomes backward or the steering is reversed, check channels A/B and the wheel wiring against the table before changing any names in code. You can flash `00_Motor_Mapping_Test` to drive one channel at a time and see which wheel turns, confirming the mapping.

> [Image to be added] Close-up of the A and B motor terminals, with wire colors and terminal mapping visible.

### 2.3.2 Power

Connect the battery holder's red wire (positive) to the expansion board's positive power input and the black wire (negative) to the negative input — never reverse them. The whole robot is powered through the expansion board: motors and servos draw from it directly, while the controller is fed through its regulator.

> [Image to be added] Close-up of the battery wires at the expansion board's power terminals, with polarities clearly visible.

### 2.3.3 8×16 Dot Matrix

The display needs only two signal lines and uses a two-wire (I2C-like) driver:

| Display pin | Controller GPIO |
| --- | :---: |
| CLK | IO9 |
| DIN | IO8 |

> [Image to be added] Wiring of the display's CLK and DIN lines to controller IO9 and IO8.

### 2.3.4 Line-Tracking Sensor

The five-channel tracking module carries a PCF8574 that packs the five sensor results into one byte, so the controller needs only two signal lines to read them all, over I2C:

| Tracking module | Controller GPIO |
| --- | :---: |
| SDA | IO8 |
| SCL | IO9 |

Plug one end of the four-wire cable into the module and the other into the expansion board's right-side `I2C` socket. The silkscreen beside it reads `SDA:IO8` and `SCL:IO9`; match V and G on the plug to V and G on the socket. The plug is keyed — if it won't go in, you have it backwards, so don't force it.

These two pins are shared with the dot matrix; only one of the two can use them at a time (see 1.5).

> [Image to be added] Wiring of the tracking sensor's four-wire cable to the expansion board's I2C socket, with plug direction and silkscreen visible.

### 2.3.5 Ultrasonic Module

Plug the ultrasonic module into the 4P socket at the top of the expansion board, silkscreened `G` `IO12` `IO13` `V`. The supplied 4P double-ended cable goes from the module to this socket; the plug is keyed, so if it won't fit, reverse it.

| Ultrasonic pin | Controller GPIO |
| --- | :---: |
| Trig | IO13 |
| Echo | IO12 |

> [Image to be added] Wiring of the ultrasonic module's four-wire cable to the top 4P socket, with silkscreen and plug direction visible.

### 2.3.6 Pan-Tilt Servo

The pan-tilt servo signal wire goes to controller IO42, powered from the expansion board's 5 V. Servos are controlled by 50 Hz PWM, with 0–180° corresponding to roughly 500–2400 µs pulse width. If both servos stall at once, the 5 V rail can sag; avoid starting both servos simultaneously in your program.

> [Image to be added] Wiring of the pan-tilt servo signal line to IO42, showing the routing and power connection.

### 2.3.7 Gripper Servo

The gripper servo signal wire goes to controller IO11, also powered from the expansion board's 5 V. Motion angles: 90° closed, 140° open, pulse width 500–2500 µs.

> [Image to be added] Wiring of the gripper servo signal line to IO11.

### 2.3.8 AI Vision Module (UART)

The vision module talks to the controller over hardware serial 2, cross-connected (controller RX to module TX, controller TX to module RX), with a common ground:

| Vision module | Controller GPIO |
| --- | :---: |
| TX | RX18 |
| RX | TX17 |
| GND | GND (common) |

Serial parameters are 9600 8N1. Hardware serial 2 is used to keep the USB programming port separate, so downloading code doesn't compete with the vision module for the serial port.

> [Image to be added] UART wiring between the vision module and the controller, with the TX/RX crossover and ground wire visible.

## 2.4 Installing the Batteries

Use six AA alkaline or Ni-MH rechargeable batteries, inserted according to the polarity marked in the holder. Don't mix old and new cells or different brands, to avoid leakage. Remove the batteries when not in use.

- Six alkaline cells in series give about 9 V, Ni-MH about 7.2 V — both within the expansion board's input range.
- Don't use carbon-zinc cells: their high internal resistance causes a marked voltage drop when the motors and servos stall together, which can reset the controller from undervoltage.
- Take the batteries out for long-term storage so they can't leak and corrode the holder's contacts.

> [Image to be added] Top view of six batteries inserted in the correct orientation.

> [Image to be added] Photo of the completed robot with the battery cover fitted.
