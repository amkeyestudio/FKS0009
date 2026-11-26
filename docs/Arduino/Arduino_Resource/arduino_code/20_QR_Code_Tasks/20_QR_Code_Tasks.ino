#include <ESP32Servo.h>
#include "AiCam.h"  // Vision module driver

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Pan-tilt servo =====
// Camera and ultrasonic module share a housing on the pan-tilt; this servo changes their direction
#define PAN_SERVO_PIN 42  // Pan-tilt servo, Lessons 8 and 18
#define PAN_CENTER    90  // Straight ahead, camera level

// ===== Vision module serial port =====
// Controller RX <- module TX, controller TX -> module RX, common ground required
#define CAM_RX 18  // Controller RX pin
#define CAM_TX 17  // Controller TX pin

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connect to module CLK
#define DIN_Pin 8  // Data, connect to module DIN

// ===== Task parameters =====
// Straight 150: below about 70 there is a startup dead zone, 150 gives a reliable start
#define SPEED_MOVE     150  // Forward / backward speed

// Turning uses different speeds on the two sides rather than reversing: reversing only spins in place, while this lesson needs to turn while moving
#define SPEED_TURN_OUT 150  // Outer wheel speed when turning
#define SPEED_TURN_IN   50  // Inner wheel speed when turning

// Action duration per card, determines travel distance
#define ACTION_TIME   1000  // Auto-stop when time is up

// ===== Debug switches =====
// 1 prints scanned content and byte count, 0 turns it off
#define DEBUG_OUT 1

// ===== Expression numbers =====
// Using numbers makes it easy for showFace() to detect expression changes and avoid redundant display writes
#define FACE_EYES  0  // Standby: eyes
#define FACE_SMILE 1  // Card recognized: smiley

// 16 bytes correspond to 16 columns, each byte's 8 bits to 8 rows in a column, bit0 is the top row

// Standby: two square eyes, columns 4/5 and 10/11 lit on rows 2-4
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smiley
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // Vision module instance
Servo panServo;               // Pan-tilt servo

// Content of the last card executed, to distinguish a held card from a new one
String lastQr = "";

// Current expression, -1 means not shown
int curFace = -1;

void setup() {
#if DEBUG_OUT
  // Debug serial port, not compiled when DEBUG_OUT is 0
  Serial.begin(115200);
#endif

  // Pull both lines HIGH to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // Show standby expression

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);      // Right wheel direction
  pinMode(MOTOR_AIN, OUTPUT);      // Left wheel direction
  ledcAttach(MOTOR_BEN, 1000, 8);  // Right wheel PWM: 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);  // Left wheel PWM
  stopCar();                       // Stop on power on

  // Pan-tilt: point the camera straight ahead on power on
  ESP32PWM::allocateTimer(0);                 // Allocate an LEDC timer
  panServo.setPeriodHertz(50);                // 50 Hz
  panServo.attach(PAN_SERVO_PIN, 500, 2500);  // IO42, pulse width 500-2500 µs
  panServo.write(PAN_CENTER);                 // Turn to straight ahead
  delay(300);                                 // Wait for the servo to settle

  // Start the vision module in QR code mode
  aiCam.begin();          // Open Serial2 and bind pins
  aiCam.setMode("qr");    // Send MODE:QR

  delay(1000);  // Wait for mode switch to finish, avoid reading stray data during switching
}

// ===== Main loop =====
// Each cycle: read serial -> check for a new card -> execute or stop
void loop() {
  aiCam.readSerial();  // Read the serial port and update the buffer

  String qr = aiCam.getQrCode();  // Current QR code content, empty means no card

#if DEBUG_OUT
  // Print the raw content and byte count when content changes
  // The four cards are 7 / 8 / 4 / 5 bytes respectively
  static String lastPrinted = "";        // static persists across cycles
  if (qr.length() > 0 && qr != lastPrinted) {
    lastPrinted = qr;
    Serial.print("QR code scanned: ");
    Serial.print(qr);
    Serial.print("  Byte count ");
    Serial.println(qr.length());
  }
#endif

  // No card
  // Clear lastQr to release the lock
  if (qr.length() == 0) {
    lastQr = "";          // Allow the next card to trigger
    stopCar();            // Stop the car
    showFace(FACE_EYES);  // Standby expression
    delay(20);
    return;               // End this cycle
  }

  // Same card not removed
  // Skip, do not execute again
  if (qr == lastQr) {
    delay(20);
    return;
  }

  // New card
  // Record before executing to avoid repeated triggering during execution
  lastQr = qr;    // Record this card
  doTask(qr);     // Execute action, blocks internally for 1 s

  delay(20);
}

// ===== Task execution =====
// Each of the four cards runs an action and stops automatically when time is up
//   FORWARD  both wheels forward at the same speed
//   BACKWARD both wheels reverse at the same speed
//   LEFT     left 50 / right 150, arc to the left
//   RIGHT    left 150 / right 50, arc to the right
void doTask(String qr) {
  showFace(FACE_SMILE);  // Show smiley

  // Convert to uppercase before comparing
  String key = qr;
  key.trim();        // Remove leading and trailing whitespace
  key.toUpperCase(); // Convert to uppercase

  bool known = true;  // Whether it is a known card

  if (key == "FORWARD") {
    goForward(SPEED_MOVE);                        // Both wheels forward at the same speed
  } else if (key == "BACKWARD") {
    goBackward(SPEED_MOVE);                       // Both wheels reverse at the same speed
  } else if (key == "LEFT") {
    driveMotors(SPEED_TURN_IN, SPEED_TURN_OUT);   // Left slow, right fast, arc to the left
  } else if (key == "RIGHT") {
    driveMotors(SPEED_TURN_OUT, SPEED_TURN_IN);   // Left fast, right slow, arc to the right
  } else {
    known = false;  // Not one of the four cards, no action
  }

  // Drive motors only for known cards
  if (known) {
    delay(ACTION_TIME);  // Run for the full set duration
    stopCar();           // Stop when time is up
  }
}

// ===== Dot matrix display =====
// Only refresh the display when the expression changes
// Writing the display needs over 20 bytes of software I2C; frequent calls slow the main loop
void showFace(int face) {
  if (face == curFace) return;  // Expression unchanged
  curFace = face;               // Record the current expression

  if (face == FACE_EYES) {
    matrix_display(eyes);   // Standby
  } else {
    matrix_display(smile);  // Smiley
  }
}

// Write the 16-byte font bitmap into the dot matrix display buffer
// Sequence: write data command -> start address -> send 16 bytes -> turn display on
void matrix_display(unsigned char matrix_value[]) {
  // 0x40: write data mode, auto-increment address
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0: start address 0
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
    ledcWrite(MOTOR_BEN, -rightSpeed); // Take absolute value, direction set by direction pin
  }

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // Pull HIGH: forward rotation
    ledcWrite(MOTOR_AEN, leftSpeed);  // Duty cycle = speed
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // Pull LOW: reverse rotation
    ledcWrite(MOTOR_AEN, -leftSpeed); // Take absolute value of duty cycle
  }
}

// Basic actions
// Single speed parameter, same value for both wheels
void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels forward at the same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels reverse at the same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Turn left: spin in place
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Turn right: spin in place
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle to zero
