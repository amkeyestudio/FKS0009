# 项目 6：循迹传感器
# 接线：PCF8574 的 SCL=IO9、SDA=IO8，地址 0x20
# 现象：串口打印五个探头的状态，A 最左、E 最右，1 表示压到黑线

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Line_init(9, 8)

time.sleep(1)

while True:
    A, B, C, D, E = car.Line_get()
    print('A:{}  B:{}  C:{}  D:{}  E:{}'.format(A, B, C, D, E))
    time.sleep_ms(200)
