import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

PAN_CENTER = 90
FOLLOW_SPEED = 100         # Following speed
KP = 0.5                   # Proportional gain
TURN_MAX = 80              # Maximum correction on one side
DEAD_ZONE = 8              # Alignment dead zone
CALI_FRAMES = 20           # Number of calibration frames
LOST_STOP = 300            # Lost target timeout (ms)

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

cam = AiCam()
cam.begin(tx=17, rx=18)
cam.setMode(cam.FACE)

cur_face = -1
center_x = -1              # Calibrated center, -1 means not calibrated yet
sum_x = 0
cali_count = 0
last_seen = 0


def show_face(face):
    global cur_face
    if face == cur_face:
        return
    cur_face = face
    if face == FACE_EYES:
        car.Matrix_display(eyes)
    else:
        car.Matrix_display(smile)


show_face(FACE_EYES)
time.sleep_ms(500)         # Wait for the module to finish switching mode

while True:
    cam.read()

    if not cam.faceDetected():
        # No face: stop after the timeout
        show_face(FACE_EYES)
        if time.ticks_diff(time.ticks_ms(), last_seen) > LOST_STOP:
            car.stop_motor()
        time.sleep_ms(20)
        continue

    last_seen = time.ticks_ms()
    show_face(FACE_SMILE)

    x = cam.faceX()

    # Calibrate straight ahead at power-on: average the X value of the first 20 frames
    if center_x < 0:
        sum_x += x
        cali_count += 1
        car.stop_motor()       # Stay still while calibrating

        if cali_count >= CALI_FRAMES:
            center_x = sum_x // cali_count
            print('Frame center locked: ', center_x)

        time.sleep_ms(20)
        continue

    # Following: convert the offset into a speed difference
    err = x - center_x         # Positive means right, negative means left

    if -DEAD_ZONE <= err <= DEAD_ZONE:
        car.forward(FOLLOW_SPEED, FOLLOW_SPEED)
    else:
        correction = int(err * KP)
        if correction > TURN_MAX:
            correction = TURN_MAX
        elif correction < -TURN_MAX:
            correction = -TURN_MAX
        car.drive(FOLLOW_SPEED + correction, FOLLOW_SPEED - correction)

    time.sleep_ms(20)
