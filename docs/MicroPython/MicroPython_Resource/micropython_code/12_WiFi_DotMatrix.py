import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer

WIFI_SSID = 'your_wifi_ssid'
WIFI_PASS = 'your_wifi_password'

CLK_PIN = 9
DIN_PIN = 8
SCROLL_MS = 120            # Interval between column shifts (ms), smaller is faster
MAX_CHARS = 24             # Maximum number of characters

# Dot matrix column order: this module uses the normal order (address 0 is leftmost), no flip needed
# For a module with the reversed column order (text appears mirrored), set this to True
MIRROR_X = False

# 5x7 font: 5 columns per character, one byte per column, bit0 is the top row
# Index 0-25 are A-Z, 26-35 are 0-9, 36 is the space
FONT = [
    (0x7E, 0x11, 0x11, 0x11, 0x7E),  # A
    (0x7F, 0x49, 0x49, 0x49, 0x36),  # B
    (0x3E, 0x41, 0x41, 0x41, 0x22),  # C
    (0x7F, 0x41, 0x41, 0x22, 0x1C),  # D
    (0x7F, 0x49, 0x49, 0x49, 0x41),  # E
    (0x7F, 0x09, 0x09, 0x09, 0x01),  # F
    (0x3E, 0x41, 0x49, 0x49, 0x7A),  # G
    (0x7F, 0x08, 0x08, 0x08, 0x7F),  # H
    (0x00, 0x41, 0x7F, 0x41, 0x00),  # I
    (0x20, 0x40, 0x41, 0x3F, 0x01),  # J
    (0x7F, 0x08, 0x14, 0x22, 0x41),  # K
    (0x7F, 0x40, 0x40, 0x40, 0x40),  # L
    (0x7F, 0x02, 0x0C, 0x02, 0x7F),  # M
    (0x7F, 0x04, 0x08, 0x10, 0x7F),  # N
    (0x3E, 0x41, 0x41, 0x41, 0x3E),  # O
    (0x7F, 0x09, 0x09, 0x09, 0x06),  # P
    (0x3E, 0x41, 0x51, 0x21, 0x5E),  # Q
    (0x7F, 0x09, 0x19, 0x29, 0x46),  # R
    (0x46, 0x49, 0x49, 0x49, 0x31),  # S
    (0x01, 0x01, 0x7F, 0x01, 0x01),  # T
    (0x3F, 0x40, 0x40, 0x40, 0x3F),  # U
    (0x1F, 0x20, 0x40, 0x20, 0x1F),  # V
    (0x3F, 0x40, 0x38, 0x40, 0x3F),  # W
    (0x63, 0x14, 0x08, 0x14, 0x63),  # X
    (0x07, 0x08, 0x70, 0x08, 0x07),  # Y
    (0x61, 0x51, 0x49, 0x45, 0x43),  # Z
    (0x3E, 0x51, 0x49, 0x45, 0x3E),  # 0
    (0x00, 0x42, 0x7F, 0x40, 0x00),  # 1
    (0x42, 0x61, 0x51, 0x49, 0x46),  # 2
    (0x21, 0x41, 0x45, 0x4B, 0x31),  # 3
    (0x18, 0x14, 0x12, 0x7F, 0x10),  # 4
    (0x27, 0x45, 0x45, 0x45, 0x39),  # 5
    (0x3C, 0x4A, 0x49, 0x49, 0x30),  # 6
    (0x01, 0x71, 0x09, 0x05, 0x03),  # 7
    (0x36, 0x49, 0x49, 0x49, 0x36),  # 8
    (0x06, 0x49, 0x49, 0x29, 0x1E),  # 9
    (0x00, 0x00, 0x00, 0x00, 0x00),  # Space
]

INDEX_HTML = """<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dot Matrix Marquee</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;background:#1a1a1a;color:#ddd;
     text-align:center;margin:0;padding:20px}
.card{background:#242424;border-radius:14px;padding:18px;max-width:420px;margin:0 auto}
h2{margin:0 0 14px;font-size:18px;color:#4dd0e1;font-weight:500}
input{width:100%;box-sizing:border-box;height:52px;font-size:20px;text-align:center;
      border:none;border-radius:10px;background:#111;color:#eee;outline:none;
      letter-spacing:2px;text-transform:uppercase}
button{margin-top:12px;width:100%;height:52px;font-size:19px;border:none;border-radius:10px;
       background:#00897b;color:#fff}
p{font-size:13px;color:#777;margin:14px 0 0;line-height:1.6}
</style></head><body>
<div class="card">
<h2>Dot Matrix Marquee</h2>
<input id="t" maxlength="24" placeholder="Enter letters or digits">
<button onclick="send()">Send to the matrix</button>
<p id="r">Letters, digits and spaces only, up to 24 characters</p>
</div>
<script>
function send(){
  var t=document.getElementById('t').value;
  fetch('/msg?t='+encodeURIComponent(t)).then(function(){
    document.getElementById('r').innerText='Sent: '+t.toUpperCase();
  });
}
</script>
</body></html>"""

car = MiniTrackedTankRobot()
car.Matrix_init(CLK_PIN, DIN_PIN)
car.Matrix_clear()

msg = 'HELLO'
scroll_pos = 0
last_scroll = 0
need_redraw = True


def text_width(text):
    # 6 columns per character, no trailing gap after the last one
    if len(text) == 0:
        return 0
    return len(text) * 6 - 1


def char_index(c):
    if 'A' <= c <= 'Z':
        return ord(c) - ord('A')
    if '0' <= c <= '9':
        return 26 + ord(c) - ord('0')
    return 36


def get_column(idx):
    if idx < 0:
        return 0
    char_idx = idx // 6
    col_idx = idx % 6
    if char_idx >= len(msg):
        return 0
    if col_idx >= 5:
        return 0
    return FONT[char_index(msg[char_idx])][col_idx]


def send_frame(cols):
    # Reverse the 16 columns only when MIRROR_X is True (module with the reversed column order)
    if MIRROR_X:
        cols = bytes(reversed(cols))
    car.Matrix_display(bytes(cols))


def render_static():
    # Center the whole text within one 16-column screen
    total = text_width(msg)
    start_col = (16 - total) // 2
    cols = bytearray(16)
    for i in range(16):
        idx = i - start_col
        cols[i] = get_column(idx) if idx >= 0 else 0
    send_frame(cols)


def render_frame(pos):
    cols = bytearray(16)
    for i in range(16):
        cols[i] = get_column(pos + i)
    send_frame(cols)


def on_index(params):
    return INDEX_HTML


def on_msg(params):
    global msg, scroll_pos, need_redraw
    text = params.get('t', '').strip().upper()
    # Same as charToIndex in the Arduino version: invalid characters become spaces instead of being dropped
    text = ''.join(c if (('A' <= c <= 'Z') or ('0' <= c <= '9')) else ' ' for c in text)
    if len(text) > MAX_CHARS:
        text = text[:MAX_CHARS]
    if len(text) > 0:
        msg = text
        scroll_pos = 0
        need_redraw = True
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
print('Marquee display ready, open on your phone: http://' + wlan.ifconfig()[0])

server = WebServer(80)
server.route('/', on_index)
server.route('/msg', on_msg)

# ==================== Main loop ====================
while True:
    server.handle()

    # Fits on one screen: show it centered and static
    if text_width(msg) <= 16:
        if need_redraw:
            render_static()
            need_redraw = False
        time.sleep_ms(20)
        continue

    # Wider than one screen: scroll horizontally
    if time.ticks_diff(time.ticks_ms(), last_scroll) < SCROLL_MS:
        time.sleep_ms(5)
        continue

    last_scroll = time.ticks_ms()
    render_frame(scroll_pos)
    scroll_pos = (scroll_pos + 1) % (text_width(msg) + 16)
