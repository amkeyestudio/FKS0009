# 项目 5：超声波跟随
# 接线：TRIG=IO13、ECHO=IO12、电机 40/41/38/21、点阵 9/8
# 现象：30~50 cm 前进；10~30 cm 停住；小于 10 cm 后退；超范围显示问号

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Ultrasonic_init(13, 12)
car.Matrix_init(9, 8)

SAFE_DISTANCE = 30         # 30~50 cm：跟随
CLOSE_DISTANCE = 10        # 小于 10 cm：后退
MAX_DISTANCE = 50          # 大于 50 cm：目标丢失

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
    print('当前距离：', distance, 'cm')

    if SAFE_DISTANCE < distance <= MAX_DISTANCE:
        # 30~50 cm：跟随前进
        car.Matrix_display(heart)
        car.forward(150, 150)

    elif CLOSE_DISTANCE <= distance <= SAFE_DISTANCE:
        # 10~30 cm：距离合适，停住
        car.stop_motor()
        car.Matrix_display(smile)

    elif 0 < distance < CLOSE_DISTANCE:
        # 太近了，差速后退
        car.Matrix_display(arrow_down)
        car.back(150, 105)

    else:
        # 大于 50 cm 或读数无效：目标丢失
        car.stop_motor()
        car.Matrix_display(question)

    time.sleep_ms(100)
