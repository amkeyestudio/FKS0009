#include "AiCam.h"  // Vision module driver

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Vision module serial port =====
// Controller RX <- module TX, controller TX -> module RX, common ground required
// Modify according to actual wiring
#define CAM_RX 18  // Controller RX pin
#define CAM_TX 17  // Controller TX pin

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connect to module CLK
#define DIN_Pin 8  // Data, connect to module DIN

// ===== Line-following parameters =====
#define BASE_SPEED   130  // Base straight-line speed
#define KP           0.4  // Proportional coefficient
#define KD           0.6  // Derivative coefficient, suppresses overshoot
#define TURN_MAX      90  // Maximum one-side correction
#define D_MAX         50  // Derivative term limit

// ===== Line loss and search =====
#define SEARCH_SPEED  90  // Line-search rotation speed
#define SEARCH_DEAD   15  // Line-search dead zone
#define LOST_TIMEOUT 600  // Line-search timeout (ms)

// ===== Valid flag =====
#define LINE_OK 5  // The module's third field being 5 means a line is detected

// ===== Expression numbers =====
#define FACE_EYES     0  // Standby: eyes
#define FACE_SMILE    1  // Following line: smiley
#define FACE_SURPRISE 2  // Searching for line: wide eyes
#define FACE_SAD      3  // Timeout: sad face

// 16 bytes correspond to 16 columns, each byte's 8 bits correspond to 8 rows in one column

// Standby: two square eyes
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smiley: stable on the line
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Wide eyes: searching for the line
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

int lastError = 0;            // Last error, determines search direction when line is lost
bool hasLast = false;         // Whether a previous error exists
unsigned long lostStart = 0;  // Time when line was lost, 0 means not lost
int curFace = -1;             // Current expression, -1 means not shown

void setup() {
  // Pull both lines HIGH to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();                       // Stop on power on

  // Show standby expression
  showFace(FACE_EYES);

  // Start the vision module in line-following mode
  aiCam.begin();
  aiCam.setMode("line");

  delay(1000);  // Wait for the mode switch to complete
}

void loop() {
  aiCam.readSerial();  // Read the serial port each cycle

  int offset = aiCam.getLineOffset();  // Error: negative means left, positive means right
  int valid  = aiCam.getLineValid();   // Valid flag, 5 means line detected

  if (valid == LINE_OK) {
    // Line detected: PD calculation for differential speed
    lostStart = 0;         // Clear line-lost timer
    showFace(FACE_SMILE);  // Show smiley

    int error = offset;  // Current error

    // Derivative term: change in error, suppresses overshoot
    int dError = 0;
    if (hasLast) {
      dError = error - lastError;
      dError = constrain(dError, -D_MAX, D_MAX);  // Limit range
    }

    // Differential = P x error + D x change in error
    int correction = (int)(error * KP + dError * KD);

    // Limit the correction
    correction = constrain(correction, -TURN_MAX, TURN_MAX);

    // When correction is positive (to the right), left fast and right slow, nose turns right
    // Opposite when correction is negative
    driveMotors(BASE_SPEED + correction, BASE_SPEED - correction);

    lastError = error;  // Save the current error
    hasLast = true;
  } else {
    // No line detected: search in the direction of the last error
    if (lostStart == 0) {    // Line just lost
      lostStart = millis();  // Start timing
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
      showFace(FACE_SAD);  // Line-search timeout
      stopCar();           // Stop after losing the line
    }
  }

  delay(5);  // 5 ms per cycle
}

// ===== Dot matrix display =====
// Only refresh the display when the expression changes
void showFace(int face) {
  if (face == curFace) return;  // Expression unchanged
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);       // Standby
  } else if (face == FACE_SMILE) {
    matrix_display(smile);      // Smiley
  } else if (face == FACE_SURPRISE) {
    matrix_display(surprise);   // Wide eyes
  } else {
    matrix_display(sad);        // Sad face
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
// Channel A (38/21) drives the left wheel, channel B (40/41) the right wheel
// leftSpeed goes to channel A, rightSpeed to channel B; first limit to ±255, then take absolute value for duty cycle
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // Left wheel limit
  rightSpeed = constrain(rightSpeed, -255, 255);  // Right wheel limit

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // Pull HIGH: forward rotation
    ledcWrite(MOTOR_AEN, leftSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // Pull LOW: reverse rotation
    ledcWrite(MOTOR_AEN, -leftSpeed); // Take absolute value
  }

  // Channel B: right wheel
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // Pull HIGH: forward rotation
    ledcWrite(MOTOR_BEN, rightSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // Pull LOW: reverse rotation
    ledcWrite(MOTOR_BEN, -rightSpeed); // Take absolute value
  }
}

// Basic actions
void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels forward at the same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels reverse at the same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Turn left: left wheel back, right wheel forward
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Turn right: left wheel forward, right wheel back
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle to zero
