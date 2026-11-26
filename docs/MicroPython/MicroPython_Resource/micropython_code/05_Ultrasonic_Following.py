from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Ultrasonic_init(13, 12)
car.Matrix_init(9, 8)

SAFE_DISTANCE = 30         # 30-50 cm: follow
CLOSE_DISTANCE = 10        # Closer than 10 cm: back off
MAX_DISTANCE = 50          # Farther than 50 cm: target lost

heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])
arrow_down = bytes([
    0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7E, 0xFE,
    0xFE, 0x7E, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
])
question = bytes([
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x51,
    0x59, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
])

car.stop_motor()
car.Matrix_display(smile)

while True:
    distance = car.Ultrasonic_distance()
    print('Current distance: ', distance, 'cm')

    if SAFE_DISTANCE < distance <= MAX_DISTANCE:
        # 30-50 cm: follow the target
        car.Matrix_display(heart)
        car.forward(150, 150)

    elif CLOSE_DISTANCE <= distance <= SAFE_DISTANCE:
        # 10-30 cm: good distance, hold still
        car.stop_motor()
        car.Matrix_display(smile)

    elif 0 < distance < CLOSE_DISTANCE:
        # Too close: reverse with differential speed
        car.Matrix_display(arrow_down)
        car.back(150, 105)

    else:
        # Farther than 50 cm or invalid reading: target lost
        car.stop_motor()
        car.Matrix_display(question)

    time.sleep_ms(100)
