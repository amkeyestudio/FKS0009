# 项目 3：超声波测距
# 接线：TRIG=IO13，ECHO=IO12
# 现象：串口每 1 秒打印一次前方距离（厘米）

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Ultrasonic_init(13, 12)

while True:
    distance = car.Ultrasonic_distance()
    print('距离：', distance, 'cm')
    time.sleep(1)           # 1 秒测一次
