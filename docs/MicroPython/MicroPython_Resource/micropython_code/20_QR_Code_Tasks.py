import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

PAN_CENTER = 90

SPEED_MOVE = 150           # Forward / backward speed
SPEED_TURN_OUT = 150       # Outer wheel speed for arc turns
SPEED_TURN_IN = 50         # Inner wheel speed for arc turns
ACTION_TIME = 1000         # Duration of each card action (ms)

FACE_EYES = 0
FACE_SMILE = 1

eyes = bytes([
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])

car = MiniTrackedTankRobot()
car.Motor_init()
car.Servo_init(42)
car.Matrix_init(9, 8)

car.Servo_set_angle(PAN_CENTER)
car.stop_motor()
time.sleep_ms(300)

cam = AiCam()
cam.begin(tx=17, rx=18)
cam.setMode(cam.QR)

cur_face = -1
last_qr = ''


def show_face(face):
    global cur_face
    if face == cur_face:
        return
    cur_face = face
    if face == FACE_EYES:
        car.Matrix_display(eyes)
    else:
        car.Matrix_display(smile)


def do_task(qr):
    show_face(FACE_SMILE)

    key = qr.strip().upper()

    if key == 'FORWARD':
        car.forward(SPEED_MOVE, SPEED_MOVE)                    # Drive straight for a while
    elif key == 'BACKWARD':
        car.back(SPEED_MOVE, SPEED_MOVE)                       # Reverse for a while
    elif key == 'LEFT':
        car.drive(SPEED_TURN_IN, SPEED_TURN_OUT)               # Slow left and fast right: arc to the left
    elif key == 'RIGHT':
        car.drive(SPEED_TURN_OUT, SPEED_TURN_IN)               # Fast left and slow right: arc to the right
    else:
        return                                                 # Not one of these four cards: do nothing

    time.sleep_ms(ACTION_TIME)
    car.stop_motor()


show_face(FACE_EYES)
print('QR task cards ready')
time.sleep(1)              # Wait for the module to finish switching mode

while True:
    cam.read()
    qr = cam.qr()

    # No card in view: clear last_qr to unlock
    if qr == '':
        last_qr = ''
        car.stop_motor()
        show_face(FACE_EYES)
        time.sleep_ms(20)
        continue

    # The same card is still held: skip so the action is not repeated
    if qr == last_qr:
        time.sleep_ms(20)
        continue

    # New card: record it before acting so it is not triggered again during the action
    last_qr = qr
    print('QR code scanned: ', qr, ' bytes ', len(qr))
    do_task(qr)

    time.sleep_ms(20)
