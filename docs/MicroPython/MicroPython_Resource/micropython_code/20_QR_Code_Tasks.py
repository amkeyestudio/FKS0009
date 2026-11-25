import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

PAN_CENTER = 90

SPEED_MOVE = 150           # 直行 / 后退速度
SPEED_TURN_OUT = 150       # 转向弧线外侧轮速度
SPEED_TURN_IN = 50         # 转向弧线内侧轮速度
ACTION_TIME = 1000         # 每张卡走多久（ms）

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
        car.forward(SPEED_MOVE, SPEED_MOVE)                    # 直行一段
    elif key == 'BACKWARD':
        car.back(SPEED_MOVE, SPEED_MOVE)                       # 后退一段
    elif key == 'LEFT':
        car.drive(SPEED_TURN_IN, SPEED_TURN_OUT)               # 左慢右快，向左画弧
    elif key == 'RIGHT':
        car.drive(SPEED_TURN_OUT, SPEED_TURN_IN)               # 左快右慢，向右画弧
    else:
        return                                                 # 不是这四张卡，不做动作

    time.sleep_ms(ACTION_TIME)
    car.stop_motor()


show_face(FACE_EYES)
print('二维码任务卡已就绪')
time.sleep(1)              # 等模块切换模式完成

while True:
    cam.read()
    qr = cam.qr()

    # 画面里没有卡：清空 last_qr 解除锁定
    if qr == '':
        last_qr = ''
        car.stop_motor()
        show_face(FACE_EYES)
        time.sleep_ms(20)
        continue

    # 同一张卡一直举着：跳过，不重复执行
    if qr == last_qr:
        time.sleep_ms(20)
        continue

    # 新卡：先记录再执行，避免执行期间重复触发
    last_qr = qr
    print('扫到二维码：', qr, ' 字节数 ', len(qr))
    do_task(qr)

    time.sleep_ms(20)
