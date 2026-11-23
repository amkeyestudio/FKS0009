// ===== 引脚 =====
// 点阵屏
#define CLK_Pin 9   // 时钟
#define DIN_Pin 8   // 数据

// 电机驱动
#define MOTOR_AIN 38  // 左电机方向
#define MOTOR_AEN 21  // 左电机调速
#define MOTOR_BIN 40  // 右电机方向
#define MOTOR_BEN 41  // 右电机调速

// ===== 速度参数 =====
#define SPEED_RUN  150  // 直行速度，0~255
#define SPEED_TURN 150  // 原地转向速度，0~255

// ===== 点阵字模（16 列 × 8 行）=====
// 爱心：直行
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
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

// 笑脸：停车待机、调速滚动
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40, 
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// ===== 点阵驱动 =====
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(3);
    digitalWrite(DIN_Pin, (send_data & 0x01) ? HIGH : LOW);
    delayMicroseconds(3);
    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(3);
    send_data >>= 1;
  }
}

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

void matrix_display(unsigned char matrix_value[]) {
  IIC_start();
  IIC_send(0x40); // 写数据模式，地址自增
  IIC_end();
  
  IIC_start();
  IIC_send(0xC0); // 从地址 0 开始写
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();
  
  IIC_start();
  IIC_send(0x8A); // 开显示，最大亮度
  IIC_end();
}

// 字模左移 shift 列后显示
void matrix_display_scroll(unsigned char base_pattern[], int shift) {
  unsigned char scroll_data[16];
  for (int i = 0; i < 16; i++) {
    scroll_data[i] = base_pattern[(i + shift) % 16];
  }
  matrix_display(scroll_data);
}

// ===== 电机驱动 =====
// 速度带符号：正转 / 反转 / 0 停
void driveMotors(int rightSpeed, int leftSpeed) {
  rightSpeed  = constrain(rightSpeed,  -255, 255);
  leftSpeed = constrain(leftSpeed, -255, 255);

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

// 每个动作同时刷新点阵图案
void goForward(int speed) {
  matrix_display(heart);        // 前进：爱心
  driveMotors(speed, speed);
}

void goBackward(int speed) {
  matrix_display(arrow_down);   // 后退：向下箭头
  driveMotors(-speed, -speed);
}

void spinRight(int speed) {
  matrix_display(arrow_left);   // 原地右转，点阵显示左箭头
  driveMotors(-speed, speed);
}

void spinLeft(int speed) {
  matrix_display(arrow_right);  // 原地左转，点阵显示右箭头
  driveMotors(speed, -speed);
}

void stopCar() {
  matrix_display(smile);        // 停车：笑脸
  driveMotors(0, 0);
}

// ===== 初始化 =====
void setup() {
  Serial.begin(115200);

  // 点阵上电清屏
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  // 方向脚输出，调速脚启用 PWM
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  pinMode(MOTOR_BEN, OUTPUT);
  pinMode(MOTOR_AEN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);

  // 上电停车并显示笑脸
  stopCar();
  Serial.println("电机驱动实验开始，1 秒后动作");
  delay(1000);
}

// ===== 主循环 =====
void loop() {
  // 直行 2 s
  Serial.println("前进（显示爱心）");
  goForward(SPEED_RUN);
  delay(2000);

  stopCar();
  delay(500);

  // 后退 2 s
  Serial.println("后退（显示向下箭头）");
  goBackward(SPEED_RUN);
  delay(2000);

  stopCar();
  delay(500);

  // 原地右转 1.5 s
  Serial.println("原地右转");
  spinRight(SPEED_TURN);
  delay(1500);

  stopCar();
  delay(500);

  // 原地左转 1.5 s
  Serial.println("原地左转");
  spinLeft(SPEED_TURN);
  delay(1500);

  stopCar();
  delay(1000);

  // 调速：占空比 0→255→0，点阵同步滚动
  int scroll_step = 0;

  Serial.println("加速：占空比 0 -> 255");
  for (int duty = 0; duty <= 255; duty += 5) {
    driveMotors(duty, duty);
    if (duty % 20 == 0) {
      matrix_display_scroll(heart, scroll_step++);
    }
    delay(20);
  }

  Serial.println("减速：占空比 255 -> 0");
  for (int duty = 255; duty >= 0; duty -= 5) {
    driveMotors(duty, duty);
    if (duty % 20 == 0) {
      matrix_display_scroll(heart, scroll_step++);
    }
    delay(20);
  }

  stopCar();
  Serial.println("停止，2 秒后进入下一轮");
  delay(2000);
}
