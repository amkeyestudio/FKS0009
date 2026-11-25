import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer

WIFI_SSID = '你的WiFi账号'
WIFI_PASS = '你的WiFi密码'

# ==================== 参数 ====================
MANUAL_TIMEOUT = 1500      # 手动模式指令超时（ms）
SERVO_STEP = 15            # 云台单次步进角度
CLAW_OPEN = 140
CLAW_CLOSE = 90

BASE_SPEED = 115           # 循迹直行速度
SPIN_SPEED = 125           # 急转外侧轮速度
INNER_SPEED = 95           # 急转内侧轮反转速度

SAFE_DISTANCE = 30         # 跟随：30~50 cm 前进
CLOSE_FOLLOW = 10          # 跟随：小于 10 cm 后退
MAX_DISTANCE = 50
CLOSE_AVOID = 15           # 避障：小于 15 cm 视为障碍

# 避障状态机，避免用 delay 卡住网页
AV_GO, AV_LEFT, AV_RIGHT, AV_TURN, AV_TURNING = 0, 1, 2, 3, 4

INDEX_HTML = """<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>迷你履带坦克 V4.0 - Wi-Fi 全功能车</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;
     background:#f2f3f5;margin:0;padding:16px}
.card{background:#fff;border-radius:14px;padding:16px;max-width:440px;margin:0 auto}
h2{margin:4px 0 10px;font-size:20px}
.dist{font-size:16px;color:#666;margin:0 0 14px}
.dist b{color:#e53935;font-size:20px}
button{font-size:22px;border:none;border-radius:14px;color:#fff;
       user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none}
.modes{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-bottom:14px}
.modes button{height:46px;font-size:16px;background:#3f51b5}
.on{background:#e53935 !important}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.grid button{height:66px;font-size:20px}
.f{background:#4caf50}.b{background:#f44336}.l{background:#ff9800}
.r{background:#2196f3}.s{background:#607d8b}
.p{background:#9c27b0}.c{background:#009688}
p.tip{font-size:14px;color:#999;margin:14px 0 0}
</style></head><body>
<div class="card">
<h2>当前模式：<span id="m">手动遥控</span></h2>
<p class="dist">前方距离：<b id="d">--</b> cm</p>
<div class="modes">
<button id="b-manual" class="on" onclick="mode('manual')">手动遥控</button>
<button id="b-line" onclick="mode('line')">循迹行驶</button>
<button id="b-avoid" onclick="mode('avoid')">超声波避障</button>
<button id="b-follow" onclick="mode('follow')">超声波跟随</button>
</div>
<div class="grid">
<button class="p" onclick="go('servo_plus')">云台◀</button>
<button class="f" onpointerdown="go('forward')" onpointerup="go('stop')" onpointercancel="go('stop')"
        onpointerleave="go('stop')">↑</button>
<button class="p" onclick="go('servo_minus')">云台▶</button>
<button class="l" onpointerdown="go('left')" onpointerup="go('stop')" onpointercancel="go('stop')"
        onpointerleave="go('stop')">←</button>
<button class="s" onclick="go('stop')">■</button>
<button class="r" onpointerdown="go('right')" onpointerup="go('stop')" onpointercancel="go('stop')"
        onpointerleave="go('stop')">→</button>
<button class="c" onclick="go('claw_close')">爪合</button>
<button class="b" onpointerdown="go('backward')" onpointerup="go('stop')" onpointercancel="go('stop')"
        onpointerleave="go('stop')">↓</button>
<button class="c" onclick="go('claw_open')">爪开</button>
</div>
<p class="tip">按住方向键开车，松手即停 ｜ 云台和夹爪点一下走一步</p>
</div>
<script>
var names={manual:'手动遥控',line:'循迹行驶',avoid:'超声波避障',follow:'超声波跟随'};
function mode(m){
  document.getElementById('m').innerText=names[m];
  for(var k in names){
    document.getElementById('b-'+k).className=(k===m)?'on':'';
  }
  fetch('/cmd?mode='+m).catch(function(){});
}
function go(cmd){ fetch('/cmd?move='+cmd).catch(function(){}); }
setInterval(function(){
  fetch('/distance').then(function(r){return r.text();})
    .then(function(t){document.getElementById('d').innerText=t;})
    .catch(function(){});
},1000);
</script>
</body></html>"""

# ==================== 字模 ====================
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])
alert = bytes([
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F,
    0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
])
heart = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
    0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
])

# ==================== 初始化 ====================
car = MiniTrackedTankRobot()
car.Motor_init()
car.Ultrasonic_init(13, 12)
car.Servo_init(42)
car.Claw_init(11)
car.Claw_set_angle(CLAW_CLOSE)     # 对齐 Arduino：上电夹紧到 90°
car.Matrix_init(9, 8)
car.Matrix_display(smile)
car.stop_motor()

mode = 'manual'
last_cmd = time.ticks_ms()
pan_angle = 90
left_distance = 0
right_distance = 0
av_step = AV_GO
av_time = 0
last_dir = 0
lost_start = 0
last_line_read = 0
last_follow_check = 0


def show_mode_icon():
    # 点阵与循迹共用 io8/io9：循迹模式关屏后把总线交给 I2C
    if mode == 'line':
        car.Matrix_init(9, 8)
        car.Matrix_off()
        car.Line_init(9, 8)
        return

    car.Matrix_init(9, 8)          # 内部会释放 I2C
    if mode == 'manual':
        car.Matrix_display(smile)
    elif mode == 'avoid':
        car.Matrix_display(alert)
    else:
        car.Matrix_display(heart)


def set_mode(m):
    global mode, av_step, lost_start, pan_angle, last_cmd
    if m not in ('manual', 'line', 'avoid', 'follow'):
        return

    changed = (mode != m)
    mode = m
    car.stop_motor()
    av_step = AV_GO
    lost_start = 0
    pan_angle = 90
    car.Servo_set_angle(90)
    last_cmd = time.ticks_ms()

    if changed:
        show_mode_icon()


# ==================== 1. 循迹 ====================
def run_line():
    global last_dir, lost_start, last_line_read

    if time.ticks_diff(time.ticks_ms(), last_line_read) < 5:
        return
    last_line_read = time.ticks_ms()

    A, B, C, D, E = car.Line_get()
    state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E

    if state != 0:
        lost_start = 0

    if state in (0b00100, 0b01110, 0b11111):
        car.drive(BASE_SPEED, BASE_SPEED)
        last_dir = 0
    elif state == 0b01100:
        car.drive(85, BASE_SPEED)
        last_dir = 1
    elif state in (0b01000, 0b11100):
        car.drive(45, SPIN_SPEED)
        last_dir = 1
    elif state in (0b11000, 0b10000):
        car.drive(-INNER_SPEED, SPIN_SPEED)
        last_dir = 1
    elif state == 0b00110:
        car.drive(BASE_SPEED, 85)
        last_dir = 2
    elif state in (0b00010, 0b00111):
        car.drive(SPIN_SPEED, 45)
        last_dir = 2
    elif state in (0b00011, 0b00001):
        car.drive(SPIN_SPEED, -INNER_SPEED)
        last_dir = 2
    elif state == 0b00000:
        # 五路全部脱线：按记忆方向找线
        if lost_start == 0:
            lost_start = time.ticks_ms()
        if time.ticks_diff(time.ticks_ms(), lost_start) < 1500:
            if last_dir == 1:
                car.drive(-INNER_SPEED, SPIN_SPEED)
            elif last_dir == 2:
                car.drive(SPIN_SPEED, -INNER_SPEED)
            else:
                car.stop_motor()
        else:
            car.stop_motor()

    # 其余组合是探头过渡态，保持上一条电机指令不动


# ==================== 2. 跟随 ====================
def run_follow():
    global last_follow_check

    if time.ticks_diff(time.ticks_ms(), last_follow_check) < 100:
        return
    last_follow_check = time.ticks_ms()

    d = car.Ultrasonic_distance()
    if SAFE_DISTANCE < d <= MAX_DISTANCE:
        car.forward(150, 150)
    elif 0 < d < CLOSE_FOLLOW:
        car.back(105, 150)
    else:
        car.stop_motor()


# ==================== 3. 避障 ====================
def run_avoidance():
    global av_step, av_time, pan_angle, left_distance, right_distance

    now = time.ticks_ms()

    if av_step == AV_GO:
        d = car.Ultrasonic_distance()
        if 0 < d <= CLOSE_AVOID:
            car.stop_motor()
            pan_angle = 180
            car.Servo_set_angle(180)
            av_time = now
            av_step = AV_LEFT
        else:
            car.forward(150, 150)

    elif av_step == AV_LEFT:
        if time.ticks_diff(now, av_time) >= 300:
            left_distance = car.Ultrasonic_distance()
            pan_angle = 0
            car.Servo_set_angle(0)
            av_time = now
            av_step = AV_RIGHT

    elif av_step == AV_RIGHT:
        if time.ticks_diff(now, av_time) >= 300:
            right_distance = car.Ultrasonic_distance()
            pan_angle = 90
            car.Servo_set_angle(90)
            av_time = now
            av_step = AV_TURN

    elif av_step == AV_TURN:
        if time.ticks_diff(now, av_time) >= 100:
            if left_distance > right_distance:
                car.left(255, 255)
            else:
                car.right(255, 255)
            av_time = now
            av_step = AV_TURNING

    elif av_step == AV_TURNING:
        if time.ticks_diff(now, av_time) >= 500:
            car.stop_motor()
            av_step = AV_GO


# ==================== 网页请求 ====================
def on_index(params):
    return INDEX_HTML


def on_cmd(params):
    global last_cmd, pan_angle

    if 'mode' in params:
        set_mode(params['mode'])
        return 'OK'

    move = params.get('move', '')

    # 云台与夹爪：任何模式下都能动
    if move == 'servo_plus':
        pan_angle = min(180, pan_angle + SERVO_STEP)
        car.Servo_set_angle(pan_angle)
    elif move == 'servo_minus':
        pan_angle = max(0, pan_angle - SERVO_STEP)
        car.Servo_set_angle(pan_angle)
    elif move == 'claw_open':
        car.Claw_set_angle(CLAW_OPEN)
    elif move == 'claw_close':
        car.Claw_set_angle(CLAW_CLOSE)
    else:
        # 行驶指令：先切回手动，再执行
        if mode != 'manual':
            set_mode('manual')
        last_cmd = time.ticks_ms()
        if move == 'forward':
            car.forward(180, 180)
        elif move == 'backward':
            car.back(180, 180)
        elif move == 'left':
            car.left(180, 180)
        elif move == 'right':
            car.right(180, 180)
        else:
            car.stop_motor()

    return 'OK'


def on_distance(params):
    return str(car.Ultrasonic_distance())


# ==================== 连接 Wi-Fi ====================
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
print('正在连接', WIFI_SSID)
wlan.connect(WIFI_SSID, WIFI_PASS)
while not wlan.isconnected():
    time.sleep(0.5)
    print('.', end='')
print()
print('板子的 IP：' + wlan.ifconfig()[0])

server = WebServer(80)
server.route('/', on_index)
server.route('/cmd', on_cmd)
server.route('/distance', on_distance)

print('网页服务已启动，默认手动模式')

while True:
    server.handle()

    if mode == 'line':
        run_line()
    elif mode == 'avoid':
        run_avoidance()
    elif mode == 'follow':
        run_follow()
    elif time.ticks_diff(time.ticks_ms(), last_cmd) > MANUAL_TIMEOUT:
        car.stop_motor()
