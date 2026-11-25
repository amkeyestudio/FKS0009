#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi =====
// Only 2.4 GHz is supported
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

// ===== Motor pins =====
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Gripper servo =====
#define CLAW_SERVO_PIN 11  // Gripper servo, same as Lesson 9

// ===== Dot matrix pins =====
#define CLK_Pin 9  // Clock, connect to module CLK
#define DIN_Pin 8  // Data, connect to module DIN

// ===== Speed =====
#define SPEED_RUN   180  // Driving speed
#define SPEED_TURN  180  // Spot-turn speed

// ===== Gripper angles =====
#define CLAW_OPEN   140  // Open
#define CLAW_CLOSE   90  // Closed

// ===== Timeout protection =====
#define STOP_AFTER 1500  // Auto-stop on command timeout (ms)

// ===== Expression numbers =====
#define FACE_EYES  0  // Standby: eyes
#define FACE_SMILE 1  // Goal scored: smiling face

// 16 bytes correspond to 16 columns, each byte's 8 bits correspond to 8 rows in one column

// Standby: two square eyes
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// Smiling face: goal scored
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

WebServer server(80);  // Web server
Servo clawServo;       // Gripper servo

int score = 0;                      // Current score
unsigned long lastCmdTime = 0;      // Time of the last command
unsigned long smileUntil = 0;       // Time when the smile ends
int curFace = -1;                   // Current expression, -1 means not shown

void setup() {
  // Pull both lines HIGH to set the bus idle
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // Show standby expression on power on

  // Motor pins
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // Gripper: 50 Hz, pulse width 500-2500 µs
  ESP32PWM::allocateTimer(0);
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);
  clawServo.write(CLAW_OPEN);  // Open on power on
  delay(300);

  // Connect to Wi-Fi
  Serial.begin(115200);
  delay(300);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Block until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Board IP: ");
  Serial.println(WiFi.localIP());  // Open this address in a phone browser

  // Home page
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // Command endpoint, d is the action name
  server.on("/cmd", []() {
    String d = server.arg("d");
    server.send(200, "text/plain", runCmd(d));  // Return "score,status" for direct use by the web page
  });

  // Score query, polled once per second by the web page
  server.on("/score", []() {
    server.send(200, "text/plain", String(score) + "," + faceName());
  });

  server.begin();
  Serial.println("Soccer car ready");
}

void loop() {
  server.handleClient();  // Must be called continuously

  // Stop on timeout
  if (millis() - lastCmdTime > STOP_AFTER) {
    stopCar();
  }

  // Smiling face returns to standby after 3 s
  if (smileUntil != 0 && millis() > smileUntil) {
    smileUntil = 0;
    showFace(FACE_EYES);
  }
}

// ===== Command dispatch =====
// Return "score,status" for web page sync
String runCmd(String d) {
  lastCmdTime = millis();  // Refresh the timeout timer

  if (d == "f") {
    goForward(SPEED_RUN);
  } else if (d == "b") {
    goBackward(SPEED_RUN);
  } else if (d == "l") {
    spinLeft(SPEED_TURN);
  } else if (d == "r") {
    spinRight(SPEED_TURN);
  } else if (d == "carry") {
    clawServo.write(CLAW_OPEN);   // Dribbling: open
  } else if (d == "shoot") {
    clawServo.write(CLAW_CLOSE);  // Shooting: close
  } else if (d == "goal") {
    score++;                        // Manual scoring
    celebrate();                    // Play celebration expression
  } else if (d == "reset") {
    score = 0;                      // Reset to zero
  } else {
    stopCar();                      // Stop for all other commands
  }

  return String(score) + "," + faceName();
}

// ===== Gripper usage =====
// Two states correspond to two actions:
//   Open 140°: ball held in the gripper, dribble while moving
//   Closed 90°: gripper arms close and push the ball out, i.e. shoot

// ===== Goal celebration =====
void celebrate() {
  showFace(FACE_SMILE);        // Switch to smiling face
  smileUntil = millis() + 3000;  // Restore after 3 s
}

// Current expression name, for the web page to display
String faceName() {
  if (curFace == FACE_SMILE) return "Goal!";
  return "Standby";
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
