import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

BASE_SPEED = 130           # 直行基础速度
KP = 0.4                   # 比例系数
KD = 0.6                   # 微分系数，抑制超调
TURN_MAX = 90              # 单侧修正上限
D_MAX = 50                 # 微分项限幅

SEARCH_SPEED = 90          # 找线转速
SEARCH_DEAD = 15           # 找线死区
LOST_TIMEOUT = 600         # 找线超时（ms）

LINE_OK = 5                # 模块第三字段为 5 表示识别到线

DEBUG = False              # 调试开关：改成 True 可打印视觉模块回传的循线数据

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
dbg_prev = None            # 上次打印的循线数据，只在新数据到达时打印一次


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
time.sleep(1)              # 等模块切换模式完成

while True:
    cam.read()

    offset = cam.lineOffset()      # 负偏左，正偏右
    valid = cam.lineValid()

    if DEBUG:
        cur = (offset, valid, cam.lineAngle())
        if cur != dbg_prev:
            print('循线数据：offset=%d angle=%d valid=%d' % (cur[0], cur[2], cur[1]))
            dbg_prev = cur

    if valid == LINE_OK:
        # 识别到线：PD 换算差速
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
        # 未识别到线：按上次偏差方向找线
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < LOST_TIMEOUT:
            show_face(FACE_SURPRISE)
            if last_error > SEARCH_DEAD:
                car.drive(SEARCH_SPEED, -SEARCH_SPEED)     # 线原在右
            elif last_error < -SEARCH_DEAD:
                car.drive(-SEARCH_SPEED, SEARCH_SPEED)     # 线原在左
            else:
                car.stop_motor()                           # 方向不明
        else:
            show_face(FACE_SAD)
            car.stop_motor()

    time.sleep_ms(5)
