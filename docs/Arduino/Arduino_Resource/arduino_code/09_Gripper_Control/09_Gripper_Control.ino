#include <ESP32Servo.h>

const int CLAW_SERVO_PIN = 11; // Gripper servo signal
const int STEP_DELAY     = 15; // Delay per degree (ms), smaller is faster

Servo clawServo;

void setup() {
  Serial.begin(115200);

  // Allocate hardware timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // 50 Hz, pulse width 500~2500 µs
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);

  // Initial 90°
  clawServo.write(90);
  Serial.println("Gripper servo started, opening and closing repeatedly between 90° and 140°...");
  delay(1000);
}

void loop() {
  // 90° → 140°
  for (int pos = 90; pos <= 140; pos += 1) {
    clawServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // Pause 0.5 s

  // 140° → 90°
  for (int pos = 140; pos >= 90; pos -= 1) {
    clawServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // Pause 0.5 s
}
