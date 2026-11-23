// ===== 点阵屏引脚 =====
#define CLK_Pin 9   // 时钟，接点阵 CLK
#define DIN_Pin 8   // 数据，接点阵 DIN

// ===== 超声波引脚 =====
const int trigPin = 13;  // 触发脚，接 IO13
const int echoPin = 12;  // 回响脚，接 IO12

// ===== 距离档位（cm）=====
// 跟随按三个阈值分档
const long safeDistance = 30;   // 30~50：跟随
const long closeDistance = 10;  // 小于 10：后退
const long maxDistance = 50;    // 大于 50：目标丢失

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // 左电机方向
#define MOTOR_AEN 21  // 左电机调速
#define MOTOR_BIN 40  // 右电机方向
#define MOTOR_BEN 41  // 右电机调速

// ===== 字模（16 列 × 8 行）=====
// 爱心：跟随前进
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// 笑脸：距离合适，停止
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// 向下箭头：太近后退
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7e, 0xfe,
  0xfe, 0x7e, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// 问号：目标丢失
unsigned char question_mark[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x51,
  0x59, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Serial.println("超声波跟随启动");

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

  // 上电停车并显示笑脸
  stopMotor();
  matrix_display(smile);
}

void loop() {
  // 测前方距离
  long distance = getDistance();

  Serial.print("当前距离：");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > safeDistance && distance <= maxDistance) {
    // 30~50 cm：跟随前进
    forward(150, 150);

  } else if (distance >= closeDistance && distance <= safeDistance) {
    // 10~30 cm：停止
    stopMotor();
    matrix_display(smile);

  } else if (distance < closeDistance && distance > 0) {
    // 小于 10 cm：后退
    // 左右差速，后退时车身略偏
    back(105, 150);

  } else {
    // 大于 50 cm 或读数无效：停车显示问号
    stopMotor();
    matrix_display(question_mark);
  }

  // 100 ms 测一次
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

// 直接写左右轮 PWM（0~255）
void setMotor(int rightSpeed, int leftSpeed) {
  ledcWrite(MOTOR_BEN, rightSpeed);
  ledcWrite(MOTOR_AEN, leftSpeed);
}

// 前进：方向脚拉高并显示爱心
void forward(int rightSpeed, int leftSpeed) {
  matrix_display(heart);
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(rightSpeed, leftSpeed);
}

// 后退：方向脚拉低并显示向下箭头
void back(int rightSpeed, int leftSpeed) {
  matrix_display(arrow_down);
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(rightSpeed, leftSpeed);
}

// 停止：速度置 0
void stopMotor() {
  setMotor(0, 0);
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