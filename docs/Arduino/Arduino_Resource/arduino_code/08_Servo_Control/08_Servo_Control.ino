#include <ESP32Servo.h>

const int PAN_SERVO_PIN = 42; // 云台舵机信号
const int STEP_DELAY    = 15; // 每度延时（ms），越小越快

Servo panServo;

void setup() {
  Serial.begin(115200);

  // 分配硬件定时器
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // 50 Hz，脉宽 500~2500 µs
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);

  // 归零
  panServo.write(0);
  Serial.println("云台舵机已启动，开始在 0°~180° 之间来回转动...");
  delay(1000);
}

void loop() {
  // 0° → 180°
  for (int pos = 0; pos <= 180; pos += 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // 停顿 0.5 s

  // 180° → 0°
  for (int pos = 180; pos >= 0; pos -= 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // 停顿 0.5 s
}