#include <ESP32Servo.h>

const int PAN_SERVO_PIN = 42; // Pan-tilt servo signal
const int STEP_DELAY    = 15; // Delay per degree (ms); smaller is faster

Servo panServo;

void setup() {
  Serial.begin(115200);

  // Allocate hardware timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // 50 Hz, pulse width 500~2500 µs
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);

  // Return to zero
  panServo.write(0);
  Serial.println("Pan-tilt servo started, sweeping back and forth between 0° and 180°...");
  delay(1000);
}

void loop() {
  // 0° → 180°
  for (int pos = 0; pos <= 180; pos += 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // Pause 0.5 s

  // 180° → 0°
  for (int pos = 180; pos >= 0; pos -= 1) {
    panServo.write(pos);
    delay(STEP_DELAY);
  }
  delay(500); // Pause 0.5 s
}