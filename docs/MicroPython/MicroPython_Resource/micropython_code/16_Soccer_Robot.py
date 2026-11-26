import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer

WIFI_SSID = 'Lance'
WIFI_PASS = 'MZW123456'

SPEED_RUN = 180
SPEED_TURN = 180
STOP_AFTER = 1500          # Auto stop when a command times out (ms)

CLAW_OPEN = 140            # Open: the ball sits inside the claw
CLAW_CLOSE = 90            # Closed: the ball is squeezed out

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

INDEX_HTML = """<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mini Tracked Tank Robot V4.0 - Soccer Car</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;
     background:#f2f3f5;margin:0;padding:14px}
.card{background:#fff;border-radius:14px;padding:14px;max-width:420px;margin:0 auto 12px}
h2{margin:2px 0 12px;font-size:19px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:9px}
button{height:66px;font-size:22px;border:none;border-radius:14px;color:#fff;
       user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none}
.f{background:#4caf50}.b{background:#f44336}.l{background:#ff9800}
.r{background:#2196f3}.s{background:#607d8b}
.g{background:#009688}.w{background:#ff5722}.k{background:#795548}
.blank{background:transparent}
.score{font-size:40px;font-weight:700;color:#e53935;margin:6px 0}
p{font-size:13px;color:#888;margin:8px 0 0}
</style></head><body>

<div class="card">
<h2>Score</h2>
<div class="score" id="sc">0</div>
<p id="st">Hold a direction button to drive</p>
</div>

<div class="card">
<h2>Driving</h2>
<div class="grid">
<div class="blank"></div>
<button class="f" onpointerdown="go('f')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">↑</button>
<div class="blank"></div>
<button class="l" onpointerdown="go('l')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">←</button>
<button class="s" onclick="go('s')">■</button>
<button class="r" onpointerdown="go('r')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">→</button>
<div class="blank"></div>
<button class="b" onpointerdown="go('b')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">↓</button>
<div class="blank"></div>
</div>
</div>

<div class="card">
<h2>Claw</h2>
<div class="grid" style="grid-template-columns:repeat(2,1fr)">
<button class="g" onclick="go('carry')">Carry</button>
<button class="w" onclick="go('shoot')">Shoot</button>
</div>
<div class="grid" style="grid-template-columns:repeat(2,1fr);margin-top:9px">
<button class="k" onclick="go('goal')">Score a goal</button>
<button class="k" onclick="go('reset')">Reset score</button>
</div>
<p>Press "Carry" to open the claw and hold the ball; press "Shoot" to close the claw and squeeze the ball out</p>
</div>

<script>
function go(d){
  fetch('/cmd?d='+d).then(function(r){return r.text();}).then(function(t){
    if(t.indexOf(',')>0){
      var a=t.split(',');
      document.getElementById('sc').innerText=a[0];
      document.getElementById('st').innerText=a[1];
    }
  }).catch(function(){});
}
function tick(){
  fetch('/score').then(function(r){return r.text();}).then(function(t){
    document.getElementById('sc').innerText=t.split(',')[0];
  }).catch(function(){});
}
setInterval(tick,1000);
</script>
</body></html>"""

car = MiniTrackedTankRobot()
car.Motor_init()
car.Claw_init(11)
car.Matrix_init(9, 8)
car.Claw_set_angle(CLAW_OPEN)
car.stop_motor()

score = 0
last_cmd = time.ticks_ms()
smile_until = 0
cur_face = -1


def show_face(face):
    # Only refresh the display when the face changes, so the bus is not used constantly
    global cur_face
    if face == cur_face:
        return
    cur_face = face
    if face == FACE_EYES:
        car.Matrix_display(eyes)
    else:
        car.Matrix_display(smile)


def face_name():
    return 'Goal!' if cur_face == FACE_SMILE else 'Standby'


def run_cmd(d):
    # Return "score,state" so one web request refreshes both fields
    global score, last_cmd, smile_until

    last_cmd = time.ticks_ms()

    if d == 'f':
        car.forward(SPEED_RUN, SPEED_RUN)
    elif d == 'b':
        car.back(SPEED_RUN, SPEED_RUN)
    elif d == 'l':
        car.left(SPEED_TURN, SPEED_TURN)
    elif d == 'r':
        car.right(SPEED_TURN, SPEED_TURN)
    elif d == 'carry':
        car.Claw_set_angle(CLAW_OPEN)
    elif d == 'shoot':
        car.Claw_set_angle(CLAW_CLOSE)
    elif d == 'goal':
        score += 1
        show_face(FACE_SMILE)
        smile_until = time.ticks_ms() + 3000
    elif d == 'reset':
        score = 0
    else:
        car.stop_motor()

    return '{},{}'.format(score, face_name())


def on_index(params):
    return INDEX_HTML


def on_cmd(params):
    return run_cmd(params.get('d', 's'))


def on_score(params):
    return '{},{}'.format(score, face_name())


# ==================== Connect to Wi-Fi ====================
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
print('Connecting to', WIFI_SSID)
wlan.connect(WIFI_SSID, WIFI_PASS)
while not wlan.isconnected():
    time.sleep(0.5)
    print('.', end='')
print()
print('Board IP: ' + wlan.ifconfig()[0])

show_face(FACE_EYES)

server = WebServer(80)
server.route('/', on_index)
server.route('/cmd', on_cmd)
server.route('/score', on_score)

print('Soccer car ready')

while True:
    server.handle()

    # Stop when no command arrives in time
    if time.ticks_diff(time.ticks_ms(), last_cmd) > STOP_AFTER:
        car.stop_motor()

    # Return to the standby face 3 seconds after the smile
    if smile_until != 0 and time.ticks_ms() > smile_until:
        smile_until = 0
        show_face(FACE_EYES)
