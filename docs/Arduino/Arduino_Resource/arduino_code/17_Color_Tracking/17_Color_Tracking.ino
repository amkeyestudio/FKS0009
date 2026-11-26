#include <ESP32Servo.h>
#include "AiCam.h"  // Vision module driver

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Servo pins =====
#define CLAW_SERVO_PIN 11  // Gripper servo, Lesson 9
#define PAN_SERVO_PIN  42  // Pan-tilt servo, Lesson 8

// ===== Vision module serial port =====
// Controller RX <- module TX, controller TX -> module RX, common ground required
#define CAM_RX 18  // Controller RX pin
#define CAM_TX 17  // Controller TX pin

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connect to module CLK
#define DIN_Pin 8  // Data, connect to module DIN

// ===== Speed =====
#define SPEED_RUN      180  // Straight-line delivery speed
#define SPEED_TURN     180  // Spot-turn speed
#define SPEED_APPROACH 150  // Approach speed (low)

// ===== Angles =====
#define CLAW_OPEN   140  // Open
#define CLAW_CLOSE   90  // Closed
#define PAN_CENTER   90  // Pan-tilt centered

// ===== Action durations (ms, fine-tune for field and voltage) =====
#define T_APPROACH 350  // Approach duration
#define T_TURN     600  // Duration for a 90-degree spot turn
#define T_DELIVER  800  // Delivery duration

// ===== Expression numbers =====
#define FACE_EYES   0  // Eyes: standby
#define FACE_SMILE  1  // Smiley: target recognized or delivery complete
#define FACE_HEART  2  // Heart: gripped

// Standby: two square eyes, columns 4/5 and 10/11 lit on rows 2-4
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smiley: 16 bytes correspond to 16 columns, each byte's 8 bits to 8 rows per column
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Heart: same format as the smiley
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
  0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // Vision module instance
Servo clawServo;              // Gripper servo
Servo panServo;               // Pan-tilt servo

int curFace = -1;  // Current expression, -1 means not shown

void setup() {
  Serial.begin(115200);

  // Pull both lines HIGH to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  // Each servo uses a timer to avoid conflicts with motor PWM
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  // Pan-tilt: 50 Hz, pulse width 500-2500 µs
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);

  // Gripper: 50 Hz, pulse width 500-2500 µs
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);

  // Camera moves with pan-tilt, centers to face straight ahead
  panServo.write(PAN_CENTER);
  // Gripper open and on standby
  clawServo.write(CLAW_OPEN);

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // Start the vision module in color recognition mode
  aiCam.begin();
  aiCam.setMode("color");

  // Show standby expression
  showFace(FACE_EYES);

  Serial.println("Color-tracking car ready, place a red or yellow block in front");
  delay(1000);  // Wait for servos to settle and module switch to complete
}

void loop() {
  aiCam.readSerial();  // Read the serial port each cycle

  String color = aiCam.getColor();  // Currently recognized color

  // No valid target or not red/yellow: stop and wait
  if (!aiCam.isColorValid() || (color != "RED" && color != "YELLOW")) {
    stopCar();
    showFace(FACE_EYES);  // Standby expression
    return;
  }

  Serial.print("Target recognized: ");
  Serial.println(color);

  showFace(FACE_SMILE);  // Show smiley
  aiCam.clearColor();    // Clear buffer to avoid repeated triggering

  pickAndDeliver(color);  // Run the pick-and-deliver sequence

  aiCam.flushInput();   // Discard data accumulated during the action
  aiCam.clearColor();   // Clear the buffer again
  showFace(FACE_EYES);  // Back to standby
  delay(2000);          // Cool down 2 s before searching for the next target
}

// ===== Pick and deliver =====
// Outbound: approach -> grip -> turn -> deliver -> release
// Return retraces the steps to the start in reverse order
void pickAndDeliver(String color) {
  // Outbound
  // 1. Approach at low speed
  Serial.println("1 Approaching");
  goForward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();

  // 2. Grip
  Serial.println("2 Gripping");
  clawServo.write(CLAW_CLOSE);
  delay(400);           // Wait for the servo to settle
  showFace(FACE_HEART); // Show heart

  // 3. Turn by color: red left, yellow right
  Serial.println("3 Turning to drop zone");
  if (color == "RED") {
    spinLeft(SPEED_TURN);  // Red: turn left
  } else {
    spinRight(SPEED_TURN);  // Yellow: turn right
  }
  delay(T_TURN);
  stopCar();

  // 4. Deliver
  Serial.println("4 Delivering");
  goForward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();

  // 5. Open gripper to unload
  Serial.println("5 Releasing");
  clawServo.write(CLAW_OPEN);
  delay(300);

  // Return
  // 6. Back to the turn point
  Serial.println("6 Backing to turn point");
  goBackward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();

  // 7. Straighten up
  Serial.println("7 Straightening");
  if (color == "RED") {
    spinRight(SPEED_TURN);  // Turn back in reverse
  } else {
    spinLeft(SPEED_TURN);  // Turn back in reverse
  }
  delay(T_TURN);
  stopCar();

  // 8. Back to the start
  Serial.println("8 Backing to start");
  goBackward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();

  showFace(FACE_SMILE);  // Show smiley
  Serial.println("Delivery complete, back at start");
}

// ===== Dot matrix display =====
// Only refresh the display when the expression changes
void showFace(int face) {
  if (face == curFace) return;  // Expression unchanged
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);   // Standby
  } else if (face == FACE_SMILE) {
    matrix_display(smile);  // Smiley
  } else {
    matrix_display(heart);  // Heart
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
