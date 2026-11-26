from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Matrix_init(9, 8)

# 16 bytes for 16 columns; the 8 bits of each byte are the 8 rows of one column
heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])

smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])

car.Matrix_clear()          # Clear the display at power-on

while True:
    car.Matrix_display(heart)
    time.sleep(3)           # Heart for 3 seconds

    car.Matrix_display(smile)
    time.sleep(2)           # Smiley for 2 seconds

    # Scroll: shift the whole image left 16 times to return to the original
    for i in range(16):
        car.Matrix_display(smile[i:] + smile[:i])
        time.sleep_ms(150)
