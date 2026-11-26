from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

STEP_MS = 15               # Delay for each degree of rotation, smaller is faster
CLAW_OPEN = 140
CLAW_CLOSE = 90

car = MiniTrackedTankRobot()

# This lesson only moves the claw, but the motors and the pan-tilt still need fixed levels:
# With floating driver inputs the B-channel wheels creep, and a floating pan-tilt signal picks up interference from the claw PWM
car.Motor_init()
car.stop_motor()
car.Servo_init(42)
car.Servo_set_angle(90)

car.Claw_init(11)
car.Claw_set_angle(CLAW_CLOSE)
time.sleep(1)

while True:
    for angle in range(CLAW_CLOSE, CLAW_OPEN + 1):     # 90 deg -> 140 deg: open
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)

    for angle in range(CLAW_OPEN, CLAW_CLOSE - 1, -1): # 140 deg -> 90 deg: close
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)
