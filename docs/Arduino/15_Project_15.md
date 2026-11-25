### 项目 15：Wi-Fi 全功能车

#### **1. 简介：**

前面十四课的代码，到这里合成一份：手动遥控、循迹、避障、跟随，四种模式在同一个网页上切换，点阵跟着换表情，云台和夹爪随时能调。六百多行，全书最长的一份。

四套功能单独跑都没问题，合到一起第一件事是抢线：点阵占着 IO8、IO9，循迹传感器的总线也是这两根。于是改成按模式分时占用——循迹时把线交给传感器、点阵让开，其余模式反过来。

第二件事是等待。避障要转舵机、停 300 毫秒、再测距，这段时间网页会僵住；改成把避障拆成五步，每步只做一件事，靠计时往前推，谁也不用等谁。模式切换也各自带一段收尾：先停车、云台回中、清掉上一段的记忆，再进入新模式。

手机页面顶部四个模式按钮。手动模式点阵笑脸，循迹关屏，避障感叹号，跟随爱心；切换的一瞬间车会先停一下、云台回正，然后按新模式动起来。

#### **2. 所需清单：**

本课用到的器件：

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> | ![](../media/kit_02.png){width=170px}<br><span style="font-size:0.85em">履带底盘总成（含循迹传感器） ×1</span> |
| ![](../media/kit_05.png){width=170px}<br><span style="font-size:0.85em">超声波模块（带外壳） ×1</span> | ![](../media/kit_06.png){width=170px}<br><span style="font-size:0.85em">8×16 LED 点阵屏 ×1</span> | ![](../media/kit_13.png){width=170px}<br><span style="font-size:0.85em">9G 舵机（180°） ×2</span> |
| <span style="font-size:0.85em">2.4 GHz 无线路由器或手机热点 ×1</span> |  |  |
> ⚠️ **全部器件都在第 2 章装好，本表只作对照，不用拆；下面的接线说明只标清接线方式，供对照，不必拆装。完整物料见 1.4 套件清单。**
#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 工作模式 | manual / line / avoid / follow |
| 手动超时 | 1500 ms |
| 循迹直行速度 | 115 |
| 循迹急转 | 内侧反转 95、外侧 125 |
| 跟随区间 | 30～50 cm 前进，小于 10 cm 后退 |
| 避障阈值 | 小于 15 cm 视为障碍 |
| 避障状态 | AV_GO → AV_LEFT → AV_RIGHT → AV_TURN → AV_TURNING |
| 云台步进 | 15° / 次 |
| 夹爪角度 | 张开 140°、夹紧 90° |
| 点阵与循迹 | 共用 IO8 / IO9，按模式独占 |

#### **4. 接线说明：**

| 模块 | 引脚 | 主控 GPIO |
| --- | --- | :---: |
| 左轮电机 | AIN / AEN | IO38 / IO21 |
| 右轮电机 | BIN / BEN | IO40 / IO41 |
| 超声波 | Trig / Echo | IO13 / IO12 |
| 点阵 | CLK / DIN | IO9 / IO8 |
| 循迹 I2C | SCL / SDA | IO9 / IO8 |
| 云台舵机 | 信号 | IO42 |
| 夹爪舵机 | 信号 | IO11 |

> **注意**：接线图中的元器件只用于说明接线关系，实际接线时不需要把清单中带外壳的元器件拆开。

注意 **IO8 / IO9 出现两次**，这不是笔误：点阵是软件模拟时序的**假 I2C**，循迹是真的硬件 I2C，两者抢同一对引脚，靠代码分时切换。

![](media/PRO15.png)

#### **5. 流程图：**

主循环按模式分发，四种模式谁在跑由 `carMode` 决定：

![](media/multi_mode_flow.png)

#### **6. 测试代码：**

> **上传前先改一处**：把下面 `ssid`、`password` 换成自己的 Wi-Fi 名称和密码，不改连不上。
>
> 另外两点要同时满足：Wi-Fi 必须是 **2.4 GHz**（板子不支持 5 GHz），手机和车要在**同一个局域网**——否则浏览器打不开板子的 IP。

> **本课工程有两个文件**：`15_WiFi_MultiFunction.ino` 和 `webpage.h`。网页的 HTML 全部放在 `webpage.h`，`.ino` 里只留 Arduino 逻辑；两个文件放在同一个文件夹，少一个编译不过。

```cpp
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
```

#### **7. 代码解释：**

**(1) 全课最难的一处：IO8 / IO9 要分时复用**

点阵是软件模拟 I2C，占着 IO8（DIN）和 IO9（CLK）；循迹传感器是硬件 I2C，也占 IO8（SDA）和 IO9（SCL）。同一组引脚，两套协议，**不能同时开**。

```cpp
#define CLK_Pin 9   // 时钟，接点阵 CLK
#define DIN_Pin 8   // 数据，接点阵 DIN
```

这两个宏和第 1 课一样，但这课它们和 `Wire` 抢同一组脚。理解这一点，后面所有 `Wire.end()` / `Wire.begin()` 才有意义——不是"重新初始化"，是**把引脚从硬件 I2C 手里抢回来当普通 GPIO 用，用完再还回去**。

**(2) `matrix_show()`：刷屏前先关硬件 I2C**

```cpp
void matrix_show(unsigned char matrix_value[]) {
  Wire.end();                     // 释放 io8/io9 的 I2C 占用
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  matrix_write(matrix_value);     // 写屏
}
```

`Wire.end()` 把硬件 I2C 外设关掉，引脚退回普通 GPIO 状态；接着重新 `pinMode` 成输出并拉高，总线回到空闲态，然后才走点阵自己的软件时序。

少 `Wire.end()` 的后果：`digitalWrite` 改了引脚电平，但 I2C 外设还挂在那两根线上，电平会被它拽回去，屏上显示乱码或者干脆不亮。

注意函数名后面没有交还 I2C 的动作——刷完屏引脚仍归点阵，要等下一次循迹读取前才还（见下一节）。

**(3) `showModeIcon()`：循迹模式要关屏并交还总线**

```cpp
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
```

关屏动作本身要用点阵的软件时序，所以先把 I2C 停掉；关完立刻 `Wire.begin()` 把引脚还给循迹，再重新设 400 kHz、重新初始化 PCF8574，然后 `return`——循迹模式不显示任何图案。

`0x88` 是"关显示"命令（在 `matrix_off()` 里），屏上内容还在显存里，只是不亮。下次切回别的模式，`0x8A` 一开就恢复。

**(4) `setMode()`：换模式先清残留**

```cpp
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
```

第一行先校验 `m` 是不是四个合法模式之一，不合法的字符串直接丢弃，`carMode` 不会被改成乱七八糟的值。

换模式时上一模式可能正处在任意中间状态：还在往前走、云台歪在 180°、丢线计时已经走了 1 秒。所以四件事一起做——停车、避障状态机回第一步、清掉循迹丢线计时、云台回正。

`lastCmdTime = millis()` 也在里面：刚切完模式就判超时，手动模式会立刻停车，看起来像"按了没反应"。

**(5) `changed` 挡住的是反复刷屏**

```cpp
    if (changed) showModeIcon();
  }
}
```

网页每发一次模式请求都会调 `setMode()`，哪怕点的还是当前模式。不判 `changed`，连点十次"循迹"就会刷十次屏，每次都要 `Wire.end()` / `Wire.begin()` 折腾一遍 I2C 总线，循迹采样被打断，车在原地抖。

**(6) 四种模式共用一个 `loop()`，靠 `static` 计时不靠 `delay`**

```cpp
void runLine() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 5) return;  // 5 ms 读一次
  lastRead = millis();
```

`static` 让 `lastRead` 在两次调用之间保住值——普通局部变量每次进函数都重新初始化成 0，起不到"记住上次是什么时候跑的"作用。

这两行是四种模式通用的节流写法：进来先看距离上次执行够不够久，不够就直接 `return`，一个字节的 I2C 都不读。

为什么不能像第 7 课那样写 `delay(5)`：`loop()` 里还有 `server.handleClient()`，`delay()` 一卡，网页就没响应了。四个模式函数轮流被调用，任何一个里出现 `delay()`，其他三个和网页处理全被拖住。

三个模式的节拍各不相同，都写在各自函数的头两行：

| 模式 | 判据 | 周期 |
| --- | --- | --- |
| `runLine()` | `millis() - lastRead < 5` | 5 ms（200 Hz） |
| `runFollow()` | `millis() - lastCheck < 100` | 100 ms（10 Hz） |
| `runAvoidance()` | 每个状态各带一个 `avTime` | 300 / 100 / 500 ms |

循迹要快，慢了就冲出线；跟随和人脸一样 10 Hz 足够；避障要等舵机转到位，最慢。三种节拍能同时存在，靠的就是"谁进来谁自己看表，不够时间就退出去"，而不是在哪个函数里睡一觉。

**(7) 避障从"等"改成了状态机**

第 4 课的避障是一路 `delay(300)` 串下来的，整整 1 秒多里程序什么都不干。这课要同时伺候网页请求，不能再这么写：

```cpp
    case AV_LEFT:
      if (millis() - avTime >= 300) { // 等舵机到位再测
        leftDistance = getDistance();
        panAngle = 0;
        myPan.write(panAngle);        // 云台右转
        avTime = millis();
        avStep = AV_RIGHT;
      }
      break;
```

每个状态只做一件事、立刻返回。等到时间了才进下一个状态，中间这段时间 `loop()` 已经转了几百轮，`handleClient()` 一直在跑。

**(8) 五个状态串成一轮**

```cpp
    case AV_TURN:
      if (millis() - avTime >= 100) {
        if (leftDistance > rightDistance) left(255, 255);   // 向较空一侧转向
        else                              right(255, 255);
        avTime = millis();
        avStep = AV_TURNING;
      }
      break;
```

`AV_GO`（前进 + 测前方）→ `AV_LEFT`（转头 180°、等 300 ms、测一次）→ `AV_RIGHT`（转头 0°、等 300 ms、测一次）→ `AV_TURN`（回正、等 100 ms、选边转）→ `AV_TURNING`（转 500 ms 后停）。

五个状态走完一轮约 1.2 秒，这 1.2 秒被切成五段，每段之间 `loop()` 都能去处理网页请求，所以网页全程不卡。这就是状态机换掉 `delay` 的全部收益。

**(9) 行驶指令要先切回手动，而且顺序不能反**

```cpp
    if (carMode != "manual") {
      setMode("manual");
    }
    lastCmdTime = millis();

    if (moveCommand == "forward")       forward(180, 180);
```

在循迹模式按"前进"，得先退出循迹，否则 `loop()` 每轮都被 `runLine()` 抢走，手动指令下一轮就被覆盖。

**先切模式、再发动作**，这个顺序不能换。`setMode()` 里有一句 `stopMotor()`，如果先 `forward()` 再 `setMode()`，车刚动起来立刻被这一句停掉，表现是"按了前进只抖一下"。

云台和夹爪那四个分支排在前面，不碰模式——不管当前在循迹还是避障，都能直接扭头、开合爪子。

**(10) `loop()`：一个入口，四条分支**

```cpp
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
```

`handleClient()` 永远排第一，任何模式下网页都能响应。四条分支里只有一条会跑，`carMode` 是唯一的开关。

四种模式的节奏差得很远：手动模式没有周期，完全由网页指令驱动，超过 `MANUAL_TIMEOUT`（1500 ms）没新指令就停车；循迹 5 ms 一轮（200 Hz），必须快；跟随 100 ms 一轮（10 Hz），和第 5 课一致；避障靠 `avTime` 分段，最慢，一轮约 1.2 秒。

整课从头到尾没有一句 `delay()`——不是不想用，是用不得。`delay()` 会把整个 `loop()` 卡住，四条模式共用这一个 `loop()`，任何一条里出现长 `delay()`，网页响应就会跟着卡死。

**(11) 两套电机函数：`driveLine()` 和 `setMotor()`**

```cpp
void setMotor(int leftSpeed, int rightSpeed) {
  ledcWrite(MOTOR_AEN, leftSpeed);    // B 路：右轮
  ledcWrite(MOTOR_BEN, rightSpeed);   // A 路：左轮
}
```

`setMotor()` 只写两个 PWM 值，不碰方向脚，所以参数必须是正数。方向由上层决定：`forward()` 把两颗方向脚拉高、`back()` 拉低、`left()` / `right()` 一高一低。这套是第 5 课的写法。

函数里那两行注释的位置是反的：`ledcWrite(MOTOR_AEN, ...)` 写的是 A 路左轮，注释却标"B 路：右轮"。以 `MOTOR_AEN` / `MOTOR_BEN` 这两个名字为准，和第 11 课那次一样，注释不可靠。

循迹模式用的是另一套：

```cpp
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
```

参数带符号，函数自己判正负、自己拉方向脚——这是第 7 课的写法。

两套并存不是没整理干净，是需求不同：循迹要"一侧反转原地拧"（`driveLine(-INNER_SPEED, SPIN_SPEED)`），必须能收负数；手动模式只有四个固定方向，用 `setMotor` 配 `forward` 那几个包装更直白。

代价是参数顺序容易混：两套都是左轮在前，但 `setMotor` 只认正数、`driveLine` 认符号。改代码时先看清调的是哪一个。

#### **8. 运行现象：**

串口打印：

```text
正在连接 你的WiFi名字
.....
WiFi 已连接
板子的 IP：192.168.1.23
网页服务已启动，默认手动模式
```

手机页面顶部是四个模式按钮，下面是方向键、云台、夹爪按钮。

| 模式 | 点阵 | 行为 |
| --- | --- | --- |
| 手动 | 笑脸 | 按方向键走，松手 1.5 s 后停 |
| 循迹 | 关屏 | 自己沿线走，网页仍可切模式 |
| 避障 | 感叹号 | 遇障停下，左右张望后绕开 |
| 跟随 | 爱心 | 手在前方 30～50 cm 跟着走 |

四种模式都能随时切换，切换瞬间车会先停一下（`setMode()` 里的 `stopMotor()`），云台回中，然后进入新模式。

> 【图片待补】手机页面切换四种模式时的四张截图拼图。

#### **9. 常见问题解决：**

**Wi-Fi 连不上、网页打不开，按这个顺序查：**

(1) **账号密码换了没有**：代码开头的 `WIFI_SSID`、`WIFI_PASS` 还是占位符就烧进去，一定连不上，改成自己路由器的名称和密码。

(2) **串口一行字都没有**：按一下板子上的 RESET 键再试，可能是上一版程序卡住了。

![](media/RESET.png){width=340px}

(3) **串口一直冒点、拿不到 IP**：Wi-Fi 连接失败。路由器必须是 **2.4 GHz**，5 GHz 板子不支持；没有 2.4 G 就开手机热点（热点默认 2.4 G）。

(4) **拿到 IP 但页面打不开**：地址要带 `http://`，IP 以串口打印的为准；手机必须连同一个 Wi-Fi，两边 IP 前三段要一样。

(5) **网段对、还是打不开**：进路由器后台把**AP 隔离**关掉，开了它同一 Wi-Fi 下的设备互相访问不了。

(6) 切到循迹后点阵不亮是正常的——那是为了把 IO8 / IO9 让给传感器，是有意关的，不是坏了。切回手动它会自己亮回来。

(7) 如果切到循迹后车不动、串口也没有报错，先确认循迹 4P 头插紧了没，再确认车是不是压在线外（五路全 0 且没有历史方向时，代码会停车）。
