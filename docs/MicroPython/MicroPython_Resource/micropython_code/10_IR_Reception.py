# 项目 10：红外接收
# 接线：红外接收头 = IO48
# 现象：按下遥控器按键，串口打印协议解出的按键码和中文名

from machine import Pin
from ir_rx.nec import NEC_8
import time

IR_RX_PIN = 48

# 遥控器按键码值；批次不同可能不同，以串口打印为准
KEY_NAMES = {
    70: '上',
    21: '下',
    68: '左',
    67: '右',
    64: 'OK / 停止',
}

holding = False


def on_ir(data, addr, ctrl):
    # data 为 -1 表示长按重复码，其余负数表示解码错误
    global holding
    if data == -1:
        if not holding:
            print('（长按中，后面不再逐帧打印）')
            holding = True
        return

    holding = False
    if data < 0:
        return

    name = KEY_NAMES.get(data, '')
    print('按键码：{}（0x{:02X}） {}'.format(data, data, name))


ir = NEC_8(Pin(IR_RX_PIN, Pin.IN), on_ir)
print('红外接收就绪，把遥控器对准车头')

while True:
    time.sleep_ms(100)
