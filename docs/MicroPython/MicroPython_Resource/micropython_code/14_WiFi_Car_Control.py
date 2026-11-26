import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer

WIFI_SSID = 'Lance'
WIFI_PASS = 'MZW123456'

SPEED_RUN = 180
SPEED_TURN = 180
STOP_AFTER = 1500          # Auto stop when a command times out (ms)

INDEX_HTML = """<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mini Tracked Tank Robot V4.0 - Remote Car</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;
     background:#f2f3f5;margin:0;padding:16px}
.card{background:#fff;border-radius:14px;padding:16px;max-width:420px;margin:0 auto}
h2{margin:4px 0 16px;font-size:20px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{height:72px;font-size:24px;border:none;border-radius:14px;color:#fff;
       user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none}
.f{background:#4caf50}.b{background:#f44336}.l{background:#ff9800}
.r{background:#2196f3}.s{background:#607d8b}
.blank{background:transparent}
p{font-size:15px;color:#888;margin-top:14px}
</style></head><body>
<div class="card">
<h2>Hold a direction button to drive</h2>
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
<p>Release to stop automatically, it also stops after 1.5 s without commands</p>
</div>
<script>
function go(d){ fetch('/cmd?d='+d).catch(function(){}); }
</script>
</body></html>"""

car = MiniTrackedTankRobot()
car.Motor_init()
car.stop_motor()

last_cmd = time.ticks_ms()


def on_index(params):
    return INDEX_HTML


def on_cmd(params):
    global last_cmd
    last_cmd = time.ticks_ms()
    cmd = params.get('d', 's')

    if cmd == 'f':
        car.forward(SPEED_RUN, SPEED_RUN)
    elif cmd == 'b':
        car.back(SPEED_RUN, SPEED_RUN)
    elif cmd == 'l':
        car.left(SPEED_TURN, SPEED_TURN)
    elif cmd == 'r':
        car.right(SPEED_TURN, SPEED_TURN)
    else:
        car.stop_motor()
    return 'OK'


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

server = WebServer(80)
server.route('/', on_index)
server.route('/cmd', on_cmd)

print('Remote car ready')

while True:
    server.handle()

    # Stop when no command arrives in time
    if time.ticks_diff(time.ticks_ms(), last_cmd) > STOP_AFTER:
        car.stop_motor()
