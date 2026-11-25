import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer

WIFI_SSID = '你的WiFi账号'
WIFI_PASS = '你的WiFi密码'

PAN_SERVO_PIN = 42
TRIG_PIN = 13
ECHO_PIN = 12

INDEX_HTML = """<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>迷你履带坦克 V4.0 - 联网测试</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;
     background:#f2f3f5;margin:0;padding:16px}
.card{background:#fff;border-radius:14px;padding:16px;max-width:420px;margin:0 auto}
h2{margin:4px 0 16px;font-size:20px}
button{height:56px;font-size:17px;border:none;border-radius:12px;color:#fff;
       background:#3f51b5;margin:4px;width:30%}
.wide{width:100%;background:#009688;margin-top:16px}
p{font-size:17px;margin:12px 0}
b{color:#e53935;font-size:22px}
</style></head><body>
<div class="card">
<h2>迷你履带坦克 V4.0</h2>
<p>云台角度：<b id="a">90</b> °</p>
<button onclick="pan(180)">左 180°</button>
<button onclick="pan(90)">中 90°</button>
<button onclick="pan(0)">右 0°</button>
<button class="wide" onclick="readD()">测一次距离</button>
<p>前方距离：<b id="d">--</b> cm</p>
</div>
<script>
function pan(v){
  fetch('/pan?angle='+v).then(r=>r.text()).then(t=>document.getElementById('a').innerText=t);
}
function readD(){
  document.getElementById('d').innerText='...';
  fetch('/distance').then(r=>r.text()).then(t=>document.getElementById('d').innerText=t);
}
</script>
</body></html>"""

car = MiniTrackedTankRobot()
car.Servo_init(PAN_SERVO_PIN)
car.Ultrasonic_init(TRIG_PIN, ECHO_PIN)

pan_angle = 90


def on_index(params):
    return INDEX_HTML


def on_pan(params):
    global pan_angle
    try:
        angle = int(params.get('angle', '90'))
    except ValueError:
        angle = 90
    if angle < 0:
        angle = 0
    if angle > 180:
        angle = 180
    pan_angle = angle
    car.Servo_set_angle(angle)
    return str(angle)


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
print('WiFi 已连接')
print('板子的 IP：' + wlan.ifconfig()[0])

server = WebServer(80)
server.route('/', on_index)
server.route('/pan', on_pan)
server.route('/distance', on_distance)

print('网页服务已启动')

while True:
    server.handle()
    car.quiet_motor()          # 空闲时反复压住电机，抵消舵机动作带来的串扰
