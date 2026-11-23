# 项目 1：8×16 点阵
# 接线：CLK=IO9，DIN=IO8
# 现象：先亮爱心 3 秒，再亮笑脸 2 秒，然后笑脸滚动一轮

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Matrix_init(9, 8)

# 16 字节对应 16 列，每字节 8 位对应一列 8 行
heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])

smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])

car.Matrix_clear()          # 上电清屏

while True:
    car.Matrix_display(heart)
    time.sleep(3)           # 爱心 3 秒

    car.Matrix_display(smile)
    time.sleep(2)           # 笑脸 2 秒

    # 滚动：整幅图循环左移 16 次，正好转回原样
    for i in range(16):
        car.Matrix_display(smile[i:] + smile[:i])
        time.sleep_ms(150)
