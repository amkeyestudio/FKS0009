#include <Wire.h>
#include <PCF8574.h>

PCF8574 pcf8574(0x20);

#define OUTA P4  // 最左
#define OUTB P0  // 左
#define OUTC P1  // 中
#define OUTD P2  // 右
#define OUTE P3  // 最右

#define MOTOR_AIN 38  // A 路方向
#define MOTOR_AEN 21  // A 路 PWM
#define MOTOR_BIN 40  // B 路方向
#define MOTOR_BEN 41  // B 路 PWM

// ===== 循迹速度 =====
#define BASE_SPEED   115  // 直行基础速度
#define RECOVER_IN   95   // 急转时内侧轮反转速度
#define RECOVER_OUT  125  // 急转时外侧轮正转速度

// ===== 状态记忆 =====
uint8_t lastDir = 0;            // 0 直行，1 线在左，2 线在右
unsigned long lostStartTime = 0;// 丢线起始时刻

// 左右电机映射：A 路接左轮、B 路接右轮
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
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

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  pinMode(MOTOR_BEN, OUTPUT);
  pinMode(MOTOR_AEN, OUTPUT);

  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  pcf8574.begin();
  pcf8574.pinMode(OUTA, INPUT);
  pcf8574.pinMode(OUTB, INPUT);
  pcf8574.pinMode(OUTC, INPUT);
  pcf8574.pinMode(OUTD, INPUT);
  pcf8574.pinMode(OUTE, INPUT);

  driveMotors(0, 0);
  delay(2000);
}

void loop() {
  PCF8574::DigitalInput val = pcf8574.digitalReadAll();

  int A = !val.p4; // 最左
  int B = !val.p0; // 左
  int C = !val.p1; // 中
  int D = !val.p2; // 右
  int E = !val.p3; // 最右

  uint8_t state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E;

  // 压到线即清零丢线计时
  if (state != 0b00000) {
    lostStartTime = 0;
  }

  switch (state) {
    // 居中直行
    case 0b00100:
    case 0b01110:
    case 0b11111:
      driveMotors(BASE_SPEED, BASE_SPEED);
      lastDir = 0;
      break;

    // 偏左：右轮降速微调
    case 0b01100: // B、C 压线
      driveMotors(85, BASE_SPEED); // 右轮降速贴回黑线
      lastDir = 1;
      break;

    // 偏左：大幅修正
    case 0b01000: // 仅 B 压线
    case 0b11100: // 左直角弯
      driveMotors(45, RECOVER_OUT);
      lastDir = 1;
      break;

    // 严重偏左：原地左转
    case 0b11000:
    case 0b10000:
      driveMotors(-RECOVER_IN, RECOVER_OUT);
      lastDir = 1;
      break;

    // 偏右：左轮降速微调
    case 0b00110: // C、D 压线
      driveMotors(BASE_SPEED, 85); // 左轮降速贴回黑线
      lastDir = 2;
      break;

    // 偏右：大幅修正
    case 0b00010: // 仅 D 压线
    case 0b00111: // 右直角弯
      driveMotors(RECOVER_OUT, 45);
      lastDir = 2;
      break;

    // 严重偏右：原地右转
    case 0b00011:
    case 0b00001:
      driveMotors(RECOVER_OUT, -RECOVER_IN);
      lastDir = 2;
      break;

    // 全部脱线：按记忆方向找线
    case 0b00000:
      if (lostStartTime == 0) {
        lostStartTime = millis(); // 记录丢线时刻
      }
      // 1.5 s 内按记忆方向找线
      if (millis() - lostStartTime < 1500) {
        if (lastDir == 1) {
          driveMotors(-RECOVER_IN, RECOVER_OUT); // 线原在左，左转找回
        } else if (lastDir == 2) {
          driveMotors(RECOVER_OUT, -RECOVER_IN); // 线原在右，右转找回
        } else {
          driveMotors(0, 0); // 原为直行，停车
        }
      } else {
        // 超时未找到，停车
        driveMotors(0, 0);
      }
      break;

    default:
      break;
  }

  delay(5);
}