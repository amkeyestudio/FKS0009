# 项目 4：超声波避障
# 接线：TRIG=IO13、ECHO=IO12、云台舵机=IO42、电机 40/41/38/21、点阵 9/8
# 现象：前方 15 cm 内有障碍就停车，舵机左右各测一次，朝空间大的一侧转

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Ultrasonic_init(13, 12)
car.Servo_init(42)
car.Matrix_init(9, 8)

CLOSE_DISTANCE = 15        # 小于 15 cm 视为障碍

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
    print('前方距离：', distance, 'cm')

    if distance <= CLOSE_DISTANCE:
        # 有障碍：先停车，亮感叹号
        car.stop_motor()
        car.Matrix_display(alert)
        time.sleep_ms(150)

        # 云台转向一侧，测这一侧距离
        car.Matrix_display(look_left)
        car.Servo_set_angle(180)
        time.sleep_ms(300)
        right_distance = car.Ultrasonic_distance()
        time.sleep_ms(100)

        # 云台转向另一侧，测另一侧距离
        car.Matrix_display(look_right)
        car.Servo_set_angle(0)
        time.sleep_ms(300)
        left_distance = car.Ultrasonic_distance()
        time.sleep_ms(100)

        car.Servo_set_angle(90)

        # 向空间较大的一侧转
        if right_distance > left_distance:
            car.right(255, 255)
        else:
            car.left(255, 255)
        time.sleep_ms(500)
    else:
        # 无障碍，继续直行
        car.Matrix_display(heart)
        car.forward(150, 150)

    time.sleep_ms(100)
