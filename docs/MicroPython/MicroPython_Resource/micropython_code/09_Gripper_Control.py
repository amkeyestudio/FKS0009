# 项目 9：夹爪控制与抓取
# 接线：夹爪舵机信号线 = IO11
# 现象：夹爪在 90°（夹紧）和 140°（张开）之间来回开合

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

STEP_MS = 15               # 每转 1 度的延时，越小越快
CLAW_OPEN = 140
CLAW_CLOSE = 90

car = MiniTrackedTankRobot()

# 本课只动夹爪，但电机和云台也要给固定电平：
# 驱动板输入悬空时 B 路轮子会自己转，云台信号悬空会被夹爪的 PWM 干扰
car.Motor_init()
car.stop_motor()
car.Servo_init(42)
car.Servo_set_angle(90)

car.Claw_init(11)
car.Claw_set_angle(CLAW_CLOSE)
time.sleep(1)

while True:
    for angle in range(CLAW_CLOSE, CLAW_OPEN + 1):     # 90° → 140° 张开
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)

    for angle in range(CLAW_OPEN, CLAW_CLOSE - 1, -1): # 140° → 90° 夹紧
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)
