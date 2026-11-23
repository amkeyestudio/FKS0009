#include <ESP32Servo.h>
#include "AiCam.h"  // 视觉模块驱动

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 云台舵机 =====
#define PAN_SERVO_PIN 42  // 第 8 课

// ===== 视觉模块串口 =====
// 主控 RX←模块 TX，主控 TX→模块 RX，需共地
#define CAM_RX 18  // 主控接收脚
#define CAM_TX 17  // 主控发送脚

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 跟随参数 =====
#define PAN_CENTER     90  // 云台固定朝正前
#define FOLLOW_SPEED  100  // 跟随前进速度
#define KP            0.5  // 比例系数
#define TURN_MAX       80  // 单侧修正上限
#define DEAD_ZONE       8  // 对准死区
#define CALI_FRAMES    20  // 标定帧数
#define LOST_STOP     300  // 丢失超时（ms）

// ===== 调试开关 =====
// 1 打印标定中心值，0 静音
#define DEBUG_OUT 1

// ===== 表情编号 =====
#define FACE_EYES  0  // 没看到人：眼睛
#define FACE_SMILE 1  // 看到人：笑脸

// 16 字节对应 16 列，每字节 8 位对应一列 8 行

// 待机：两只方眼
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// 笑脸：看到人
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // 视觉模块实例
Servo panServo;               // 云台舵机

int centerX = -1;             // 标定中心 X，-1 未完成
long sumX = 0;                // 标定累加值
int caliCount = 0;            // 标定帧计数
unsigned long lastSeen = 0;   // 最后检测到人
int curFace = -1;             // 当前表情，-1 未显示

void setup() {
#if DEBUG_OUT
  Serial.begin(115200);
#endif

  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // 显示待机表情

  // 云台：50 Hz，脉宽 500~2500 µs
  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(PAN_CENTER);  // 朝正前，本课不再转动
  delay(500);

  // 电机引脚
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // 启动视觉模块，人脸检测模式
  aiCam.begin();
  aiCam.setMode("face");

  delay(500);  // 等模式切换完成
}

void loop() {
  aiCam.readSerial();  // 每轮读取串口

  if (!aiCam.isFaceValid()) {
    // 未检测到人：超时停车
    showFace(FACE_EYES);
    if (millis() - lastSeen > LOST_STOP) {
      stopCar();  // 超时停车
    }
    delay(20);
    return;
  }

  // 检测到人
  lastSeen = millis();   // 刷新检测时刻
  showFace(FACE_SMILE);  // 显示笑脸

  int x = aiCam.getFaceX();  // 人脸中心 X

  // 标定正前方
  // 模块 X 量程未知（0~100 或 0~320 像素），
  // 改用前 20 帧取平均，不写死中心值
  if (centerX < 0) {
    sumX += x;
    caliCount++;
    stopCar();  // 标定期间停车

    if (caliCount >= CALI_FRAMES) {
      centerX = (int)(sumX / caliCount);  // 均值为正前方 X
#if DEBUG_OUT
      Serial.print("画面中心已锁定：");
      Serial.println(centerX);
#endif
    }

    delay(20);
    return;
  }

  // 跟随
  int err = x - centerX;  // 偏差：正在右，负在左

  if (abs(err) <= DEAD_ZONE) {
    // 已对准：直行靠近
    goForward(FOLLOW_SPEED);
  } else {
    // 未对准：差速转向
    // err 为正（人在右）时左快右慢
    int correction = constrain((int)(err * KP), -TURN_MAX, TURN_MAX);
    driveMotors(FOLLOW_SPEED + correction, FOLLOW_SPEED - correction);
  }

  delay(20);  // 20 ms 一轮
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
// leftSpeed 走 B 路，rightSpeed 走 A 路
// 先限幅 ±255，再取绝对值给占空比
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
