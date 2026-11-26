from machine import Pin
from ir_rx.nec import NEC_8
import time

IR_RX_PIN = 48

# Remote control key codes; they may differ between batches, trust the Shell output
KEY_NAMES = {
    70: 'Up',
    21: 'Down',
    68: 'Left',
    67: 'Right',
    64: 'OK / Stop',
}

holding = False


def on_ir(data, addr, ctrl):
    # data == -1 means a long-press repeat code, any other negative value is a decode error
    global holding
    if data == -1:
        if not holding:
            print('(key held, further frames are not printed)')
            holding = True
        return

    holding = False
    if data < 0:
        return

    name = KEY_NAMES.get(data, '')
    print('Key code: {} (0x{:02X}) {}'.format(data, data, name))


ir = NEC_8(Pin(IR_RX_PIN, Pin.IN), on_ir)
print('IR receiver ready, point the remote at the front of the car')

while True:
    time.sleep_ms(100)
