# 项目 17：颜色追踪与抓取
# 接线：视觉模块 RX=IO18 / TX=IO17（9600）；电机 40/41/38/21；云台 42；夹爪 11；点阵 9/8
# 现象：看到红色或黄色物块，靠近 → 夹取 → 按颜色转向 → 运送 → 放下 → 退回起点

import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

SPEED_RUN = 180
SPEED_TURN = 180
SPEED_APPROACH = 150

CLAW_OPEN = 140
CLAW_CLOSE = 90
PAN_CENTER = 90

T_APPROACH = 350           # 接近目标时长（ms）
T_TURN = 600               # 原地转 90° 时长（ms）
T_DELIVER = 800            # 运送时长（ms）

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
    # 去程：靠近 → 夹取 → 转向 → 运送 → 放下；回程按相反顺序退回起点
    print('1 低速靠近')
    car.forward(SPEED_APPROACH, SPEED_APPROACH)
    time.sleep_ms(T_APPROACH)
    car.stop_motor()

    print('2 夹取')
    car.Claw_set_angle(CLAW_CLOSE)
    time.sleep_ms(400)
    show_face(FACE_HEART)

    print('3 按颜色转向：红左、黄右')
    if color == 'RED':
        car.left(SPEED_TURN, SPEED_TURN)
    else:
        car.right(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(T_TURN)
    car.stop_motor()

    print('4 运送')
    car.forward(SPEED_RUN, SPEED_RUN)
    time.sleep_ms(T_DELIVER)
    car.stop_motor()

    print('5 松爪卸货')
    car.Claw_set_angle(CLAW_OPEN)
    time.sleep_ms(300)

    print('6 倒回转向点')
    car.back(SPEED_RUN, SPEED_RUN)
    time.sleep_ms(T_DELIVER)
    car.stop_motor()

    print('7 回正')
    if color == 'RED':
        car.right(SPEED_TURN, SPEED_TURN)
    else:
        car.left(SPEED_TURN, SPEED_TURN)
    time.sleep_ms(T_TURN)
    car.stop_motor()

    print('8 倒回起点')
    car.back(SPEED_APPROACH, SPEED_APPROACH)
    time.sleep_ms(T_APPROACH)
    car.stop_motor()

    show_face(FACE_SMILE)
    print('投放完成，已回到起点')


show_face(FACE_EYES)
print('颜色追踪车已就绪，把红色或黄色物块放到车前')
time.sleep(1)

while True:
    cam.read()
    color = cam.color()

    # 没有目标，或不是红黄两色：停车等待
    if color not in ('RED', 'YELLOW'):
        car.stop_motor()
        show_face(FACE_EYES)
        time.sleep_ms(20)
        continue

    print('识别到目标：', color)
    show_face(FACE_SMILE)
    cam.clearColor()

    pick_and_deliver(color)

    cam.flush()                 # 丢掉动作期间积压的数据
    cam.clearColor()
    show_face(FACE_EYES)
    time.sleep(2)               # 冷却 2 秒再找下一个目标
