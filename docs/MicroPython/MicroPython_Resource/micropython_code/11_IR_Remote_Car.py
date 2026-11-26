from machine import Pin
from ir_rx.nec import NEC_8
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

IR_RX_PIN = 48
SPEED_RUN = 180
SPEED_TURN = 180
STOP_AFTER = 1500          # Auto stop after this timeout (ms)

# Remote key codes; change them according to the Shell output if you use another remote
KEY_UP = 70
KEY_DOWN = 21
KEY_LEFT = 68
KEY_RIGHT = 67
KEY_STOP = 64

car = MiniTrackedTankRobot()
car.Motor_init()
car.stop_motor()

key = None                 # Pending key, consumed by the main loop
last_cmd = time.ticks_ms()


def on_ir(data, addr, ctrl):
    # The interrupt only records the key and the time, the main loop performs the action
    global key, last_cmd
    if data == -1:                 # Long-press repeat code: only refresh the timeout
        last_cmd = time.ticks_ms()
    elif data in (KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_STOP):
        key = data                 # Known key: store it and refresh the timeout
        last_cmd = time.ticks_ms()
    # Unknown key: ignored and the timeout is not refreshed, same as isKnownKey in the Arduino version


ir = NEC_8(Pin(IR_RX_PIN, Pin.IN), on_ir)
print('IR remote car ready')

while True:
    if key is not None:
        cmd = key
        key = None

        if cmd == KEY_UP:
            print('Forward')
            car.forward(SPEED_RUN, SPEED_RUN)
        elif cmd == KEY_DOWN:
            print('Backward')
            car.back(SPEED_RUN, SPEED_RUN)
        elif cmd == KEY_LEFT:
            print('Turn left')
            car.left(SPEED_TURN, SPEED_TURN)
        elif cmd == KEY_RIGHT:
            print('Turn right')
            car.right(SPEED_TURN, SPEED_TURN)
        elif cmd == KEY_STOP:
            print('Stop')
            car.stop_motor()

    # Stop when no valid key arrives in time, so the car does not keep running when the remote is out of range
    if time.ticks_diff(time.ticks_ms(), last_cmd) > STOP_AFTER:
        car.stop_motor()

    time.sleep_ms(10)
