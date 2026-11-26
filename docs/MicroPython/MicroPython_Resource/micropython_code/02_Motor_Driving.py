from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Matrix_init(9, 8)

SPEED_RUN = 150
SPEED_TURN = 150

heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])
arrow_down = bytes([
    0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7E, 0xFE,
    0xFE, 0x7E, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
])
# Arrow glyph shown on the dot matrix when turning left
arrow_left = bytes([
    0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0xFF, 0x7E, 0x3C, 0x18, 0x00, 0x00, 0x00
])
arrow_right = bytes([
    0x00, 0x00, 0x00, 0x18, 0x3C, 0x7E, 0xFF, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])

car.stop_motor()
car.Matrix_display(smile)
print('Motor driving experiment started')
time.sleep(1)

while True:
    print('Forward (heart icon)')
    car.Matrix_display(heart)
    car.forward(SPEED_RUN, SPEED_RUN)
    time.sleep(2)
    car.stop_motor()
    time.sleep_ms(500)

    print('Backward (down arrow)')
    car.Matrix_display(arrow_down)
    car.back(SPEED_RUN, SPEED_RUN)
    time.sleep(2)
    car.stop_motor()
    time.sleep_ms(500)

    print('Turn right in place')
    car.Matrix_display(arrow_right)
    car.right(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(1500)
    car.stop_motor()
    time.sleep_ms(500)

    print('Turn left in place')
    car.Matrix_display(arrow_left)
    car.left(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(1500)
    car.stop_motor()
    time.sleep_ms(1000)

    # Speed sweep: duty 0 -> 255 -> 0 with the matrix scrolling in sync
    print('Accelerating: 0 -> 255')
    for duty in range(0, 256, 5):
        car.forward(duty, duty)          # Only forward() sets the direction together with the duty
        if duty % 20 == 0:
            step = (duty // 20) % 16
            car.Matrix_display(heart[step:] + heart[:step])
        time.sleep_ms(20)

    print('Decelerating: 255 -> 0')
    for duty in range(255, -1, -5):
        car.forward(duty, duty)
        if duty % 20 == 0:
            step = (duty // 20) % 16
            car.Matrix_display(heart[step:] + heart[:step])
        time.sleep_ms(20)

    car.stop_motor()
    print('Stopped, next round in 2 seconds')
    time.sleep(2)
