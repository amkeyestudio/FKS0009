#include <IRremote.hpp>  // 红外解码库

// ===== 引脚 =====
#define IR_RECEIVE_PIN 48  // 红外接收头，IO48

#define MOTOR_AIN 38  // A 路方向
#define MOTOR_AEN 21  // A 路调速
#define MOTOR_BIN 40  // B 路方向
#define MOTOR_BEN 41  // B 路调速

// ===== 速度 =====
#define SPEED_RUN  180  // 直行
#define SPEED_TURN 180  // 原地转

// ===== 遥控器键值 =====
// 换遥控器后需按串口打印值修改
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64

// 指令超时（ms）自动停车，遥控器失联时避免失控
#define STOP_AFTER 1500

unsigned long lastKeyTime = 0;  // 最后有效按键时刻

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("红外遥控车启动，用方向盘控制");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  stopCar();  // 上电停车
}

void loop() {
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

    if (holding) {
      // 长按只刷新超时计时
      lastKeyTime = millis();
    } else if (isKnownKey(cmd)) {
      handleKey(cmd);
      lastKeyTime = millis();
    }

    IrReceiver.resume();
  }

  // 超过 1.5 s 无有效按键即停车
  if (millis() - lastKeyTime > STOP_AFTER) {
    stopCar();
  }
}

// 按键处理
void handleKey(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:
      Serial.println("前进");
      goForward(SPEED_RUN);
      break;
    case KEY_DOWN:
      Serial.println("后退");
      goBackward(SPEED_RUN);
      break;
    case KEY_LEFT:
      Serial.println("左转");
      spinLeft(SPEED_TURN);
      break;
    case KEY_RIGHT:
      Serial.println("右转");
      spinRight(SPEED_TURN);
      break;
    case KEY_STOP:
      Serial.println("停车");
      stopCar();
      break;
    default:
      break;
  }
}

// 是否为已定义按键
bool isKnownKey(uint8_t cmd) {
  return cmd == KEY_UP || cmd == KEY_DOWN || cmd == KEY_LEFT ||
         cmd == KEY_RIGHT || cmd == KEY_STOP;
}

// ===== 电机驱动 =====
// A 路接左轮、B 路接右轮，与第 7 课一致
// 若左右相反，互换下面两段
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // A 路：左轮
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);
  }

  // B 路：右轮
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

void goForward(int speed)  { driveMotors(speed, speed); }    // 前进：两轮同速正转
void goBackward(int speed) { driveMotors(-speed, -speed); }  // 后退：两轮同速反转
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // 左转：左轮后退、右轮前进
void spinRight(int speed)  { driveMotors(speed, -speed); }   // 右转：左轮前进、右轮后退
void stopCar()             { driveMotors(0, 0); }            // 停：占空比归零
