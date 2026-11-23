#include "AiCam.h"  // 视觉模块驱动

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 视觉模块串口 =====
// 主控 RX←模块 TX，主控 TX→模块 RX，需共地
// 按实际接线修改
#define CAM_RX 18  // 主控接收脚
#define CAM_TX 17  // 主控发送脚

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 循线参数 =====
#define BASE_SPEED   130  // 直行基础速度
#define KP           0.4  // 比例系数
#define KD           0.6  // 微分系数，抑制超调
#define TURN_MAX      90  // 单侧修正上限
#define D_MAX         50  // 微分项限幅

// ===== 丢线找线 =====
#define SEARCH_SPEED  90  // 找线转速
#define SEARCH_DEAD   15  // 找线死区
#define LOST_TIMEOUT 600  // 找线超时（ms）

// ===== 有效标志 =====
#define LINE_OK 5  // 模块第三字段为 5 表示识别到线

// ===== 表情编号 =====
#define FACE_EYES     0  // 待机：眼睛
#define FACE_SMILE    1  // 跟线：笑脸
#define FACE_SURPRISE 2  // 找线：瞪眼
#define FACE_SAD      3  // 超时：苦脸

// 16 字节对应 16 列，每字节 8 位对应一列 8 行

// 待机：两只方眼
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// 笑脸：跟线稳定
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// 瞪眼：正在找线
unsigned char surprise[] = {
  0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x00, 0xC0,
  0xC0, 0x00, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x00
};

// 苦脸：找线超时
unsigned char sad[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x80, 0x40,
  0x40, 0x80, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // 视觉模块实例

int lastError = 0;            // 上次偏差，丢线时决定找线方向
bool hasLast = false;         // 是否已有上次偏差
unsigned long lostStart = 0;  // 丢线起始时刻，0 表示未丢线
int curFace = -1;             // 当前表情，-1 未显示

void setup() {
  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  // 电机引脚
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();                       // 上电停车

  // 显示待机表情
  showFace(FACE_EYES);

  // 启动视觉模块，循线模式
  aiCam.begin();
  aiCam.setMode("line");

  delay(1000);  // 等模式切换完成
}

void loop() {
  aiCam.readSerial();  // 每轮读取串口

  int offset = aiCam.getLineOffset();  // 偏差：负偏左，正偏右
  int valid  = aiCam.getLineValid();   // 有效标志，5 为识别到线

  if (valid == LINE_OK) {
    // 识别到线：PD 换算差速
    lostStart = 0;         // 清丢线计时
    showFace(FACE_SMILE);  // 显示笑脸

    int error = offset;  // 当前偏差

    // 微分项：偏差变化量，抑制超调
    int dError = 0;
    if (hasLast) {
      dError = error - lastError;
      dError = constrain(dError, -D_MAX, D_MAX);  // 限幅
    }

    // 差速 = P×偏差 + D×偏差变化
    int correction = (int)(error * KP + dError * KD);

    // 修正量限幅
    correction = constrain(correction, -TURN_MAX, TURN_MAX);

    // 修正为正（偏右）时左快右慢，车头右偏
    // 修正为负时相反
    driveMotors(BASE_SPEED + correction, BASE_SPEED - correction);

    lastError = error;  // 保存本次偏差
    hasLast = true;
  } else {
    // 未识别到线：按上次偏差方向找线
    if (lostStart == 0) {    // 刚丢线
      lostStart = millis();  // 开始计时
    }

    if (millis() - lostStart < LOST_TIMEOUT) {
      showFace(FACE_SURPRISE);  // 找线中

      // 死区内方向不明，停车
      if (lastError > SEARCH_DEAD) {
        driveMotors(SEARCH_SPEED, -SEARCH_SPEED);   // 线原在右：向右找
      } else if (lastError < -SEARCH_DEAD) {
        driveMotors(-SEARCH_SPEED, SEARCH_SPEED);   // 线原在左：向左找
      } else {
        stopCar();                                  // 方向不明，停车
      }
    } else {
      showFace(FACE_SAD);  // 找线超时
      stopCar();           // 脱线停车
    }
  }

  delay(5);  // 5 ms 一轮
}

// ===== 点阵显示 =====
// 仅表情变化时写屏
void showFace(int face) {
  if (face == curFace) return;  // 表情未变
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);       // 待机
  } else if (face == FACE_SMILE) {
    matrix_display(smile);      // 笑脸
  } else if (face == FACE_SURPRISE) {
    matrix_display(surprise);   // 瞪眼
  } else {
    matrix_display(sad);        // 苦脸
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
// A 路(38/21)接左轮、B 路(40/41)接右轮
// leftSpeed 写 A 路、rightSpeed 写 B 路；先限幅 ±255，再取绝对值给占空比
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // 左轮限幅
  rightSpeed = constrain(rightSpeed, -255, 255);  // 右轮限幅

  // A 路：左轮
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // 拉高：正转
    ledcWrite(MOTOR_AEN, leftSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // 拉低：反转
    ledcWrite(MOTOR_AEN, -leftSpeed); // 取绝对值
  }

  // B 路：右轮
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // 拉高：正转
    ledcWrite(MOTOR_BEN, rightSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // 拉低：反转
    ledcWrite(MOTOR_BEN, -rightSpeed); // 取绝对值
  }
}

// 基本动作
void goForward(int speed)  { driveMotors(speed, speed); }    // 前进：左右同速正转
void goBackward(int speed) { driveMotors(-speed, -speed); }  // 后退：左右同速反转
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // 左转：左后退右前进
void spinRight(int speed)  { driveMotors(speed, -speed); }   // 右转：左前进右后退
void stopCar()             { driveMotors(0, 0); }            // 停止：占空比归零
