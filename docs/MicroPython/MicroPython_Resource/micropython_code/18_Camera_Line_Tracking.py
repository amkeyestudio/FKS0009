import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

BASE_SPEED = 130           # Base speed for driving straight
KP = 0.4                   # Proportional gain
KD = 0.6                   # Derivative gain, damps overshoot
TURN_MAX = 90              # Maximum correction on one side
D_MAX = 50                 # Derivative term limit

SEARCH_SPEED = 90          # Search rotation speed
SEARCH_DEAD = 15           # Search dead zone
LOST_TIMEOUT = 600         # Line search timeout (ms)

LINE_OK = 5                # A value of 5 in the module's third field means the line is detected

DEBUG = False              # Debug switch: set to True to print the line data reported by the vision module

FACE_EYES = 0
FACE_SMILE = 1
FACE_SURPRISE = 2
FACE_SAD = 3

eyes = bytes([
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])
surprise = bytes([
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x00, 0xC0,
    0xC0, 0x00, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x00
])
sad = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x80, 0x40,
    0x40, 0x80, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00
])

car = MiniTrackedTankRobot()
car.Motor_init()
car.Matrix_init(9, 8)
car.stop_motor()

cam = AiCam()
cam.begin(tx=17, rx=18)
cam.setMode(cam.LINE)

cur_face = -1
last_error = 0
has_last = False
lost_start = 0
dbg_prev = None            # Last printed line data, printed once per new sample


def show_face(face):
    global cur_face
    if face == cur_face:
        return
    cur_face = face
    if face == FACE_EYES:
        car.Matrix_display(eyes)
    elif face == FACE_SMILE:
        car.Matrix_display(smile)
    elif face == FACE_SURPRISE:
        car.Matrix_display(surprise)
    else:
        car.Matrix_display(sad)


show_face(FACE_EYES)
time.sleep(1)              # Wait for the module to finish switching mode

while True:
    cam.read()

    offset = cam.lineOffset()      # Negative means the line is on the left, positive on the right
    valid = cam.lineValid()

    if DEBUG:
        cur = (offset, valid, cam.lineAngle())
        if cur != dbg_prev:
            print('Line data: offset=%d angle=%d valid=%d' % (cur[0], cur[2], cur[1]))
            dbg_prev = cur

    if valid == LINE_OK:
        # Line detected: convert the PD output into a speed difference
        lost_start = 0
        show_face(FACE_SMILE)

        error = offset

        d_error = 0
        if has_last:
            d_error = error - last_error
            if d_error > D_MAX:
                d_error = D_MAX
            elif d_error < -D_MAX:
                d_error = -D_MAX

        correction = int(error * KP + d_error * KD)

        if correction > TURN_MAX:
            correction = TURN_MAX
        elif correction < -TURN_MAX:
            correction = -TURN_MAX

        car.drive(BASE_SPEED + correction, BASE_SPEED - correction)

        last_error = error
        has_last = True

    else:
        # Line lost: search in the direction of the last offset
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < LOST_TIMEOUT:
            show_face(FACE_SURPRISE)
            if last_error > SEARCH_DEAD:
                car.drive(SEARCH_SPEED, -SEARCH_SPEED)     # The line was on the right
            elif last_error < -SEARCH_DEAD:
                car.drive(-SEARCH_SPEED, SEARCH_SPEED)     # The line was on the left
            else:
                car.stop_motor()                           # Direction unknown
        else:
            show_face(FACE_SAD)
            car.stop_motor()

    time.sleep_ms(5)
