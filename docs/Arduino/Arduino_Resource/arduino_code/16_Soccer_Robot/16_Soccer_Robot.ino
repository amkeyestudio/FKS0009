#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "webpage.h"  // 网页内容

// ===== Wi-Fi =====
// 仅支持 2.4 GHz
const char* WIFI_SSID = "你的WiFi名字";
const char* WIFI_PASS = "你的WiFi密码";

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 夹爪舵机 =====
#define CLAW_SERVO_PIN 11  // 夹爪舵机，同第 9 课

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 速度 =====
#define SPEED_RUN   180  // 行驶速度
#define SPEED_TURN  180  // 原地转向速度

// ===== 夹爪角度 =====
#define CLAW_OPEN   140  // 张开
#define CLAW_CLOSE   90  // 夹紧

// ===== 超时保护 =====
#define STOP_AFTER 1500  // 指令超时（ms）自动停车

// ===== 表情编号 =====
#define FACE_EYES  0  // 待机：眼睛
#define FACE_SMILE 1  // 进球：笑脸

// 16 字节对应 16 列，每字节 8 位对应一列 8 行

// 待机：两只方眼
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// 笑脸：进球
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

WebServer server(80);  // 网页服务器
Servo clawServo;       // 夹爪舵机

int score = 0;                      // 当前比分
unsigned long lastCmdTime = 0;      // 最后指令时刻
unsigned long smileUntil = 0;       // 笑脸结束时刻
int curFace = -1;                   // 当前表情，-1 表示未显示

void setup() {
  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // 上电显示待机表情

  // 电机引脚
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // 夹爪：50 Hz，脉宽 500~2500 µs
  ESP32PWM::allocateTimer(0);
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);
  clawServo.write(CLAW_OPEN);  // 上电张开
  delay(300);

  // 连接 Wi-Fi
  Serial.begin(115200);
  delay(300);
  Serial.print("正在连接 ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // 阻塞等待连接
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi 已连接");
  Serial.print("板子的 IP：");
  Serial.println(WiFi.localIP());  // 手机浏览器访问此地址

  // 首页
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // 指令入口，d 为动作名
  server.on("/cmd", []() {
    String d = server.arg("d");
    server.send(200, "text/plain", runCmd(d));  // 返回"比分,状态"，网页直接用
  });

  // 比分查询，网页每秒轮询
  server.on("/score", []() {
    server.send(200, "text/plain", String(score) + "," + faceName());
  });

  server.begin();
  Serial.println("足球车已就绪");
}

void loop() {
  server.handleClient();  // 必须持续调用

  // 超时停车
  if (millis() - lastCmdTime > STOP_AFTER) {
    stopCar();
  }

  // 笑脸 3 s 后恢复待机
  if (smileUntil != 0 && millis() > smileUntil) {
    smileUntil = 0;
    showFace(FACE_EYES);
  }
}

// ===== 指令分发 =====
// 返回 "比分,状态"，供网页同步刷新
String runCmd(String d) {
  lastCmdTime = millis();  // 刷新超时计时

  if (d == "f") {
    goForward(SPEED_RUN);
  } else if (d == "b") {
    goBackward(SPEED_RUN);
  } else if (d == "l") {
    spinLeft(SPEED_TURN);
  } else if (d == "r") {
    spinRight(SPEED_TURN);
  } else if (d == "carry") {
    clawServo.write(CLAW_OPEN);   // 带球：张开
  } else if (d == "shoot") {
    clawServo.write(CLAW_CLOSE);  // 射门：闭合
  } else if (d == "goal") {
    score++;                        // 手动记分
    celebrate();                    // 播放庆祝表情
  } else if (d == "reset") {
    score = 0;                      // 清零
  } else {
    stopCar();                      // 其余指令一律停车
  }

  return String(score) + "," + faceName();
}

// ===== 夹爪用法 =====
// 两个状态对应两个动作：
//   张开 140°：球兜在爪内，行进时带球
//   闭合 90°：爪臂收拢挤出，即射门

// ===== 进球庆祝 =====
void celebrate() {
  showFace(FACE_SMILE);        // 切笑脸
  smileUntil = millis() + 3000;  // 3 s 后恢复
}

// 当前表情名，供网页显示
String faceName() {
  if (curFace == FACE_SMILE) return "进球！";
  return "待命";
}

// ===== 点阵显示 =====
// 仅表情变化时写屏
void showFace(int face) {
  if (face == curFace) return;  // 表情未变
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);   // 待机
  } else {
    matrix_display(smile);  // 笑脸
  }
}

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

  // 0x8A：开显示，亮度最大
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

// ===== 电机驱动 =====
// A 路接左轮、B 路接右轮（与第 7 课一致）
// leftSpeed 写 B 路，rightSpeed 写 A 路
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // 右轮限幅
  rightSpeed = constrain(rightSpeed, -255, 255);  // 左轮限幅

  // A 路：左轮
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // 拉高：正转
    ledcWrite(MOTOR_BEN, rightSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // 拉低：反转
    ledcWrite(MOTOR_BEN, -rightSpeed); // 取绝对值
  }

  // B 路：右轮
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // 拉高：正转
    ledcWrite(MOTOR_AEN, leftSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // 拉低：反转
    ledcWrite(MOTOR_AEN, -leftSpeed); // 取绝对值
  }
}

// 基本动作
void goForward(int speed)  { driveMotors(speed, speed); }    // 前进：左右同速正转
void goBackward(int speed) { driveMotors(-speed, -speed); }  // 后退：左右同速反转
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // 左转：左后退右前进
void spinRight(int speed)  { driveMotors(speed, -speed); }   // 右转：左前进右后退
void stopCar()             { driveMotors(0, 0); }            // 停止：占空比归零
