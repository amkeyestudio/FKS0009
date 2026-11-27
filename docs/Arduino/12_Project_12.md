### Project 12: Wi-Fi Dot Matrix Message Display

#### **1. Introduction:**

![LED sign at a station](media/intro_12.jpg)

Back in Project 1 the dot matrix could only loop through a few patterns baked into the code. Now turn it into a real message board: connect your phone to the same Wi-Fi as the car, open the board's address in a browser, type a sentence, tap send — and the words roll across the dots.

The display is only 16 columns wide. One letter takes 5 columns plus 1 column of spacing, so 6 columns per character and at most two characters on screen at once — anything longer has to scroll, or you only see half of it. The scroll shifts one column every 120 ms, just about the pace the eye can follow.

The font is the unsung hero behind the scenes: 26 letters plus 10 digits, each character 5 columns wide with one byte per column. Want a different letter shape? Change those bytes.

On power-on the board prints its address on serial. Open that address on a phone on the same Wi-Fi and you get an input box and a send button. Send `HI` and it sits centered; send `LOVE` and it scrolls in from the right, leaving a gap of blank columns before starting over.

#### **2. Parts List:**
Parts used in this lesson:

| 8×16 LED dot matrix display ×1 | DRV8835 motor expansion board ×1 |
| :---: | :---: |
| ![](../media/kit_06.png) | ![](../media/kit_04.png) |
| ESP32-S3 development board ×1 | 2.4 GHz wireless router or phone hotspot ×1 |
| ![](../media/kit_03.png) |  |
> ⚠️ **The dot matrix was installed back in Chapter 2; this table is for reference only — there is nothing to take apart. The wiring notes below only show how the connections are made, for reference; no assembly or disassembly is required. For the complete parts list, see 1.4 Kit Parts List.**
#### **3. Parameters:**
| Item | Parameter |
| --- | --- |
| Wi-Fi mode | Station (STA), 2.4 GHz only |
| Web page port | 80 |
| Access | Open the IP printed on serial in a browser |
| Character set | A–Z, 0–9 and space; 37 in total |
| Font spec | 5×7; 5 columns per character, 1 byte per column |
| Character width | 6 columns (5 font bitmap columns + 1 spacing column) |
| Maximum characters | 24 |
| One-screen capacity | 16 columns, about two characters |
| Scroll interval | 120 ms per column |

#### **4. Wiring:**

| Signal | Controller GPIO |
| --- | :---: |
| Dot matrix CLK | IO9 |
| Dot matrix DIN | IO8 |

This lesson uses no motors or sensors — only the dot matrix is wired. Wi-Fi is built into the chip and needs no extra wiring.

![](media/8X16_connection_diagram.png)

#### **5. Flowchart:**

From tapping **Send** on the phone to seeing the message light up on the dot matrix, the signal passes through four stages:

![](media/wifi_matrix_flow.png)

#### **6. Test Code:**
> **Before uploading, change one thing**: replace `WIFI_SSID` and `WIFI_PASS` below with your own Wi-Fi name and password — otherwise it won't connect.
>
> Two more conditions must hold at the same time: the Wi-Fi must be **2.4 GHz** (the board doesn't support 5 GHz), and the phone and the car must be on the **same LAN** — otherwise the browser can't open the board's IP.

> **This project has two files**: `12_WiFi_DotMatrix.ino` and `webpage.h`. All the page HTML lives in `webpage.h`; the `.ino` keeps only the Arduino logic. Put both files in the same folder — if one is missing, compilation fails.

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi configuration =====
// Only 2.4 GHz is supported
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connected to module CLK
#define DIN_Pin 8  // Data, connected to module DIN

// ===== Scroll parameters =====
#define SCROLL_MS 120  // Scroll interval per column (ms), smaller is faster
#define MAX_CHARS  24  // Maximum number of characters

// ===== 5×7 font bitmap =====
// 5 columns per character, 1 byte/column, bit0 is the top row, bit6 is the bottom row
// Index 0~25 for A~Z, 26~35 for 0~9, 36 for space
const uint8_t FONT[37][5] = {
  {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
  {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
  {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
  {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
  {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
  {0x7F, 0x09, 0x09, 0x09, 0x01},  // F
  {0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
  {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
  {0x00, 0x41, 0x7F, 0x41, 0x00},  // I
  {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
  {0x7F, 0x08, 0x14, 0x22, 0x41},  // K
  {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
  {0x7F, 0x02, 0x0C, 0x02, 0x7F},  // M
  {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
  {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
  {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
  {0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
  {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
  {0x46, 0x49, 0x49, 0x49, 0x31},  // S
  {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
  {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
  {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
  {0x3F, 0x40, 0x38, 0x40, 0x3F},  // W
  {0x63, 0x14, 0x08, 0x14, 0x63},  // X
  {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
  {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
  {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
  {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
  {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
  {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
  {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
  {0x00, 0x00, 0x00, 0x00, 0x00}   // Space
};

WebServer server(80);      // Web server
String msg = "HELLO";      // Text to display
int scrollPos = 0;         // Current start column
unsigned long lastScrollAt = 0;
bool needRedraw = true;    // Text changed, static display needs redraw

void setup() {
  // Pull both lines high, set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  Serial.begin(115200);
  delay(300);

  // Connect to Wi-Fi
  Serial.print("Connecting ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Dot matrix display ready, open on your phone: http://");
  Serial.println(WiFi.localIP());

  // Home page
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // Receive text
  server.on("/msg", []() {
    String t = server.arg("t");
    t.trim();
    t.toUpperCase();                       // Font contains uppercase only
    if (t.length() > MAX_CHARS) {
      t = t.substring(0, MAX_CHARS);       // Truncate if too long
    }
    if (t.length() > 0) {
      msg = t;
      scrollPos = 0;            // Start scrolling in from the rightmost column
      needRedraw = true;        // Mark as needing redraw
    }
    server.sendHeader("Location", "/");     // Back to home page
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void loop() {
  server.handleClient();  // Handle web requests

  // ---- Fits on one screen: centered static display ----
  if (textWidth() <= 16) {
    if (needRedraw) {     // Redraw only when text changes
      renderStatic();
      needRedraw = false;
    }
    return;
  }

  // ---- Exceeds one screen: scroll horizontally ----
  if (millis() - lastScrollAt < SCROLL_MS) return;
  lastScrollAt = millis();

  renderFrame(scrollPos);
  scrollPos++;

  // After scrolling the whole text, leave 16 blank columns then loop
  int totalCols = textWidth() + 16;
  if (scrollPos >= totalCols) {
    scrollPos = 0;
  }
}

// ===== Width calculation =====
// Number of columns occupied by the whole text
// 6 columns per character, no spacing after the last character
int textWidth() {
  if (msg.length() == 0) return 0;
  return msg.length() * 6 - 1;
}

// ===== Static display =====
// Display the whole text centered within 16 columns
void renderStatic() {
  int total = textWidth();
  int startCol = (16 - total) / 2;  // Number of blank columns on the left

  uint8_t cols[16];
  for (int i = 0; i < 16; i++) {
    int idx = i - startCol;                 // Map to the column index in the whole text
    cols[i] = (idx >= 0) ? getColumn(idx) : 0;  // Fill blank before the start of the text
  }

  matrix_display(cols);
}

// ===== Render one frame =====
// Capture one screen starting from column pos of the whole text
void renderFrame(int pos) {
  uint8_t cols[16];

  for (int i = 0; i < 16; i++) {
    cols[i] = getColumn(pos + i);
  }

  matrix_display(cols);
}

// Get column idx of the whole text
uint8_t getColumn(int idx) {
  if (idx < 0) return 0;                    // Not yet on screen

  int charIdx = idx / 6;                    // Character index (6 columns per character: 5 font bitmap columns + 1 spacing column)
  int colIdx  = idx % 6;                    // Column index within the character

  if (charIdx >= (int)msg.length()) return 0;  // Beyond the end of the text
  if (colIdx >= 5) return 0;                   // Spacing column

  int fi = charToIndex(msg[charIdx]);
  return FONT[fi][colIdx];
}

// Convert character to font bitmap index
int charToIndex(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';       // A~Z → 0~25
  if (c >= '0' && c <= '9') return 26 + (c - '0'); // 0~9 → 26~35
  return 36;                                       // Treat other characters as space
}

// ===== Dot matrix driver =====
// Write 16 bytes of font bitmap to the dot matrix display buffer
void matrix_display(unsigned char matrix_value[]) {
  // 0x40: write data mode, auto-increment address
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0: display buffer start address 0
  IIC_start();
  IIC_send(0xC0);

  // Write 16 columns consecutively
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8A: display on, brightness 8/16
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// Start condition: falling edge of DIN while CLK is HIGH
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// Send 1 byte, LSB first: set data while CLK is LOW, sample on rising edge
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);  // Pull clock low, prepare data
    delayMicroseconds(3);

    // Send bit by bit starting from bit0
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // Pull clock high, module samples
    delayMicroseconds(3);

    send_data = send_data >> 1;  // Shift right to get the next bit
  }
}

// Stop condition: rising edge of DIN while CLK is HIGH
void IIC_end() {
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
}
```

#### **7. Code Explanation:**
**(1) Credentials, pins and two parameters**

```cpp
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

#define CLK_Pin 9  // Clock, connected to module CLK
#define DIN_Pin 8  // Data, connected to module DIN

#define SCROLL_MS 120  // Scroll interval per column (ms), smaller is faster
#define MAX_CHARS  24  // Maximum number of characters
```

Both Wi-Fi constants must be changed to your own before compiling, or the connection will fail. `SCROLL_MS 120` is how long each column stays put during scrolling; a full 16 columns take 1.92 seconds. Set it to 60 and it runs twice as fast, but the text blurs into an unreadable smear. `MAX_CHARS 24` is the input limit — anything longer is truncated.

**(2) The font: 37 characters, 5 columns each**

```cpp
const uint8_t FONT[37][5] = {
```

A two-dimensional array: the first dimension holds 37 characters (0–25 for A–Z, 26–35 for 0–9 and 36 for space), the second holds 5 bytes — one character occupies 5 columns, with each bit of a byte controlling one row, bit0 at the top.

The display is only 8 rows high, so the ascenders and descenders that distinguish lowercase letters (b, p, g) can't be represented at that height. That's why the font contains only capitals, digits and spaces. To add a symbol, append 5 bytes to `FONT` and update the index numbers accordingly.

**(3) Four global state variables**

```cpp
String msg = "HELLO";      // Text to display
int scrollPos = 0;         // Current start column
unsigned long lastScrollAt = 0;
bool needRedraw = true;    // Text changed, static display needs redraw
```

These four variables are the only channel between the web callbacks and `loop()`: a callback changes `msg`, and `loop()` reads `msg` to decide what to draw. `scrollPos` tracks which column the scroll has reached; `lastScrollAt` stores the time of the last scroll step (no `delay` here — the interval comes from `millis()`); `needRedraw` means "the text changed, redraw the static image once".

**(4) `setup()`: connect to Wi-Fi**

```cpp
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
```

`WIFI_STA` is "connect to somebody else's router" mode, not "act as a hotspot". The `while` loop blocks here until the connection succeeds, printing a dot every half second — if it never connects it keeps printing dots, so when you see that, check the credentials and the 2.4 GHz band first.

Once connected, `WiFi.localIP()` is the address the phone needs to visit.

**(5) The web part: it only hands the text in**

```cpp
  // Home page
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // Receive text
  server.on("/msg", []() {
    String t = server.arg("t");
    t.trim();
    t.toUpperCase();                       // Font contains uppercase only
    if (t.length() > MAX_CHARS) {
      t = t.substring(0, MAX_CHARS);       // Truncate if too long
    }
    if (t.length() > 0) {
      msg = t;
      scrollPos = 0;            // Start scrolling in from the rightmost column
      needRedraw = true;        // Mark as needing redraw
    }
    server.sendHeader("Location", "/");     // Back to home page
    server.send(302, "text/plain", "");
  });

  server.begin();
```

This is the standard WebServer pattern and not the focus of the lesson; it's enough to know what it does: `/` sends the HTML stored in `webpage.h`, while `/msg` receives the text submitted by the phone, **trims whitespace, converts it to uppercase and truncates it to 24 characters** (the font has only capitals and digits), stores it in `msg`, then redirects with a 302 back to the home page — without the redirect, refreshing the page would resubmit the previous text.

The callbacks only change three global variables and draw nothing. The real drawing happens in `loop()` — the sections below explain how that text becomes an image on 16 columns of dots.

**(6) `loop()`: if it fits, hold it still**

```cpp
void loop() {
  server.handleClient();  // Handle web requests

  // ---- Fits on one screen: centered static display ----
  if (textWidth() <= 16) {
    if (needRedraw) {     // Redraw only when text changes
      renderStatic();
      needRedraw = false;
    }
    return;
  }
```

`handleClient()` must be called on every pass or the page won't respond — it only checks "is there a request?", returns immediately when there isn't, and never blocks.

`loop()` runs thousands of times a second. Without the `needRedraw` flag, a static image would be redrawn thousands of times, each redraw costing more than twenty GPIO toggles plus four I²C transactions — burning CPU for nothing and slowing the web page down. With the flag, the same sentence is drawn only once.

That `return` matters too: when the text fits, end the pass right there and skip the scrolling logic.

**(7) `loop()`: if it doesn't fit, scroll**

```cpp
  // ---- Exceeds one screen: scroll horizontally ----
  if (millis() - lastScrollAt < SCROLL_MS) return;
  lastScrollAt = millis();

  renderFrame(scrollPos);
  scrollPos++;

  // After scrolling the whole text, leave 16 blank columns then loop
  int totalCols = textWidth() + 16;
  if (scrollPos >= totalCols) {
    scrollPos = 0;
  }
}
```

Using `millis()` for the interval instead of `delay(120)`: `delay` would freeze the whole `loop()`, and during that freeze nothing would handle web requests, so tapping Send on the phone would hang. Comparing timestamps lets us return immediately after shifting one column, leaving the next pass free to pick up requests.

`totalCols = textWidth() + 16` means "the full width of the sentence plus one blank screen". After the sentence finishes scrolling, the screen empties once before starting over, which gives the display a sense of rhythm.

**(8) `textWidth()`: six columns per character**

```cpp
int textWidth() {
  if (msg.length() == 0) return 0;
  return msg.length() * 6 - 1;
}
```

Each character takes 6 columns: 5 columns of font bitmap plus 1 spacing column. The `-1` is because no spacing is needed after the last character.

So `"AB"` is 6+6−1 = 11 columns and `"ABC"` is 17 — just over 16, so scrolling starts. A 16-column screen can hold at most two characters statically; three or more and it scrolls.

**(9) Mapping the whole sentence onto 16 columns: `renderStatic()` and `renderFrame()`**

```cpp
void renderStatic() {
  int total = textWidth();
  int startCol = (16 - total) / 2;  // Number of blank columns on the left

  uint8_t cols[16];
  for (int i = 0; i < 16; i++) {
    int idx = i - startCol;                 // Map to the column index in the whole text
    cols[i] = (idx >= 0) ? getColumn(idx) : 0;  // Fill blank before the start of the text
  }

  matrix_display(cols);
}

void renderFrame(int pos) {
  uint8_t cols[16];

  for (int i = 0; i < 16; i++) {
    cols[i] = getColumn(pos + i);
  }

  matrix_display(cols);
}
```

Both functions do one thing: **build 16 columns of data and hand them to `matrix_display()` to write to the screen**. Only the starting-point math differs.

`renderStatic()` centers the text, so it first works out how many blank columns to leave on the left: `(16 - total) / 2`. For an 11-column sentence that's 2 blanks, screen columns 0 and 1 stay empty and the text begins at column 2. In the loop, `i - startCol` converts a "screen column" into a "sentence column"; for the first two columns the result is negative, meaning we haven't reached the start of the sentence yet, so we fill with 0 (off).

`renderFrame()` is for scrolling, with the starting point passed in as `pos`: screen column `i` takes sentence column `pos + i`. Add one to `pos` each frame and the whole window shifts one column to the right, which looks like the text moving left. Past the end of the sentence `getColumn()` returns 0, leaving natural blank space.

Both functions end with `matrix_display(cols)` — send 16 bytes and that frame appears on the display. The dot matrix driver itself was covered in Project 1 and isn't expanded on here.

**(10) `getColumn()`: compute one column at a time**

```cpp
uint8_t getColumn(int idx) {
  if (idx < 0) return 0;                    // Not yet on screen

  int charIdx = idx / 6;                    // Character index (6 columns per character: 5 font bitmap columns + 1 spacing column)
  int colIdx  = idx % 6;                    // Column index within the character

  if (charIdx >= (int)msg.length()) return 0;  // Beyond the end of the text
  if (colIdx >= 5) return 0;                   // Spacing column

  int fi = charToIndex(msg[charIdx]);
  return FONT[fi][colIdx];
}
```

Each scrolling frame needs only 16 columns, so there's no reason to assemble the whole sentence — hundreds of columns — in memory first. Given a global column index `idx`, divide by 6 to get the character index and take the remainder mod 6 to get the column within that character, then fetch it straight from the font.

`colIdx == 5` is the spacing column and returns 0 (off); past the end of the sentence also returns 0 (blank). Division and modulo must appear as a pair — using only one of them will run characters together.

`renderFrame(pos)` is simply `for (i=0..15) cols[i] = getColumn(pos + i)`, cutting a 16-column window starting at column `pos`. `renderStatic()` first computes the centering offset `startCol`, then uses `i - startCol` as the global column index.

**(11) `charToIndex()`: indices are packed consecutively**

```cpp
int charToIndex(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';       // A~Z → 0~25
  if (c >= '0' && c <= '9') return 26 + (c - '0'); // 0~9 → 26~35
  return 36;                                       // Treat other characters as space
}
```

`'A' - 'A' = 0` and `'Z' - 'A' = 25`, so letters use ASCII continuity to compute their index directly — no lookup table needed. Digits follow at 26–35, and entry 37 (index 36) is blank, catching everything the font doesn't contain — Chinese characters, punctuation and lowercase letters (already converted by `toUpperCase`) all land in that slot.

#### **8. Expected Behavior:**

After power-on, the serial port prints:

```text
Connecting Your WiFi name
.....
Dot matrix display ready, open on your phone: http://192.168.1.23
```

With the phone on the same Wi-Fi and that address opened in a browser, a dark page appears: an input box, a **Send to Dot Matrix** button and two lines of instructions below.

| Input | Dot matrix behavior |
| --- | --- |
| `HI` | Centered and held still |
| `LOVE` | Scrolls right to left, then leaves 16 blank columns before looping |
| `HELLO WORLD` | Scrolls with even spacing between characters |

> [Image to be added] A photo of the dot matrix at a mid-scroll position after entering `LOVE`.

#### **9. Troubleshooting:**

**If Wi-Fi won't connect or the page won't open, check in this order:**

(1) **Did you change the credentials?** If `WIFI_SSID` and `WIFI_PASS` at the top of the code are still placeholders when you flash, it will never connect; replace them with your router's name and password.

(2) **Nothing at all on serial?** Press the RESET button on the board and try again — the previous sketch may be stuck.

![](media/RESET.png)

(3) **Dots keep printing on serial, no IP?** Wi-Fi connection failed. The router must be **2.4 GHz**; the board doesn't support 5 GHz. If you have no 2.4 G network, turn on a phone hotspot (hotspots default to 2.4 G).

(4) **Got an IP but the page won't open?** The address must include `http://`, and the IP must be the one printed on serial; the phone must be on the same Wi-Fi, and the first three octets of both IPs must match.

(5) **Same subnet but still won't open?** Log in to the router settings and turn off **AP isolation**; when it's enabled, devices on the same Wi-Fi can't reach one another.
