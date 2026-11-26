from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

STEP_MS = 15               # Delay for each degree of rotation, smaller is faster

car = MiniTrackedTankRobot()

# This lesson only moves the pan-tilt servo, but the motors and the claw still need fixed levels:
# With floating driver inputs the B-channel wheels creep, and a floating claw signal picks up interference from the pan-tilt PWM
car.Motor_init()
car.stop_motor()
car.Claw_init(11)
car.Claw_set_angle(90)

car.Servo_init(42)
car.Servo_set_angle(0)
time.sleep(1)

while True:
    for angle in range(0, 181):        # 0 deg -> 180 deg
        car.Servo_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)

    for angle in range(180, -1, -1):   # 180 deg -> 0 deg
        car.Servo_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)
