### Project 12: Wi-Fi Dot-Matrix Marquee

#### **1. Introduction:**

![An LED sign at a station](media/intro_12.jpg){width=520px}

The dot matrix from Lesson 1 could only loop a few hard-coded patterns; now it becomes a marquee screen: your phone joins the same Wi-Fi as the car, you open the board's address in a browser, type a message, tap send, and it scrolls across the matrix.

This screen is only 16 columns wide. One letter takes 5 columns in the font, plus 1 column of spacing makes 6, so a screen holds two characters at most — anything longer has to scroll, otherwise you only see half of it. The scroll advances one column every 120 milliseconds, which is just about the right pace to read.

The font is the real star behind the scenes: 26 letters plus 10 digits, 5 columns per character, one byte per column. To change the lettering you change those bytes.

After power-on the Shell prints the address the board has been given; open it on a phone on the same Wi-Fi and you get an input box and a send button. Send HI and it sits centered and still; send LOVE and it scrolls from right to left, then leaves a stretch of blank columns before starting over.

#### **2. Parts List:**
Components used in this lesson:

|     |     |
| :---: | :---: |
| ![](../media/kit_06.png){width=170px}<br><span style="font-size:0.85em">8×16 LED dot matrix display ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 motor expansion board ×1</span> |
| ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 development board ×1</span> | <span style="font-size:0.85em">2.4 GHz wireless router or phone hotspot ×1</span> |
> ⚠️ **The dot matrix was installed back in Chapter 2. This table is for reference only, so there is no need to remove anything; the wiring notes below only describe the wiring method for reference and do not require disassembly. See the kit list in 1.4 for the full bill of materials.**

#### **3. Parameters:**
| Item | Parameter |
| --- | --- |
| Wi-Fi mode | STA, 2.4 GHz only |
| Web port | 80 |
| Access method | Open the IP printed by the Shell in a browser |
| Character set | A–Z, 0–9 and space, 37 in total |
| Font format | 5×7, 5 columns per character, 1 byte per column |
| Character width | 6 columns (5 columns of glyph + 1 column of spacing) |
| Maximum characters | 24 |
| One-screen capacity | 16 columns, about 2 characters |
| Scroll interval | 120 ms per column |

#### **4. Wiring:**

| Signal | Controller GPIO |
| --- | :---: |
| Dot matrix CLK | IO9 |
| Dot matrix DIN | IO8 |

This lesson uses no motors and no sensors, only the dot matrix. Wi-Fi is built into the chip, so no extra wiring is needed.

![](media/8X16_connection_diagram.png)

#### **5. Flowchart:**

From tapping **send** on the phone to lighting up the matrix, the message has to pass four stages:

![](media/wifi_matrix_flow.png)

#### **6. Test Code:**
> ⚠️ **Before uploading the code, upload `mini_tracked_tank_robot.py` and `mini_tracked_tank_robot_web.py` to the `lib/` directory on the board** (see Section 5.5), otherwise importing the library raises `ImportError`.

```python
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
```

#### **7. Code Explanation:**
**(1) Importing the libraries and the network module**

```python
import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer
```

Two more libraries than in Lesson 1: `network` is MicroPython's built-in Wi-Fi module, which gets the board onto the router, and `WebServer` comes from `mini_tracked_tank_robot_web`, which already wraps the web service. Both `.py` files have to be uploaded to `lib/` on the board first; miss one and you get an `ImportError`.

**(2) Initializing the dot matrix and the marquee state**

```python
car = MiniTrackedTankRobot()
car.Matrix_init(CLK_PIN, DIN_PIN)
car.Matrix_clear()

msg = 'HELLO'
scroll_pos = 0
last_scroll = 0
need_redraw = True
```

`car.Matrix_init(CLK_PIN, DIN_PIN)` puts CLK on IO9 and DIN on IO8, the same dot matrix as in Lesson 1. `msg` holds the string currently being displayed and starts as `HELLO`; `scroll_pos` is the scrolling start offset, `last_scroll` records when the last scroll happened, and `need_redraw` is a switch — a new string sets it to `True` so the main loop redraws once.

**(3) The 5×7 font: 5 columns per character**

```python
# 5x7 font: 5 columns per character, one byte per column, bit0 is the top row
# Index 0-25 are A-Z, 26-35 are 0-9, 36 is the space
FONT = [
    (0x7E, 0x11, 0x11, 0x11, 0x7E),  # A
```

The font is a list where indices 0–25 hold the letters, 26–35 the digits and 36 the space, exactly matching what `char_index()` returns. Each character is 5 bytes (a tuple), one byte per column, with bit0 as the top row — the same idea as the 16-byte glyphs in Lesson 1, only scaled down from **one whole screen of picture** to **one character in 5 columns**. To change the lettering, change these hexadecimal values.

**(4) Splitting text into columns of glyphs**

```python
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
```

`char_index()` converts a letter or digit into a font index. `get_column(idx)` is the heart of it: it treats the whole sentence as one very long **strip of columns**, and given any column number `idx` it works out with `//6` which character that belongs to and with `%6` which column of that character it is (each character occupies 6 columns = 5 columns of glyph + 1 blank). Anything outside the range, or falling in the gap between characters, returns 0 (LED off). Scrolling only changes the starting point it reads from; the glyphs themselves never move.

**(5) Two ways of rendering: centered static and horizontal scroll**

```python
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
```

Each frame builds a 16-element `bytearray` and fills it column by column. `render_static()` works out the centered start `start_col`, so a short message sits in the middle of the screen; `render_frame(pos)` takes 16 consecutive columns starting at `pos`, and incrementing `pos` by one shifts everything left by one column. When `MIRROR_X` is `True`, `send_frame()` reverses the 16 columns to suit a matrix module with reversed column order — if the text comes out mirrored, set it to `True`. Finally everything is converted to `bytes` and handed to `car.Matrix_display()`.

**(6) The web part: it only hands the text over**

```python
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
```

This is the standard way of using the `WebServer` library and not the focus of this lesson; knowing what it does is enough: `route()` ties a URL to a function, the function is called when the browser visits that URL, and its return value goes straight back to the browser. `/` goes to `on_index()` which returns the whole HTML page, and `/msg?t=...` goes to `on_msg()`.

`params` is the dictionary of URL parameters the library has parsed for you. The few lines inside `on_msg()` are the ones that matter for the display: strip the spaces, convert to uppercase (the font holds uppercase letters and digits only), turn anything that is not a letter or a digit into a space as a placeholder, and cut anything longer than 24 characters. Once that is done it updates `msg`, resets the scroll position, turns the redraw flag on and returns `'OK'`.

```python
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
```

The Wi-Fi lines are boilerplate: `STA_IF` is client mode, which makes the board join a router the way a phone does; it counts as connected only when `isconnected()` is true, and `ifconfig()[0]` fetches the board's IP so it can be printed. The address may change on every power-on, so trust whatever the Shell prints.

The last three lines are what wires up the web pages: `WebServer(80)` starts the service on port 80 (browsers connect there by default, which is why the address needs no `:80`), and the two `route()` calls bind the home page and the submit endpoint. To add another endpoint, add another `route()` in the same style.

The real work is what follows: how that text becomes a picture on the 16-column dot matrix.

**(7) The main loop: choosing between static and scrolling**

```python
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
```

Every round calls `server.handle()` first, otherwise nobody handles the web requests. `text_width()` works out how many columns the whole message takes: 16 or fewer goes into the static branch, which draws once only when `need_redraw` is true and then clears the switch so the frame is not redrawn every cycle. More than 16 goes into the scrolling branch: `ticks_diff()` provides non-blocking timing, so the loop skips while `SCROLL_MS` (120 ms) has not elapsed and only advances one column when it has. `scroll_pos` wraps with a modulo, and the `+ 16` is there so the message scrolls fully off the screen with a stretch of blank columns before it starts again, instead of the end butting straight against the start. Adjusting `SCROLL_MS` changes the scrolling speed.

#### **8. Expected Behavior:**
After power-on the Shell prints:

```text
Connecting to YourWiFiName
.....
Marquee display ready, open on your phone: http://192.168.1.23
```

Join the same Wi-Fi on your phone and enter that address in a browser: a dark page appears with an input box, a **Send to matrix** button and two lines of explanation below.

| Input | Dot matrix behavior |
| --- | --- |
| `HI` | Centered and static |
| `LOVE` | Scrolls from right to left, leaves 16 blank columns, then repeats |
| `HELLO WORLD` | Scrolls, with even spacing between characters |

> [Image to be added] A photo of the dot matrix at the middle of its scroll after entering `LOVE`.

#### **9. Troubleshooting:**

**If the Wi-Fi will not connect or the page will not open, check in this order:**

(1) **Have the credentials been changed?** If `WIFI_SSID` and `WIFI_PASS` at the top of the code are still placeholders when you run it, it will certainly not connect; change them to your own router's name and password.

(2) **Nothing at all in the Shell.** Press the RESET button on the board and try again; the previous program may be stuck.

![](media/RESET.png){width=340px}

(3) **The Shell keeps printing dots and never gets an IP.** The Wi-Fi connection failed. The router must be **2.4 GHz**; the board does not support 5 GHz, and if you have no 2.4 GHz network, turn on your phone's hotspot (hotspots default to 2.4 GHz).

(4) **An IP appears but the page will not open.** The address needs `http://`, and the IP must be the one the Shell printed; the phone has to be on the same Wi-Fi, and the first three parts of both IPs must match.

(5) **Same subnet, still will not open.** Go into the router settings and turn off **AP isolation**; with it on, devices on the same Wi-Fi cannot reach each other.

(6) A Shell `ImportError: no module named 'mini_tracked_tank_robot_web'` means the WebServer library is not on the board. Use Thonny to save `mini_tracked_tank_robot.py` and `mini_tracked_tank_robot_web.py` to `lib/` on the board and run again.

(7) The page opens but tapping **Send to matrix** changes nothing on the display, which usually means the endpoints do not match: check whether the path registered by `server.route('/msg', on_msg)` matches the address used by `fetch` in the page, and make sure `server.handle()` keeps being called in the main loop — leave it out and nobody handles the requests.

(8) If not a single LED lights up, check the dot matrix wiring first — `CLK_PIN = 9` and `DIN_PIN = 8`; with CLK and DIN swapped nothing will display at all, so swap them and run again.
