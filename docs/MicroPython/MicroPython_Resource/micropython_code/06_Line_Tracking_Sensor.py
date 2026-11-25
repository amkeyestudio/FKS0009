from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Line_init(9, 8)

time.sleep(1)

while True:
    A, B, C, D, E = car.Line_get()
    print('A:{}  B:{}  C:{}  D:{}  E:{}'.format(A, B, C, D, E))
    time.sleep_ms(200)
