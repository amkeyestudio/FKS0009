#include <ESP32Servo.h>

const int CLAW_SERVO_PIN = 11; // 夹爪舵机信号
const int STEP_DELAY     = 15; // 每度延时（ms），越小越快

Servo clawServo;

void setup() {
  Serial.begin(115200);

  // 分配硬件定时器
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // 50 Hz，脉宽 500~2500 µs
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);

  // 初始 90°
  clawServo.write(90);
  Serial.println("夹爪舵机已启动，开始在 90°~140° 之间来回开合...");
  delay(1000);
}

void loop() {
  // 90° → 140°
  for (int pos = 90; pos <= 140; pos += 1) {
    clawServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // 停顿 0.5 s

  // 140° → 90°
  for (int pos = 140; pos >= 90; pos -= 1) {
    clawServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // 停顿 0.5 s
}