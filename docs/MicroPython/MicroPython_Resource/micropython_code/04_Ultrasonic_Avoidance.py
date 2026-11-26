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
