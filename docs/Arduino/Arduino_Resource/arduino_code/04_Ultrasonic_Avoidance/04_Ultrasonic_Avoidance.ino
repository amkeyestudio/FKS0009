#include <ESP32Servo.h>  // 舵机库

Servo myServo;  // 云台舵机

// ===== 点阵屏引脚 =====
#define CLK_Pin 9   // 时钟，接点阵 CLK
#define DIN_Pin 8   // 数据，接点阵 DIN

// ===== 超声波引脚 =====
const int trigPin = 13;  // 触发，IO13
const int echoPin = 12;  // 回响，IO12

// 舵机信号，GPIO42
const int servoPin = 42;

// ===== 距离阈值 =====
// 小于 15 cm 视为障碍，停车后左右测距
const long closeDistance = 15;

long rightDistance = 0;  // 右侧距离
long leftDistance = 0;   // 左侧距离

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // 左电机方向
#define MOTOR_AEN 21  // 左电机调速
#define MOTOR_BIN 40  // 右电机方向
#define MOTOR_BEN 41  // 右电机调速

// ===== 字模（16 列 × 8 行）=====
// 爱心：直行
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// 感叹号：前方有障碍
unsigned char alert_sign[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f,
  0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 向左看：左转测距
unsigned char look_left[] = {
  0x00, 0x00, 0x3c, 0x4e, 0x4e, 0x42, 0x3c, 0x00,
  0x00, 0x3c, 0x4e, 0x4e, 0x42, 0x3c, 0x00, 0x00
};

// 向右看：右转测距
unsigned char look_right[] = {
  0x00, 0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x3c, 0x00,
  0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x3c, 0x00, 0x00
};

// 向下箭头：后退
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7e, 0xfe,
  0xfe, 0x7e, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// 向左箭头：左转
unsigned char arrow_left[] = {
  0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0xff, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00
};

// 向右箭头：右转
unsigned char arrow_right[] = {
  0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Serial.println("超声波避障启动");

  // 点阵引脚初始化并清屏
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  pinMode(trigPin, OUTPUT);  // Trig 输出
  pinMode(echoPin, INPUT);   // Echo 输入

  // 方向脚输出
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);

  // 调速脚 PWM：1 kHz，8 位
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  // 上电停车
  stopCar();

  // 舵机独用定时器，避免与电机 PWM 冲突
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);  // 舵机 50 Hz
  // 脉宽 500~2400 µs 对应 0~180°（SG90 类）
  // 换 MG995 等大舵机时可直接 attach(servoPin)
  myServo.attach(servoPin, 500, 2400);

  // 回中位并显示待机图案
  myServo.write(90);
  matrix_display(heart);
  delay(1000);
}

void loop() {
  // 测前方距离
  long distance = getDistance();

  Serial.print("前方距离：");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance <= closeDistance) {
    // 有障碍：停车并显示感叹号
    stopCar();
    matrix_display(alert_sign);
    delay(150);

    // 云台转向一侧，测这一侧距离
    matrix_display(look_left);
    myServo.write(180);            // 云台转到 180°
    delay(300);                    // 等舵机到位
    rightDistance = getDistance();
    delay(100);

    // 云台转向另一侧，测另一侧距离
    matrix_display(look_right);
    myServo.write(0);              // 云台转到 0°
    delay(300);
    leftDistance = getDistance();
    delay(100);

    myServo.write(90);             // 回正前方

    // 向空间较大的一侧转向
    if (rightDistance > leftDistance) {
      spinRight(255);
    } else {
      spinLeft(255);
    }
    delay(500);

  } else {
    // 无障碍，继续直行
    goForward(150);
  }

  delay(100);
}

// 测距，返回厘米
int getDistance() {
  long duration;  // 往返时间（µs）
  int distance;   // 距离（cm）

  // 先拉低，保证起始电平干净
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // 10 µs 高脉冲触发发波
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Echo 高电平宽度即往返时间
  duration = pulseIn(echoPin, HIGH);

  // 声速 0.034 cm/µs，除以 2 去掉回程
  distance = duration * 0.034 / 2;

  // 超出 2~300 cm 置 0
  if (distance < 2 || distance > 300) {
    distance = 0;
  }

  return distance;
}

// ===== 电机控制 =====
// 速度带符号：正转 / 反转 / 0 停
void driveMotors(int rightSpeed, int leftSpeed) {
  rightSpeed  = constrain(rightSpeed,  -255, 255);  // 限幅 ±255
  leftSpeed = constrain(leftSpeed, -255, 255);

  // 右轮（B 路）
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);  // 方向由方向脚决定，此处只给占空比
  }

  // 左轮（A 路）
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

// 动作与点阵图案绑定
void goForward(int speed)  {
  matrix_display(heart);        // 前进：爱心
  driveMotors(speed, speed);
}

void goBackward(int speed) {
  matrix_display(arrow_down);   // 后退：向下箭头
  driveMotors(-speed, -speed);
}

void spinRight(int speed)  {
  matrix_display(arrow_left);   // 原地右转，点阵显示左箭头
  driveMotors(-speed, speed);
}

void spinLeft(int speed)   {
  matrix_display(arrow_right);  // 原地左转，点阵显示右箭头
  driveMotors(speed, -speed);
}

void stopCar() {
  driveMotors(0, 0);            // 停：占空比归零
}

// ===== 点阵驱动（软件模拟 I2C）=====
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
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(3);

    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(3);

    send_data = send_data >> 1;
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