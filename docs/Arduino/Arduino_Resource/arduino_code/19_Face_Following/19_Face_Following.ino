#include <ESP32Servo.h>
#include "AiCam.h"  // Vision module driver

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Pan-tilt servo =====
#define PAN_SERVO_PIN 42  // Lesson 8

// ===== Vision module serial port =====
// Controller RX <- module TX, controller TX -> module RX, common ground required
#define CAM_RX 18  // Controller RX pin
#define CAM_TX 17  // Controller TX pin

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connect to module CLK
#define DIN_Pin 8  // Data, connect to module DIN

// ===== Following parameters =====
#define PAN_CENTER     90  // Pan-tilt fixed facing straight ahead
#define FOLLOW_SPEED  100  // Forward speed while following
#define KP            0.5  // Proportional coefficient
#define TURN_MAX       80  // Maximum one-side correction
#define DEAD_ZONE       8  // Alignment dead zone
#define CALI_FRAMES    20  // Number of calibration frames
#define LOST_STOP     300  // Loss timeout (ms)

// ===== Debug switches =====
// 1 prints the calibrated center value, 0 silences it
#define DEBUG_OUT 1

// ===== Expression numbers =====
#define FACE_EYES  0  // No person: eyes
#define FACE_SMILE 1  // Person seen: smiling face

// 16 bytes correspond to 16 columns, each byte's 8 bits correspond to 8 rows in one column

// Standby: two square eyes
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smiling face: person seen
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // Vision module instance
Servo panServo;               // Pan-tilt servo

int centerX = -1;             // Calibrated center X, -1 means not done
long sumX = 0;                // Calibration accumulator
int caliCount = 0;            // Calibration frame counter
unsigned long lastSeen = 0;   // Last time a person was detected
int curFace = -1;             // Current expression, -1 means not shown

void setup() {
#if DEBUG_OUT
  Serial.begin(115200);
#endif

  // Pull both lines HIGH to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // Show standby expression

  // Pan-tilt: 50 Hz, pulse width 500-2500 µs
  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(PAN_CENTER);  // Face straight ahead, no rotation in this lesson
  delay(500);

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // Start the vision module in face detection mode
  aiCam.begin();
  aiCam.setMode("face");

  delay(500);  // Wait for the mode switch to complete
}

void loop() {
  aiCam.readSerial();  // Read the serial port each cycle

  if (!aiCam.isFaceValid()) {
    // No person detected: stop on timeout
    showFace(FACE_EYES);
    if (millis() - lastSeen > LOST_STOP) {
      stopCar();  // Stop on timeout
    }
    delay(20);
    return;
  }

  // Person detected
  lastSeen = millis();   // Refresh detection time
  showFace(FACE_SMILE);  // Show smiling face

  int x = aiCam.getFaceX();  // Face center X

  // Calibrate straight ahead
  // The module's X range is unknown (0-100 or 0-320 pixels),
  // Instead average the first 20 frames, do not hardcode the center
  if (centerX < 0) {
    sumX += x;
    caliCount++;
    stopCar();  // Stop during calibration

    if (caliCount >= CALI_FRAMES) {
      centerX = (int)(sumX / caliCount);  // Average is the straight-ahead X
#if DEBUG_OUT
      Serial.print("Frame center locked: ");
      Serial.println(centerX);
#endif
    }

    delay(20);
    return;
  }

  // Follow
  int err = x - centerX;  // Error: positive means right, negative means left

  if (abs(err) <= DEAD_ZONE) {
    // Aligned: move straight forward
    goForward(FOLLOW_SPEED);
  } else {
    // Not aligned: turn with differential speed
    // When err is positive (person on the right), left fast and right slow
    int correction = constrain((int)(err * KP), -TURN_MAX, TURN_MAX);
    driveMotors(FOLLOW_SPEED + correction, FOLLOW_SPEED - correction);
  }

  delay(20);  // 20 ms per cycle
}

// ===== Dot matrix display =====
// Only refresh the display when the expression changes
void showFace(int face) {
  if (face == curFace) return;  // Expression unchanged
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);   // Standby
  } else {
    matrix_display(smile);  // Smiling face
  }
}

// Write the 16-byte font bitmap into the dot matrix display buffer
void matrix_display(unsigned char matrix_value[]) {
  // 0x40: write data mode, auto-increment address
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0: display buffer start address 0
  IIC_start();
  IIC_send(0xC0);

  // Write 16 columns consecutively
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8A: display on, maximum brightness
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// Start condition: falling edge of DIN while CLK is HIGH
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// Send one byte, LSB first: set data while CLK is LOW, sample on rising edge
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);  // Pull clock LOW to prepare data
    delayMicroseconds(3);

    // Send bit by bit starting from bit0
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // Pull clock HIGH for the module to sample
    delayMicroseconds(3);

    send_data = send_data >> 1;  // Shift right to get the next bit
  }
}

// Stop condition: rising edge of DIN while CLK is HIGH
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

// ===== Motor driver =====
// Channel A drives the left wheel, channel B drives the right wheel (same as Lesson 7)
// leftSpeed goes to channel A, rightSpeed goes to channel B
// First limit to ±255, then take the absolute value for the duty cycle
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // Left wheel limit
  rightSpeed = constrain(rightSpeed, -255, 255);  // Right wheel limit

  // Channel B: right wheel
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // Pull HIGH: forward rotation
    ledcWrite(MOTOR_BEN, rightSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // Pull LOW: reverse rotation
    ledcWrite(MOTOR_BEN, -rightSpeed); // Take absolute value
  }

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // Pull HIGH: forward rotation
    ledcWrite(MOTOR_AEN, leftSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // Pull LOW: reverse rotation
    ledcWrite(MOTOR_AEN, -leftSpeed); // Take absolute value
  }
}

// Basic actions
void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels forward at the same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels reverse at the same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Turn left: left wheel back, right wheel forward
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Turn right: left wheel forward, right wheel back
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle to zero
