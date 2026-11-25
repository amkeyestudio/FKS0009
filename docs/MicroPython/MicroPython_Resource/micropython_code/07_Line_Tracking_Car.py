from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Line_init(9, 8)

BASE_SPEED = 115           # 直行基础速度
RECOVER_IN = 95            # 急转时内侧轮反转速度
RECOVER_OUT = 125          # 急转时外侧轮正转速度

last_dir = 0               # 0 直行，1 线在左，2 线在右
lost_start = 0             # 丢线起始时刻，0 表示未丢线

car.stop_motor()
print('循迹开始')
time.sleep(2)              # 对齐 Arduino：上电 2 秒后开始循迹

while True:
    A, B, C, D, E = car.Line_get()
    state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E

    if state != 0:
        lost_start = 0     # 压到线，清丢线计时

    if state in (0b00100, 0b01110, 0b11111):
        car.drive(BASE_SPEED, BASE_SPEED)          # 居中直行
        last_dir = 0

    elif state == 0b01100:
        car.drive(85, BASE_SPEED)                  # 偏左：左轮降速
        last_dir = 1

    elif state in (0b01000, 0b11100):
        car.drive(45, RECOVER_OUT)                 # 偏左较大
        last_dir = 1

    elif state in (0b11000, 0b10000):
        car.drive(-RECOVER_IN, RECOVER_OUT)        # 严重偏左：原地左转
        last_dir = 1

    elif state == 0b00110:
        car.drive(BASE_SPEED, 85)                  # 偏右：右轮降速
        last_dir = 2

    elif state in (0b00010, 0b00111):
        car.drive(RECOVER_OUT, 45)                 # 偏右较大
        last_dir = 2

    elif state in (0b00011, 0b00001):
        car.drive(RECOVER_OUT, -RECOVER_IN)        # 严重偏右：原地右转
        last_dir = 2

    elif state == 0b00000:
        # 五路全部脱线：按记忆方向找线
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < 1500:
            if last_dir == 1:
                car.drive(-RECOVER_IN, RECOVER_OUT)    # 线原在左
            elif last_dir == 2:
                car.drive(RECOVER_OUT, -RECOVER_IN)    # 线原在右
            else:
                car.stop_motor()                       # 原为直行，停车
        else:
            car.stop_motor()                           # 超时未找到，停车

    # 其余组合（如 0b01111、0b11110）是探头过渡态，保持上一条电机指令不动

    time.sleep_ms(5)
