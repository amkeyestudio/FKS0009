#include <WiFi.h>
#include <WebServer.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi configuration =====
// Only supports 2.4 GHz
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connected to the module CLK pin
#define DIN_Pin 8  // Data, connected to the module DIN pin

// ===== Scroll parameters =====
#define SCROLL_MS 120  // Scroll interval per column (ms); smaller is faster
#define MAX_CHARS  24  // Maximum number of characters

// ===== 5×7 font library =====
// 5 columns per character, 1 byte per column, bit0 is the top row, bit6 is the bottom row
// Indices 0~25 for A~Z, 26~35 for 0~9, 36 for space
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
int scrollPos = 0;         // Current starting column
unsigned long lastScrollAt = 0;
bool needRedraw = true;    // Text changed; the static display needs a redraw

void setup() {
  // Pull both lines high to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  Serial.begin(115200);
  delay(300);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
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
    t.toUpperCase();                       // The font only contains uppercase letters
    if (t.length() > MAX_CHARS) {
      t = t.substring(0, MAX_CHARS);       // Truncate if too long
    }
    if (t.length() > 0) {
      msg = t;
      scrollPos = 0;            // Start scrolling in from the far right column
      needRedraw = true;        // Mark as needing a redraw
    }
    server.sendHeader("Location", "/");     // Return to the home page
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void loop() {
  server.handleClient();  // Handle web page requests

  // ---- Fits on one screen: display statically centered ----
  if (textWidth() <= 16) {
    if (needRedraw) {     // Redraw only when the text changes
      renderStatic();
      needRedraw = false;
    }
    return;
  }

  // ---- Wider than one screen: scroll horizontally ----
  if (millis() - lastScrollAt < SCROLL_MS) return;
  lastScrollAt = millis();

  renderFrame(scrollPos);
  scrollPos++;

  // After the full text has scrolled, leave 16 blank columns before looping
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
    cols[i] = (idx >= 0) ? getColumn(idx) : 0;  // Fill in blanks before the start of the text
  }

  matrix_display(cols);
}

// ===== Render one frame =====
// Take one screen starting from column pos of the whole text
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

  int charIdx = idx / 6;                    // Character index (6 columns per character: 5 font columns + 1 spacing column)
  int colIdx  = idx % 6;                    // Column index within the character

  if (charIdx >= (int)msg.length()) return 0;  // Past the end of the text
  if (colIdx >= 5) return 0;                   // Spacing column

  int fi = charToIndex(msg[charIdx]);
  return FONT[fi][colIdx];
}

// Convert a character to a font index
int charToIndex(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';       // A~Z → 0~25
  if (c >= '0' && c <= '9') return 26 + (c - '0'); // 0~9 → 26~35
  return 36;                                       // Treat other characters as spaces
}

// ===== Dot matrix driver =====
// Write 16 bytes of font data to the dot matrix display buffer
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

// Start condition: DIN falling edge while CLK is HIGH
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// Send one byte, least significant bit first: set data while CLK is LOW, sample on the rising edge
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);  // Pull the clock low to prepare the data
    delayMicroseconds(3);

    // Send bit by bit starting from bit0
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // Pull the clock high, module samples
    delayMicroseconds(3);

    send_data = send_data >> 1;  // Shift right to get the next bit
  }
}

// Stop condition: DIN rising edge while CLK is HIGH
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
