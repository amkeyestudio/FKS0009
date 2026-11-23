#include <WiFi.h>
#include <WebServer.h>
#include "webpage.h"  // 网页内容

// ===== Wi-Fi 配置 =====
// 仅支持 2.4 GHz
const char* WIFI_SSID = "你的WiFi名字";
const char* WIFI_PASS = "你的WiFi密码";

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 滚动参数 =====
#define SCROLL_MS 120  // 每列滚动间隔（ms），越小越快
#define MAX_CHARS  24  // 最大字符数

// ===== 5×7 字库 =====
// 每字符 5 列，1 字节/列，bit0 为最上行，bit6 为最下行
// 索引 0~25 为 A~Z，26~35 为 0~9，36 为空格
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
  {0x00, 0x00, 0x00, 0x00, 0x00}   // 空格
};

WebServer server(80);      // 网页服务器
String msg = "HELLO";      // 待显示文字
int scrollPos = 0;         // 当前起始列
unsigned long lastScrollAt = 0;
bool needRedraw = true;    // 文字变化，静止显示需重绘

void setup() {
  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  Serial.begin(115200);
  delay(300);

  // 连接 Wi-Fi
  Serial.print("正在连接 ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("弹幕屏就绪，手机打开：http://");
  Serial.println(WiFi.localIP());

  // 首页
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // 接收文字
  server.on("/msg", []() {
    String t = server.arg("t");
    t.trim();
    t.toUpperCase();                       // 字库仅含大写
    if (t.length() > MAX_CHARS) {
      t = t.substring(0, MAX_CHARS);       // 超长截断
    }
    if (t.length() > 0) {
      msg = t;
      scrollPos = 0;            // 从最右列开始滚入
      needRedraw = true;        // 标记需重绘
    }
    server.sendHeader("Location", "/");     // 返回首页
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void loop() {
  server.handleClient();  // 处理网页请求

  // ---- 一屏装得下：居中静止显示 ----
  if (textWidth() <= 16) {
    if (needRedraw) {     // 仅在文字变化时重绘
      renderStatic();
      needRedraw = false;
    }
    return;
  }

  // ---- 超出一屏：横向滚动 ----
  if (millis() - lastScrollAt < SCROLL_MS) return;
  lastScrollAt = millis();

  renderFrame(scrollPos);
  scrollPos++;

  // 滚完整句再留 16 列空白后循环
  int totalCols = textWidth() + 16;
  if (scrollPos >= totalCols) {
    scrollPos = 0;
  }
}

// ===== 宽度计算 =====
// 整句占用的列数
// 每字符 6 列，末字符不计间隔
int textWidth() {
  if (msg.length() == 0) return 0;
  return msg.length() * 6 - 1;
}

// ===== 静止显示 =====
// 整句居中显示在一屏 16 列内
void renderStatic() {
  int total = textWidth();
  int startCol = (16 - total) / 2;  // 左侧留空列数

  uint8_t cols[16];
  for (int i = 0; i < 16; i++) {
    int idx = i - startCol;                 // 映射到整句列号
    cols[i] = (idx >= 0) ? getColumn(idx) : 0;  // 句首之前补空
  }

  matrix_display(cols);
}

// ===== 渲染一帧 =====
// 从整句第 pos 列开始截取一屏
void renderFrame(int pos) {
  uint8_t cols[16];

  for (int i = 0; i < 16; i++) {
    cols[i] = getColumn(pos + i);
  }

  matrix_display(cols);
}

// 取整句第 idx 列
uint8_t getColumn(int idx) {
  if (idx < 0) return 0;                    // 尚未进入屏幕

  int charIdx = idx / 6;                    // 字符索引（每字符 6 列：5 列字模 + 1 列间隔）
  int colIdx  = idx % 6;                    // 字符内列号

  if (charIdx >= (int)msg.length()) return 0;  // 超出句尾
  if (colIdx >= 5) return 0;                   // 间隔列

  int fi = charToIndex(msg[charIdx]);
  return FONT[fi][colIdx];
}

// 字符转字库索引
int charToIndex(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';       // A~Z → 0~25
  if (c >= '0' && c <= '9') return 26 + (c - '0'); // 0~9 → 26~35
  return 36;                                       // 其他字符按空格处理
}

// ===== 点阵驱动 =====
// 将 16 字节字模写入点阵屏显存
void matrix_display(unsigned char matrix_value[]) {
  // 0x40：写数据模式，地址自增
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0：显存起始地址 0
  IIC_start();
  IIC_send(0xC0);

  // 连写 16 列
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8A：开显示，亮度 8/16
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// 起始条件：CLK 高电平期间 DIN 下降沿
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// 发送 1 字节，低位在前：CLK 低时置数据，上升沿采样
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);  // 拉低时钟，准备数据
    delayMicroseconds(3);

    // 从 bit0 起逐位发送
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // 拉高时钟，模块采样
    delayMicroseconds(3);

    send_data = send_data >> 1;  // 右移，取出下一位
  }
}

// 停止条件：CLK 高电平期间 DIN 上升沿
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
