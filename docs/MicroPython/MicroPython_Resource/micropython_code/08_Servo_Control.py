# 项目 8：舵机控制
# 接线：云台舵机信号线 = IO42
# 现象：舵机在 0°~180° 之间来回扫动

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

STEP_MS = 15               # 每转 1 度的延时，越小越快

car = MiniTrackedTankRobot()

# 本课只动云台，但电机和夹爪也要给固定电平：
# 驱动板输入悬空时 B 路轮子会自己转，夹爪信号悬空会被云台的 PWM 干扰
car.Motor_init()
car.stop_motor()
car.Claw_init(11)
car.Claw_set_angle(90)

car.Servo_init(42)
car.Servo_set_angle(0)
time.sleep(1)

while True:
    for angle in range(0, 181):        # 0° → 180°
        car.Servo_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)

    for angle in range(180, -1, -1):   # 180° → 0°
        car.Servo_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)
