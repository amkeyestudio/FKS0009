#include <Wire.h>
#include <PCF8574.h>

PCF8574 pcf8574(0x20);

#define OUTA P4  // Far left
#define OUTB P0  // Left
#define OUTC P1  // Center
#define OUTD P2  // Right
#define OUTE P3  // Far right

#define MOTOR_AIN 38  // Channel A direction
#define MOTOR_AEN 21  // Channel A PWM
#define MOTOR_BIN 40  // Channel B direction
#define MOTOR_BEN 41  // Channel B PWM

// ===== Line tracking speed =====
#define BASE_SPEED   115  // Base speed for straight travel
#define RECOVER_IN   95   // Inner wheel reverse speed during sharp turns
#define RECOVER_OUT  125  // Outer wheel forward speed during sharp turns

// ===== State memory =====
uint8_t lastDir = 0;            // 0 straight, 1 line on left, 2 line on right
unsigned long lostStartTime = 0;// Timestamp when the line was lost

// Left/right motor mapping: channel A drives the left wheel, channel B drives the right wheel
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

  int A = !val.p4; // Far left
  int B = !val.p0; // Left
  int C = !val.p1; // Center
  int D = !val.p2; // Right
  int E = !val.p3; // Far right

  uint8_t state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E;

  // Reset the lost-line timer as soon as a sensor is on the line
  if (state != 0b00000) {
    lostStartTime = 0;
  }

  switch (state) {
    // Centered, go straight
    case 0b00100:
    case 0b01110:
    case 0b11111:
      driveMotors(BASE_SPEED, BASE_SPEED);
      lastDir = 0;
      break;

    // Slightly left of the line: reduce right wheel speed for fine adjustment
    case 0b01100: // B and C on the line
      driveMotors(85, BASE_SPEED); // Slow the right wheel to steer back onto the black line
      lastDir = 1;
      break;

    // Left of the line: major correction
    case 0b01000: // Only B on the line
    case 0b11100: // Sharp left turn
      driveMotors(45, RECOVER_OUT);
      lastDir = 1;
      break;

    // Far left: spin in place to the left
    case 0b11000:
    case 0b10000:
      driveMotors(-RECOVER_IN, RECOVER_OUT);
      lastDir = 1;
      break;

    // Slightly right of the line: reduce left wheel speed for fine adjustment
    case 0b00110: // C and D on the line
      driveMotors(BASE_SPEED, 85); // Slow the left wheel to steer back onto the black line
      lastDir = 2;
      break;

    // Right of the line: major correction
    case 0b00010: // Only D on the line
    case 0b00111: // Sharp right turn
      driveMotors(RECOVER_OUT, 45);
      lastDir = 2;
      break;

    // Far right: spin in place to the right
    case 0b00011:
    case 0b00001:
      driveMotors(RECOVER_OUT, -RECOVER_IN);
      lastDir = 2;
      break;

    // All sensors off the line: search in the remembered direction
    case 0b00000:
      if (lostStartTime == 0) {
        lostStartTime = millis(); // Record the moment the line was lost
      }
      // Search in the remembered direction within 1.5 s
      if (millis() - lostStartTime < 1500) {
        if (lastDir == 1) {
          driveMotors(-RECOVER_IN, RECOVER_OUT); // Line was on the left, turn left to find it
        } else if (lastDir == 2) {
          driveMotors(RECOVER_OUT, -RECOVER_IN); // Line was on the right, turn right to find it
        } else {
          driveMotors(0, 0); // Was going straight, stop
        }
      } else {
        // Timeout with no line found, stop
        driveMotors(0, 0);
      }
      break;

    default:
      break;
  }

  delay(5);
}