#include "AiCam.h"  // Vision module driver

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Vision module serial =====
// Controller RX <- module TX, controller TX -> module RX, common ground required
// Modify according to actual wiring
#define CAM_RX 18  // Controller receive pin
#define CAM_TX 17  // Controller transmit pin

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connected to module CLK
#define DIN_Pin 8  // Data, connected to module DIN

// ===== Line tracking parameters =====
#define BASE_SPEED   130  // Base speed for straight travel
#define KP           0.4  // Proportional coefficient
#define KD           0.6  // Derivative coefficient, suppresses overshoot
#define TURN_MAX      90  // Max correction per side
#define D_MAX         50  // Derivative term clamp

// ===== Line loss and search =====
#define SEARCH_SPEED  90  // Line search rotation speed
#define SEARCH_DEAD   15  // Line search dead zone
#define LOST_TIMEOUT 600  // Line search timeout (ms)

// ===== Valid flag =====
#define LINE_OK 5  // Third field = 5 means line recognized

// ===== Expression IDs =====
#define FACE_EYES     0  // Standby: eyes
#define FACE_SMILE    1  // Line tracking: smile
#define FACE_SURPRISE 2  // Line search: wide eyes
#define FACE_SAD      3  // Timeout: sad face

// 16 bytes for 16 columns, 8 bits per byte for 8 rows per column

// Standby: two square eyes
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smile: tracking stably
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Wide eyes: searching for line
unsigned char surprise[] = {
  0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x00, 0xC0,
  0xC0, 0x00, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x00
};

// Sad face: line search timeout
unsigned char sad[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x80, 0x40,
  0x40, 0x80, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // Vision module instance

int lastError = 0;            // Previous error, determines search direction on line loss
bool hasLast = false;         // Whether a previous error exists
unsigned long lostStart = 0;  // Time of line loss, 0 means no loss
int curFace = -1;             // Current expression, -1 means none shown

void setup() {
  // Pull both lines high to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();                       // Stop at power on

  // Show standby expression
  showFace(FACE_EYES);

  // Start vision module in line tracking mode
  aiCam.begin();
  aiCam.setMode("line");

  delay(1000);  // Wait for mode switch to complete
}

void loop() {
  aiCam.readSerial();  // Read serial each loop

  int offset = aiCam.getLineOffset();  // Offset: negative left, positive right
  int valid  = aiCam.getLineValid();   // Valid flag, 5 means line recognized

  if (valid == LINE_OK) {
    // Line recognized: PD computes differential speed
    lostStart = 0;         // Clear line-loss timer
    showFace(FACE_SMILE);  // Show smile

    int error = offset;  // Current error

    // Derivative term: change in error, suppresses overshoot
    int dError = 0;
    if (hasLast) {
      dError = error - lastError;
      dError = constrain(dError, -D_MAX, D_MAX);  // Clamp
    }

    // Differential = P*error + D*error change
    int correction = (int)(error * KP + dError * KD);

    // Clamp correction
    correction = constrain(correction, -TURN_MAX, TURN_MAX);

    // Positive correction (line to the right): left faster, right slower
    // Negative correction: the opposite
    driveMotors(BASE_SPEED + correction, BASE_SPEED - correction);

    lastError = error;  // Save current error
    hasLast = true;
  } else {
    // Line not recognized: search in the direction of previous error
    if (lostStart == 0) {    // Line just lost
      lostStart = millis();  // Start timer
    }

    if (millis() - lostStart < LOST_TIMEOUT) {
      showFace(FACE_SURPRISE);  // Searching for line

      // Direction unknown within dead zone, stop
      if (lastError > SEARCH_DEAD) {
        driveMotors(SEARCH_SPEED, -SEARCH_SPEED);   // Line was on the right: search right
      } else if (lastError < -SEARCH_DEAD) {
        driveMotors(-SEARCH_SPEED, SEARCH_SPEED);   // Line was on the left: search left
      } else {
        stopCar();                                  // Direction unknown, stop
      }
    } else {
      showFace(FACE_SAD);  // Line search timeout
      stopCar();           // Stop when off line
    }
  }

  delay(5);  // 5 ms per loop
}

// ===== Dot matrix display =====
// Only update display when expression changes
void showFace(int face) {
  if (face == curFace) return;  // Expression unchanged
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);       // Standby
  } else if (face == FACE_SMILE) {
    matrix_display(smile);      // Smile
  } else if (face == FACE_SURPRISE) {
    matrix_display(surprise);   // Wide eyes
  } else {
    matrix_display(sad);        // Sad face
  }
}

// Write 16-byte font bitmap to dot matrix display buffer
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

// Start condition: DIN falling edge while CLK is HIGH
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
    digitalWrite(CLK_Pin, LOW);  // Pull clock low to prepare data
    delayMicroseconds(3);

    // Send bit by bit starting from bit0
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // Pull clock high, module samples
    delayMicroseconds(3);

    send_data = send_data >> 1;  // Shift right for next bit
  }
}

// Stop condition: DIN rising edge while CLK is HIGH
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
// Channel A (38/21) drives left wheel, channel B (40/41) drives right wheel
// leftSpeed to channel A, rightSpeed to channel B; clamp to +/-255 first, then take absolute value for duty cycle
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // Left wheel clamp
  rightSpeed = constrain(rightSpeed, -255, 255);  // Right wheel clamp

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // Pull high: forward
    ledcWrite(MOTOR_AEN, leftSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // Pull low: reverse
    ledcWrite(MOTOR_AEN, -leftSpeed); // Absolute value
  }

  // Channel B: right wheel
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // Pull high: forward
    ledcWrite(MOTOR_BEN, rightSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // Pull low: reverse
    ledcWrite(MOTOR_BEN, -rightSpeed); // Absolute value
  }
}

// Basic movements
void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels forward at same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels reverse at same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Spin left: left reverse, right forward
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Spin right: left forward, right reverse
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle zero
