from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Ultrasonic_init(13, 12)

while True:
    distance = car.Ultrasonic_distance()
    print('Distance: ', distance, 'cm')
    time.sleep(1)           # One measurement per second
