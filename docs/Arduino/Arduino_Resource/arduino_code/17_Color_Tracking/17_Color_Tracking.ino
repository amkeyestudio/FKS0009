#include <ESP32Servo.h>
#include "AiCam.h"  // 视觉模块驱动

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 舵机引脚 =====
#define CLAW_SERVO_PIN 11  // 夹爪舵机，第 9 课
#define PAN_SERVO_PIN  42  // 云台舵机，第 8 课

// ===== 视觉模块串口 =====
// 主控 RX←模块 TX，主控 TX→模块 RX，需共地
#define CAM_RX 18  // 主控接收脚
#define CAM_TX 17  // 主控发送脚

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 速度 =====
#define SPEED_RUN      180  // 直线运送速度
#define SPEED_TURN     180  // 原地转向速度
#define SPEED_APPROACH 150  // 接近目标速度（低速）

// ===== 角度 =====
#define CLAW_OPEN   140  // 张开
#define CLAW_CLOSE   90  // 夹紧
#define PAN_CENTER   90  // 云台居中

// ===== 动作时长（ms，按场地与电压微调）=====
#define T_APPROACH 350  // 接近目标时长
#define T_TURN     600  // 原地转 90° 时长
#define T_DELIVER  800  // 运送时长

// ===== 表情编号 =====
#define FACE_EYES   0  // 眼睛：待命
#define FACE_SMILE  1  // 笑脸：识别到目标或完成投放
#define FACE_HEART  2  // 爱心：已夹取

// 待机：两只方眼，第 4/5、10/11 列亮第 2~4 行
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// 笑脸：16 字节对应 16 列，每字节 8 位对应一列 8 行
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// 爱心：格式同笑脸
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
  0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // 视觉模块实例
Servo clawServo;              // 夹爪舵机
Servo panServo;               // 云台舵机

int curFace = -1;  // 当前表情，-1 表示未显示

void setup() {
  Serial.begin(115200);

  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  // 两个舵机各占一个定时器，避免与电机 PWM 冲突
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  // 云台：50 Hz，脉宽 500~2500 µs
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);

  // 夹爪：50 Hz，脉宽 500~2500 µs
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);

  // 摄像头随云台，回中朝正前
  panServo.write(PAN_CENTER);
  // 夹爪张开待命
  clawServo.write(CLAW_OPEN);

  // 电机引脚
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // 启动视觉模块，颜色识别模式
  aiCam.begin();
  aiCam.setMode("color");

  // 显示待机表情
  showFace(FACE_EYES);

  Serial.println("颜色追踪车已就绪，把红色或黄色物块放到车前");
  delay(1000);  // 等舵机到位与模块切换完成
}

void loop() {
  aiCam.readSerial();  // 每轮读取串口

  String color = aiCam.getColor();  // 当前识别颜色

  // 无有效目标或非红黄：停车等待
  if (!aiCam.isColorValid() || (color != "RED" && color != "YELLOW")) {
    stopCar();
    showFace(FACE_EYES);  // 待机表情
    return;
  }

  Serial.print("识别到目标：");
  Serial.println(color);

  showFace(FACE_SMILE);  // 显示笑脸
  aiCam.clearColor();    // 清缓存，避免重复触发

  pickAndDeliver(color);  // 执行抓取投放流程

  aiCam.flushInput();   // 丢弃动作期间积压数据
  aiCam.clearColor();   // 再清缓存
  showFace(FACE_EYES);  // 回到待机
  delay(2000);          // 冷却 2 s 再找下一目标
}

// ===== 抓取与投放 =====
// 去程：靠近 → 夹取 → 转向 → 运送 → 放下
// 回程按相反顺序退回起点
void pickAndDeliver(String color) {
  // 去程
  // 1. 低速靠近
  Serial.println("1 靠近");
  goForward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();

  // 2. 夹取
  Serial.println("2 夹取");
  clawServo.write(CLAW_CLOSE);
  delay(400);           // 等舵机到位
  showFace(FACE_HEART); // 显示爱心

  // 3. 按颜色转向：红左、黄右
  Serial.println("3 转向投放区");
  if (color == "RED") {
    spinLeft(SPEED_TURN);  // 红色左转
  } else {
    spinRight(SPEED_TURN);  // 黄色右转
  }
  delay(T_TURN);
  stopCar();

  // 4. 运送
  Serial.println("4 运送");
  goForward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();

  // 5. 松爪卸货
  Serial.println("5 放下");
  clawServo.write(CLAW_OPEN);
  delay(300);

  // 回程
  // 6. 倒回转向点
  Serial.println("6 后退到转向点");
  goBackward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();

  // 7. 回正
  Serial.println("7 回正");
  if (color == "RED") {
    spinRight(SPEED_TURN);  // 反向转回
  } else {
    spinLeft(SPEED_TURN);  // 反向转回
  }
  delay(T_TURN);
  stopCar();

  // 8. 倒回起点
  Serial.println("8 后退回起点");
  goBackward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();

  showFace(FACE_SMILE);  // 显示笑脸
  Serial.println("投放完成，已回到起点");
}

// ===== 点阵显示 =====
// 仅表情变化时写屏
void showFace(int face) {
  if (face == curFace) return;  // 表情未变
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);   // 待命
  } else if (face == FACE_SMILE) {
    matrix_display(smile);  // 笑脸
  } else {
    matrix_display(heart);  // 爱心
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
