#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <PCF8574.h>
#include "webpage.h"  // 网页内容

// ===== Wi-Fi =====
// 仅支持 2.4 GHz
const char* ssid = "你的WiFi名字";
const char* password = "你的WiFi密码";

// ===== 全局对象 =====
WebServer server(80);  // 网页服务器
Servo myClaw;          // 夹爪舵机
Servo myPan;           // 云台舵机

// ===== 电机引脚 =====
// A 路接左轮，B 路接右轮（上车实测）
// A/B 是驱动板通道号，不是左右
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 点阵引脚 =====
// 8×16 点阵，软件模拟时序，无设备地址
// 与循迹模块 SDA/SCL 共用 io8/io9
// 仅在切换模式时刷屏，避免干扰循迹总线
#define CLK_Pin 9   // 时钟，接点阵 CLK
#define DIN_Pin 8   // 数据，接点阵 DIN

// ===== 超声波引脚 =====
const int trigPin = 13;  // 触发
const int echoPin = 12;  // 回响

// ===== 舵机 =====
const int PAN_PIN  = 42;           // 云台舵机
const int CLAW_PIN = 11;           // 夹爪舵机
const int CLAW_OPEN_ANGLE  = 140;  // 张开
const int CLAW_CLOSE_ANGLE = 90;   // 夹紧
const int SERVO_STEP = 15;         // 云台单次步进角度
int servoAngle = CLAW_CLOSE_ANGLE; // 夹爪当前角度，上电夹紧
int panAngle = 90;                 // 云台当前角度，上电朝正前
                                   // 0° 指右，180° 指左

// ===== 循迹模块（PCF8574）=====
PCF8574 pcf8574(0x20);
#define OUTA P4  // 最左
#define OUTB P0  // 左
#define OUTC P1  // 中
#define OUTD P2  // 右
#define OUTE P3  // 最右

const int BASE_SPEED = 115;  // 循迹直行速度
const int SPIN_SPEED = 125;  // 急转外侧轮速度
const int INNER_SPEED = 95;  // 急转内侧轮（反转）
uint8_t lastDir = 0;         // 0 直行，1 线在左，2 线在右
unsigned long lostStartTime = 0;

// 点阵与循迹共用 io8/io9，按模式独占：
//   循迹模式 → 关屏，io8/io9 归 I2C
//   其余模式 → 释放 I2C，io8/io9 归点阵

// ===== 跟随参数 =====
const long safeDistance = 30;        // 30~50：跟随
const long closeDistanceFollow = 10; // 小于 10：后退
const long maxDistance = 50;         // 大于 50：目标丢失

// ===== 避障参数 =====
const long closeDistanceAvoid = 15;  // 小于 15：视为障碍
long leftDistance = 0;               // 左侧距离
long rightDistance = 0;              // 右侧距离

// ===== 工作模式 =====
String carMode = "manual";  // 模式：manual / line / avoid / follow

// 手动模式指令超时（ms）
#define MANUAL_TIMEOUT 1500
unsigned long lastCmdTime = 0;

// 避障状态机，避免阻塞网页
enum AvoidStep { AV_GO, AV_LEFT, AV_RIGHT, AV_TURN, AV_TURNING };
AvoidStep avStep = AV_GO;
unsigned long avTime = 0;

// ===== 点阵字模 =====
// 16 字节对应 16 列，每字节 8 位对应一列 8 行，1 亮 0 灭

// 笑脸：手动
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// 向下箭头：循迹备用
// 当前循迹模式关屏，暂未使用
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7E, 0xFE,
  0xFE, 0x7E, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// 感叹号：避障
unsigned char alert_sign[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F,
  0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 爱心：跟随
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
  0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
};

// ===== 点阵驱动（软件模拟时序）=====

// 起始条件：CLK 高时 DIN 拉低
void IIC_start() {
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(5);
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(5);
}

// 发送 1 字节，低位在前：CLK 低置数据，高电平采样
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(5);

    if (send_data & 0x01) digitalWrite(DIN_Pin, HIGH);
    else                  digitalWrite(DIN_Pin, LOW);
    delayMicroseconds(5);

    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(5);

    send_data = send_data >> 1;
  }
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(5);
}

// 结束条件：CLK 高时 DIN 拉高
void IIC_end() {
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(5);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(5);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(5);
}

// 写 16 字节显存并开屏
// 纯刷屏，不处理总线归属，约 3 ms
void matrix_write(unsigned char matrix_value[]) {
  // 0x40：写数据模式，地址自增
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0：从第 0 列起写
  IIC_start();
  IIC_send(0xC0);
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8F：开显示，最大亮度
  IIC_start();
  IIC_send(0x8F);
  IIC_end();
}

// 0x88：关显示，显存内容保留
void matrix_off() {
  IIC_start();
  IIC_send(0x88);
  IIC_end();
}

// 刷屏后引脚仍归点阵（不交还 I2C）
void matrix_show(unsigned char matrix_value[]) {
  Wire.end();                     // 释放 io8/io9 的 I2C 占用
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  matrix_write(matrix_value);     // 写屏
}

// 按模式处理点阵
// 循迹模式关屏并把 io8/io9 交给 I2C
void showModeIcon() {
  if (carMode == "line") {
    Wire.end();
    pinMode(CLK_Pin, OUTPUT);
    pinMode(DIN_Pin, OUTPUT);
    matrix_off();                 // 关显示

    Wire.begin();                 // io8/io9 交给循迹
    Wire.setClock(400000);
    pcf8574.begin();
    return;
  }

  // 其余模式不使用 I2C
  if (carMode == "manual")     matrix_show(smile);       // 手动：笑脸
  else if (carMode == "avoid") matrix_show(alert_sign);  // 避障：感叹号
  else                         matrix_show(heart);       // 跟随：爱心
}

// ===== 电机控制 =====

// 只写 PWM，不处理方向
// 形参为左右轮，内部映射到 B/A 路
void setMotor(int leftSpeed, int rightSpeed) {
  ledcWrite(MOTOR_AEN, leftSpeed);    // B 路：右轮
  ledcWrite(MOTOR_BEN, rightSpeed);   // A 路：左轮
}

// 速度带符号：正转 / 反转
// 形参为左右轮，内部映射到 B/A 路
// 循迹模式使用
void driveLine(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);
  }

  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

// 前进：方向脚拉高
void forward(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(leftSpeed, rightSpeed);
}

// 后退：方向脚拉低
void back(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(leftSpeed, rightSpeed);
}

// 左转：右轮正转，左轮反转
void left(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(leftSpeed, rightSpeed);
}

// 右转：右轮反转，左轮正转
void right(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(leftSpeed, rightSpeed);
}

// 停车
void stopMotor() {
  setMotor(0, 0);
}

// ===== 超声波测距 =====
int getDistance() {
  long duration;
  int distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 25 ms 超时，避免无回波时阻塞
  duration = pulseIn(echoPin, HIGH, 25000);
  distance = duration * 0.034 / 2;

  if (distance < 2 || distance > 300) {
    distance = 0;  // 超量程置无效
  }
  return distance;
}

// ===== 1. 循迹模式 =====
void runLine() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 5) return;  // 5 ms 读一次
  lastRead = millis();

  PCF8574::DigitalInput val = pcf8574.digitalReadAll();

  int A = !val.p4;
  int B = !val.p0;
  int C = !val.p1;
  int D = !val.p2;
  int E = !val.p3;

  uint8_t state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E;

  if (state != 0) {
    lostStartTime = 0;  // 压到线，清丢线计时
  }

  switch (state) {
    case 0b00100:
    case 0b01110:
    case 0b11111:
      driveLine(BASE_SPEED, BASE_SPEED);      // 居中直行
      lastDir = 0;
      break;

    case 0b01100:
      driveLine(85, BASE_SPEED);              // 偏左：右轮降速
      lastDir = 1;
      break;

    case 0b01000:
    case 0b11100:
      driveLine(45, SPIN_SPEED);              // 偏左较大
      lastDir = 1;
      break;

    case 0b11000:
    case 0b10000:
      driveLine(-INNER_SPEED, SPIN_SPEED);    // 严重偏左：原地左转
      lastDir = 1;
      break;

    case 0b00110:
      driveLine(BASE_SPEED, 85);              // 偏右：左轮降速
      lastDir = 2;
      break;

    case 0b00010:
    case 0b00111:
      driveLine(SPIN_SPEED, 45);              // 偏右较大
      lastDir = 2;
      break;

    case 0b00011:
    case 0b00001:
      driveLine(SPIN_SPEED, -INNER_SPEED);    // 严重偏右：原地右转
      lastDir = 2;
      break;

    case 0b00000:                             // 全部脱线
      if (lostStartTime == 0) {
        lostStartTime = millis();
      }
      if (millis() - lostStartTime < 1500) {
        if (lastDir == 1)      driveLine(-INNER_SPEED, SPIN_SPEED);
        else if (lastDir == 2) driveLine(SPIN_SPEED, -INNER_SPEED);
        else                   stopMotor();
      } else {
        stopMotor();                          // 超时未找到，停车
      }
      break;

    default:
      break;
  }
}

// ===== 2. 跟随模式 =====
void runFollow() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 100) return;
  lastCheck = millis();

  long d = getDistance();

  if (d > safeDistance && d <= maxDistance) {
    forward(150, 150);                        // 30~50 cm：前进
  } else if (d < closeDistanceFollow && d > 0) {
    back(105, 150);                           // 小于 10 cm：后退
  } else {
    stopMotor();                              // 其余情况停止
  }
}

// ===== 3. 避障模式 =====
// 状态机分步执行，不阻塞
void runAvoidance() {
  switch (avStep) {
    case AV_GO: {
      long d = getDistance();
      if (d > 0 && d <= closeDistanceAvoid) {
        stopMotor();
        panAngle = 180;
        myPan.write(panAngle);        // 云台左转
        avTime = millis();
        avStep = AV_LEFT;
      } else {
        forward(150, 150);            // 无障碍，前进
      }
      break;
    }

    case AV_LEFT:
      if (millis() - avTime >= 300) { // 等舵机到位再测
        leftDistance = getDistance();
        panAngle = 0;
        myPan.write(panAngle);        // 云台右转
        avTime = millis();
        avStep = AV_RIGHT;
      }
      break;

    case AV_RIGHT:
      if (millis() - avTime >= 300) {
        rightDistance = getDistance();
        panAngle = 90;
        myPan.write(panAngle);        // 云台回正
        avTime = millis();
        avStep = AV_TURN;
      }
      break;

    case AV_TURN:
      if (millis() - avTime >= 100) {
        if (leftDistance > rightDistance) left(255, 255);   // 向较空一侧转向
        else                              right(255, 255);
        avTime = millis();
        avStep = AV_TURNING;
      }
      break;

    case AV_TURNING:
      if (millis() - avTime >= 500) { // 转向 0.5 s 后重新探测
        stopMotor();
        avStep = AV_GO;
      }
      break;
  }
}

// ===== 模式切换 =====
void setMode(String m) {
  if (m == "manual" || m == "line" || m == "avoid" || m == "follow") {
    bool changed = (carMode != m);   // 是否真的切换
    carMode = m;
    stopMotor();      // 换模式先停车
    avStep = AV_GO;
    lostStartTime = 0;
    panAngle = 90;
    myPan.write(90);  // 云台回正
    lastCmdTime = millis();

    Serial.print("切换到模式：");
    Serial.println(carMode);

    // 仅在模式变化时刷屏
    // 避免反复占用循迹 I2C 总线
    if (changed) showModeIcon();
  }
}

// ===== 网页请求处理 =====
void handleCmd() {
  // 模式切换请求
  if (server.hasArg("mode")) {
    setMode(server.arg("mode"));
    server.send(200, "text/plain", "OK");
    return;
  }

  // 动作指令
  String moveCommand = server.arg("move");

  // 云台与夹爪：任何模式下可用
  if (moveCommand == "servo_plus") {
    panAngle = min(180, panAngle + SERVO_STEP);   // 左转
    myPan.write(panAngle);
  }
  else if (moveCommand == "servo_minus") {
    panAngle = max(0, panAngle - SERVO_STEP);     // 右转
    myPan.write(panAngle);
  }
  else if (moveCommand == "claw_open") {
    servoAngle = CLAW_OPEN_ANGLE;
    myClaw.write(servoAngle);
  }
  else if (moveCommand == "claw_close") {
    servoAngle = CLAW_CLOSE_ANGLE;
    myClaw.write(servoAngle);
  }
  else {
    // 行驶指令：先切回手动再执行
    // 顺序不可反，setMode 内含停车
    if (carMode != "manual") {
      setMode("manual");
    }
    lastCmdTime = millis();

    if (moveCommand == "forward")       forward(180, 180);
    else if (moveCommand == "backward") back(180, 180);
    else if (moveCommand == "left")     left(180, 180);
    else if (moveCommand == "right")    right(180, 180);
    else if (moveCommand == "stop")     stopMotor();
  }

  server.send(200, "text/plain", "OK");
}

// ===== 初始化 =====
void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. 点阵：两线置空闲后刷屏
  // 此时 I2C 未初始化，直接用 matrix_write
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  matrix_write(smile);

  // 2. 电机
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopMotor();

  // 3. 超声波
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // 4. 两个舵机
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  myPan.setPeriodHertz(50);
  myPan.attach(PAN_PIN, 500, 2500);
  myPan.write(90);

  myClaw.setPeriodHertz(50);
  myClaw.attach(CLAW_PIN, 500, 2500);
  myClaw.write(servoAngle);

  // 5. 循迹模块
  Wire.begin();
  Wire.setClock(400000);
  pcf8574.begin();
  pcf8574.pinMode(OUTA, INPUT);
  pcf8574.pinMode(OUTB, INPUT);
  pcf8574.pinMode(OUTC, INPUT);
  pcf8574.pinMode(OUTD, INPUT);
  pcf8574.pinMode(OUTE, INPUT);

  // 6. Wi-Fi
  Serial.print("正在连接 ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi 已连接");
  Serial.print("板子的 IP：");
  Serial.println(WiFi.localIP());

  // 7. 网页路由
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });
  server.on("/cmd", handleCmd);
  server.on("/distance", []() {
    server.send(200, "text/plain", String(getDistance()));
  });

  server.begin();
  Serial.println("网页服务已启动，默认手动模式");
}

// ===== 主循环 =====
void loop() {
  server.handleClient();  // 处理网页请求

  if (carMode == "line") {
    runLine();
  } else if (carMode == "avoid") {
    runAvoidance();
  } else if (carMode == "follow") {
    runFollow();
  } else {
    // 手动模式：超时停车
    if (millis() - lastCmdTime > MANUAL_TIMEOUT) {
      stopMotor();
    }
  }
}
