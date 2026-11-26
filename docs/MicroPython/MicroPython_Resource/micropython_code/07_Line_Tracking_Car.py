from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Line_init(9, 8)

BASE_SPEED = 115           # Base speed for driving straight
RECOVER_IN = 95            # Inner wheel reverse speed for sharp turns
RECOVER_OUT = 125          # Outer wheel forward speed for sharp turns

last_dir = 0               # 0 = straight, 1 = line on the left, 2 = line on the right
lost_start = 0             # Time the line was first lost, 0 means the line is not lost

car.stop_motor()
print('Line tracking started')
time.sleep(2)              # Same as the Arduino version: start tracking 2 seconds after power-on

while True:
    A, B, C, D, E = car.Line_get()
    state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E

    if state != 0:
        lost_start = 0     # On the line: clear the lost-line timer

    if state in (0b00100, 0b01110, 0b11111):
        car.drive(BASE_SPEED, BASE_SPEED)          # Centered: drive straight
        last_dir = 0

    elif state == 0b01100:
        car.drive(85, BASE_SPEED)                  # Drifting left: slow the left wheel
        last_dir = 1

    elif state in (0b01000, 0b11100):
        car.drive(45, RECOVER_OUT)                 # Clearly to the left
        last_dir = 1

    elif state in (0b11000, 0b10000):
        car.drive(-RECOVER_IN, RECOVER_OUT)        # Far left: spin left in place
        last_dir = 1

    elif state == 0b00110:
        car.drive(BASE_SPEED, 85)                  # Drifting right: slow the right wheel
        last_dir = 2

    elif state in (0b00010, 0b00111):
        car.drive(RECOVER_OUT, 45)                 # Clearly to the right
        last_dir = 2

    elif state in (0b00011, 0b00001):
        car.drive(RECOVER_OUT, -RECOVER_IN)        # Far right: spin right in place
        last_dir = 2

    elif state == 0b00000:
        # All five sensors off the line: search in the last known direction
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < 1500:
            if last_dir == 1:
                car.drive(-RECOVER_IN, RECOVER_OUT)    # The line was on the left
            elif last_dir == 2:
                car.drive(RECOVER_OUT, -RECOVER_IN)    # The line was on the right
            else:
                car.stop_motor()                       # Was driving straight: stop
        else:
            car.stop_motor()                           # Timed out without finding the line: stop

    # Other combinations (e.g. 0b01111, 0b11110) are sensor transitions: keep the last motor command

    time.sleep_ms(5)
