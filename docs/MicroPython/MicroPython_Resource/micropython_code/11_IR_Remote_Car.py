from machine import Pin
from ir_rx.nec import NEC_8
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

IR_RX_PIN = 48
SPEED_RUN = 180
SPEED_TURN = 180
STOP_AFTER = 1500          # 超时自动停车（ms）

# 遥控器键值；换遥控器后按串口打印值修改
KEY_UP = 70
KEY_DOWN = 21
KEY_LEFT = 68
KEY_RIGHT = 67
KEY_STOP = 64

car = MiniTrackedTankRobot()
car.Motor_init()
car.stop_motor()

key = None                 # 待处理的按键，主循环取走
last_cmd = time.ticks_ms()


def on_ir(data, addr, ctrl):
    # 中断里只记键值和时刻，动作留给主循环做
    global key, last_cmd
    if data == -1:                 # 长按重复码：只刷新超时计时
        last_cmd = time.ticks_ms()
    elif data in (KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_STOP):
        key = data                 # 已知键：记键值并刷新计时
        last_cmd = time.ticks_ms()
    # 未知键：不处理也不刷新计时，与 Arduino 的 isKnownKey 一致


ir = NEC_8(Pin(IR_RX_PIN, Pin.IN), on_ir)
print('红外遥控车就绪')

while True:
    if key is not None:
        cmd = key
        key = None

        if cmd == KEY_UP:
            print('前进')
            car.forward(SPEED_RUN, SPEED_RUN)
        elif cmd == KEY_DOWN:
            print('后退')
            car.back(SPEED_RUN, SPEED_RUN)
        elif cmd == KEY_LEFT:
            print('左转')
            car.left(SPEED_TURN, SPEED_TURN)
        elif cmd == KEY_RIGHT:
            print('右转')
            car.right(SPEED_TURN, SPEED_TURN)
        elif cmd == KEY_STOP:
            print('停车')
            car.stop_motor()

    # 超时没有有效按键就停车，避免遥控器失联后一直跑
    if time.ticks_diff(time.ticks_ms(), last_cmd) > STOP_AFTER:
        car.stop_motor()

    time.sleep_ms(10)
