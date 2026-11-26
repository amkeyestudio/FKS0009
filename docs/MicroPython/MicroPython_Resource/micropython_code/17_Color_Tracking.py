import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

SPEED_RUN = 180
SPEED_TURN = 180
SPEED_APPROACH = 150

CLAW_OPEN = 140
CLAW_CLOSE = 90
PAN_CENTER = 90

T_APPROACH = 350           # Approach time (ms)
T_TURN = 600               # Time to turn 90 deg in place (ms)
T_DELIVER = 800            # Delivery time (ms)

FACE_EYES = 0
FACE_SMILE = 1
FACE_HEART = 2

eyes = bytes([
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])
heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])

car = MiniTrackedTankRobot()
car.Motor_init()
car.Servo_init(42)
car.Claw_init(11)
car.Matrix_init(9, 8)

car.Servo_set_angle(PAN_CENTER)
car.Claw_set_angle(CLAW_OPEN)
car.stop_motor()

cam = AiCam()
cam.begin(tx=17, rx=18)
cam.setMode(cam.COLOR)

cur_face = -1


def show_face(face):
    global cur_face
    if face == cur_face:
        return
    cur_face = face
    if face == FACE_EYES:
        car.Matrix_display(eyes)
    elif face == FACE_SMILE:
        car.Matrix_display(smile)
    else:
        car.Matrix_display(heart)


def pick_and_deliver(color):
    # Outbound: approach -> grab -> turn -> deliver -> release; return to the start in reverse order
    print('1 Approaching slowly')
    car.forward(SPEED_APPROACH, SPEED_APPROACH)
    time.sleep_ms(T_APPROACH)
    car.stop_motor()

    print('2 Grabbing')
    car.Claw_set_angle(CLAW_CLOSE)
    time.sleep_ms(400)
    show_face(FACE_HEART)

    print('3 Turning by color: red left, yellow right')
    if color == 'RED':
        car.left(SPEED_TURN, SPEED_TURN)
    else:
        car.right(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(T_TURN)
    car.stop_motor()

    print('4 Delivering')
    car.forward(SPEED_RUN, SPEED_RUN)
    time.sleep_ms(T_DELIVER)
    car.stop_motor()

    print('5 Opening the claw to unload')
    car.Claw_set_angle(CLAW_OPEN)
    time.sleep_ms(300)

    print('6 Reversing to the turning point')
    car.back(SPEED_RUN, SPEED_RUN)
    time.sleep_ms(T_DELIVER)
    car.stop_motor()

    print('7 Turning back straight')
    if color == 'RED':
        car.right(SPEED_TURN, SPEED_TURN)
    else:
        car.left(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(T_TURN)
    car.stop_motor()

    print('8 Reversing to the start')
    car.back(SPEED_APPROACH, SPEED_APPROACH)
    time.sleep_ms(T_APPROACH)
    car.stop_motor()

    show_face(FACE_SMILE)
    print('Delivery finished, back at the start')


show_face(FACE_EYES)
print('Color tracking car ready, place a red or yellow block in front of the car')
time.sleep(1)

while True:
    cam.read()
    color = cam.color()

    # No target, or not red or yellow: stop and wait
    if color not in ('RED', 'YELLOW'):
        car.stop_motor()
        show_face(FACE_EYES)
        time.sleep_ms(20)
        continue

    print('Target detected: ', color)
    show_face(FACE_SMILE)
    cam.clearColor()

    pick_and_deliver(color)

    cam.flush()                 # Discard the data buffered during the manoeuvre
    cam.clearColor()
    show_face(FACE_EYES)
    time.sleep(2)               # Cool down for 2 seconds before looking for the next target
