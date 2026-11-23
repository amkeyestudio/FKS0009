# 项目 19：人脸跟随
# 接线：视觉模块 RX=IO18 / TX=IO17（9600）；电机 40/41/38/21；云台 42（本课固定朝正前）；点阵 9/8
# 现象：上电先用前 20 帧标定正前方，之后人往哪边偏，车就往哪边差速转向并慢慢靠近

import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

PAN_CENTER = 90
FOLLOW_SPEED = 100         # 跟随前进速度
KP = 0.5                   # 比例系数
TURN_MAX = 80              # 单侧修正上限
DEAD_ZONE = 8              # 对准死区
CALI_FRAMES = 20           # 标定帧数
LOST_STOP = 300            # 丢失超时（ms）

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
center_x = -1              # 标定中心，-1 表示未完成
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
time.sleep_ms(500)         # 等模块切换模式完成

while True:
    cam.read()

    if not cam.faceDetected():
        # 没看到人：超时停车
        show_face(FACE_EYES)
        if time.ticks_diff(time.ticks_ms(), last_seen) > LOST_STOP:
            car.stop_motor()
        time.sleep_ms(20)
        continue

    last_seen = time.ticks_ms()
    show_face(FACE_SMILE)

    x = cam.faceX()

    # 上电先标定正前方：取前 20 帧 X 的平均值
    if center_x < 0:
        sum_x += x
        cali_count += 1
        car.stop_motor()       # 标定期间不动

        if cali_count >= CALI_FRAMES:
            center_x = sum_x // cali_count
            print('画面中心已锁定：', center_x)

        time.sleep_ms(20)
        continue

    # 跟随：偏差换算成差速
    err = x - center_x         # 正在右，负在左

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
