# 项目 2：电机驱动与调速
# 接线：AIN=IO40、AEN=IO41、BIN=IO38、BEN=IO21；点阵 CLK=IO9、DIN=IO8
# 现象：前进 2 秒 → 后退 2 秒 → 原地右转 1.5 秒 → 原地左转 1.5 秒 → 加减速演示

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
# 左转时点阵显示的箭头字模
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
print('电机驱动实验开始')
time.sleep(1)

while True:
    print('前进（显示爱心）')
    car.Matrix_display(heart)
    car.forward(SPEED_RUN, SPEED_RUN)
    time.sleep(2)
    car.stop_motor()
    time.sleep_ms(500)

    print('后退（显示向下箭头）')
    car.Matrix_display(arrow_down)
    car.back(SPEED_RUN, SPEED_RUN)
    time.sleep(2)
    car.stop_motor()
    time.sleep_ms(500)

    print('原地右转')
    car.Matrix_display(arrow_right)
    car.right(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(1500)
    car.stop_motor()
    time.sleep_ms(500)

    print('原地左转')
    car.Matrix_display(arrow_left)
    car.left(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(1500)
    car.stop_motor()
    time.sleep_ms(1000)

    # 调速：占空比 0 → 255 → 0，点阵同步滚动
    print('加速：0 -> 255')
    for duty in range(0, 256, 5):
        car.forward(duty, duty)          # 用 forward 才能同时给定方向
        if duty % 20 == 0:
            step = (duty // 20) % 16
            car.Matrix_display(heart[step:] + heart[:step])
        time.sleep_ms(20)

    print('减速：255 -> 0')
    for duty in range(255, -1, -5):
        car.forward(duty, duty)
        if duty % 20 == 0:
            step = (duty // 20) % 16
            car.Matrix_display(heart[step:] + heart[:step])
        time.sleep_ms(20)

    car.stop_motor()
    print('停止，2 秒后进入下一轮')
    time.sleep(2)
