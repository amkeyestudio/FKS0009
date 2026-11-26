#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <PCF8574.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi =====
// Only 2.4 GHz is supported
const char* ssid = "Your WiFi name";
const char* password = "Your WiFi password";

// ===== Global objects =====
WebServer server(80);  // Web server
Servo myClaw;          // Gripper servo
Servo myPan;           // Pan-tilt servo

// ===== Motor pins =====
// Channel A drives the left wheel, channel B the right wheel (verified on the car)
// A/B are driver board channel numbers, not left/right
#define MOTOR_AIN 38  // Channel A direction: left wheel
#define MOTOR_AEN 21  // Channel A speed: left wheel
#define MOTOR_BIN 40  // Channel B direction: right wheel
#define MOTOR_BEN 41  // Channel B speed: right wheel

// ===== Dot matrix pins =====
// 8x16 dot matrix, software-simulated timing, no device address
// Shares io8/io9 with the line-tracking module's SDA/SCL
// Only refresh the display when switching modes to avoid interfering with the line-tracking bus
#define CLK_Pin 9   // Clock, connect to dot matrix CLK
#define DIN_Pin 8   // Data, connect to dot matrix DIN

// ===== Ultrasonic pins =====
const int trigPin = 13;  // Trigger
const int echoPin = 12;  // Echo

// ===== Servos =====
const int PAN_PIN  = 42;           // Pan-tilt servo
const int CLAW_PIN = 11;           // Gripper servo
const int CLAW_OPEN_ANGLE  = 140;  // Open
const int CLAW_CLOSE_ANGLE = 90;   // Closed
const int SERVO_STEP = 15;         // Pan-tilt step angle per move
int servoAngle = CLAW_CLOSE_ANGLE; // Current gripper angle, closed on power on
int panAngle = 90;                 // Current pan-tilt angle, faces straight ahead on power on
                                   // 0° points right, 180° points left

// ===== Line-tracking module (PCF8574) =====
PCF8574 pcf8574(0x20);
#define OUTA P4  // Far left
#define OUTB P0  // Left
#define OUTC P1  // Center
#define OUTD P2  // Right
#define OUTE P3  // Far right

const int BASE_SPEED = 115;  // Line-tracking straight speed
const int SPIN_SPEED = 125;  // Outer wheel speed for sharp turns
const int INNER_SPEED = 95;  // Inner wheel for sharp turns (reverse)
uint8_t lastDir = 0;         // 0 straight, 1 line on left, 2 line on right
unsigned long lostStartTime = 0;

// Dot matrix and line tracking share io8/io9, exclusively by mode:
//   Line-tracking mode -> display off, io8/io9 go to I2C
//   Other modes -> release I2C, io8/io9 go to dot matrix

// ===== Following parameters =====
const long safeDistance = 30;        // 30-50: following
const long closeDistanceFollow = 10; // Less than 10: back up
const long maxDistance = 50;         // Greater than 50: target lost

// ===== Obstacle avoidance parameters =====
const long closeDistanceAvoid = 15;  // Less than 15: treated as an obstacle
long leftDistance = 0;               // Left-side distance
long rightDistance = 0;              // Right-side distance

// ===== Operating modes =====
String carMode = "manual";  // Modes: manual / line / avoid / follow

// Manual mode command timeout (ms)
#define MANUAL_TIMEOUT 1500
unsigned long lastCmdTime = 0;

// Obstacle avoidance state machine, avoids blocking the web page
enum AvoidStep { AV_GO, AV_LEFT, AV_RIGHT, AV_TURN, AV_TURNING };
AvoidStep avStep = AV_GO;
unsigned long avTime = 0;

// ===== Dot matrix font bitmaps =====
// 16 bytes correspond to 16 columns, each byte's 8 bits to 8 rows in a column, 1 = on, 0 = off

// Smiley: manual
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Down arrow: spare for line tracking
// Display off in current line-tracking mode, not used for now
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7E, 0xFE,
  0xFE, 0x7E, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// Exclamation mark: obstacle avoidance
unsigned char alert_sign[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F,
  0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Heart: following
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
  0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
};

// ===== Dot matrix driver (software-simulated timing) =====

// Start condition: DIN pulled LOW while CLK is HIGH
void IIC_start() {
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(5);
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(5);
}

// Send one byte, LSB first: set data while CLK is LOW, sample when HIGH
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(5);

    if (send_data & 0x01) digitalWrite(DIN_Pin, HIGH);
    else                  digitalWrite(DIN_Pin, LOW);
    delayMicroseconds(5);

    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(5);

    send_data = send_data >> 1;
  }
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(5);
}

// Stop condition: DIN pulled HIGH while CLK is HIGH
void IIC_end() {
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(5);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(5);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(5);
}

// Write 16 bytes to the display buffer and turn the display on
// Just refresh the display, no bus ownership handling, about 3 ms
void matrix_write(unsigned char matrix_value[]) {
  // 0x40: write data mode, auto-increment address
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0: start writing from column 0
  IIC_start();
  IIC_send(0xC0);
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8F: display on, maximum brightness
  IIC_start();
  IIC_send(0x8F);
  IIC_end();
}

// 0x88: display off, display buffer contents retained
void matrix_off() {
  IIC_start();
  IIC_send(0x88);
  IIC_end();
}

// After refresh, pins remain with the dot matrix (not returned to I2C)
void matrix_show(unsigned char matrix_value[]) {
  Wire.end();                     // Release io8/io9 from I2C use
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  matrix_write(matrix_value);     // Write to display
}

// Handle the dot matrix according to mode
// Line-tracking mode turns display off and hands io8/io9 to I2C
void showModeIcon() {
  if (carMode == "line") {
    Wire.end();
    pinMode(CLK_Pin, OUTPUT);
    pinMode(DIN_Pin, OUTPUT);
    matrix_off();                 // Turn display off

    Wire.begin();                 // Hand io8/io9 to line tracking
    Wire.setClock(400000);
    pcf8574.begin();
    return;
  }

  // Other modes do not use I2C
  if (carMode == "manual")     matrix_show(smile);       // Manual: smiley
  else if (carMode == "avoid") matrix_show(alert_sign);  // Obstacle avoidance: exclamation mark
  else                         matrix_show(heart);       // Following: heart
}

// ===== Motor control =====

// Only write PWM, do not handle direction
// Parameters are left/right wheels, mapped internally to channels B/A
void setMotor(int leftSpeed, int rightSpeed) {
  ledcWrite(MOTOR_AEN, leftSpeed);    // Channel A: left wheel
  ledcWrite(MOTOR_BEN, rightSpeed);   // Channel B: right wheel
}

// Signed speed: forward / reverse
// Parameters are left/right wheels, mapped internally to channels B/A
// Used in line-tracking mode
void driveLine(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);
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

// Forward: direction pin HIGH
void forward(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(leftSpeed, rightSpeed);
}

// Backward: direction pin LOW
void back(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(leftSpeed, rightSpeed);
}

// Turn left: right wheel forward, left wheel reverse
void left(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(leftSpeed, rightSpeed);
}

// Turn right: right wheel reverse, left wheel forward
void right(int leftSpeed, int rightSpeed) {
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(leftSpeed, rightSpeed);
}

// Stop
void stopMotor() {
  setMotor(0, 0);
}

// ===== Ultrasonic distance measurement =====
int getDistance() {
  long duration;
  int distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 25 ms timeout to avoid blocking when there is no echo
  duration = pulseIn(echoPin, HIGH, 25000);
  distance = duration * 0.034 / 2;

  if (distance < 2 || distance > 300) {
    distance = 0;  // Out of range, set invalid
  }
  return distance;
}

// ===== 1. Line-tracking mode =====
void runLine() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 5) return;  // Read once every 5 ms
  lastRead = millis();

  PCF8574::DigitalInput val = pcf8574.digitalReadAll();

  int A = !val.p4;
  int B = !val.p0;
  int C = !val.p1;
  int D = !val.p2;
  int E = !val.p3;

  uint8_t state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E;

  if (state != 0) {
    lostStartTime = 0;  // On the line, clear the line-lost timer
  }

  switch (state) {
    case 0b00100:
    case 0b01110:
    case 0b11111:
      driveLine(BASE_SPEED, BASE_SPEED);      // Centered, go straight
      lastDir = 0;
      break;

    case 0b01100:
      driveLine(85, BASE_SPEED);              // Left of line: slow right wheel
      lastDir = 1;
      break;

    case 0b01000:
    case 0b11100:
      driveLine(45, SPIN_SPEED);              // Far left of line
      lastDir = 1;
      break;

    case 0b11000:
    case 0b10000:
      driveLine(-INNER_SPEED, SPIN_SPEED);    // Far left: spin left in place
      lastDir = 1;
      break;

    case 0b00110:
      driveLine(BASE_SPEED, 85);              // Right of line: slow left wheel
      lastDir = 2;
      break;

    case 0b00010:
    case 0b00111:
      driveLine(SPIN_SPEED, 45);              // Far right of line
      lastDir = 2;
      break;

    case 0b00011:
    case 0b00001:
      driveLine(SPIN_SPEED, -INNER_SPEED);    // Far right: spin right in place
      lastDir = 2;
      break;

    case 0b00000:                             // All sensors off the line
      if (lostStartTime == 0) {
        lostStartTime = millis();
      }
      if (millis() - lostStartTime < 1500) {
        if (lastDir == 1)      driveLine(-INNER_SPEED, SPIN_SPEED);
        else if (lastDir == 2) driveLine(SPIN_SPEED, -INNER_SPEED);
        else                   stopMotor();
      } else {
        stopMotor();                          // Not found before timeout, stop
      }
      break;

    default:
      break;
  }
}

// ===== 2. Following mode =====
void runFollow() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 100) return;
  lastCheck = millis();

  long d = getDistance();

  if (d > safeDistance && d <= maxDistance) {
    forward(150, 150);                        // 30-50 cm: go forward
  } else if (d < closeDistanceFollow && d > 0) {
    back(105, 150);                           // Less than 10 cm: back up
  } else {
    stopMotor();                              // Stop otherwise
  }
}

// ===== 3. Obstacle avoidance mode =====
// State machine executes step by step, no blocking
void runAvoidance() {
  switch (avStep) {
    case AV_GO: {
      long d = getDistance();
      if (d > 0 && d <= closeDistanceAvoid) {
        stopMotor();
        panAngle = 180;
        myPan.write(panAngle);        // Pan-tilt turns left
        avTime = millis();
        avStep = AV_LEFT;
      } else {
        forward(150, 150);            // No obstacle, go forward
      }
      break;
    }

    case AV_LEFT:
      if (millis() - avTime >= 300) { // Wait for servo to settle before measuring
        leftDistance = getDistance();
        panAngle = 0;
        myPan.write(panAngle);        // Pan-tilt turns right
        avTime = millis();
        avStep = AV_RIGHT;
      }
      break;

    case AV_RIGHT:
      if (millis() - avTime >= 300) {
        rightDistance = getDistance();
        panAngle = 90;
        myPan.write(panAngle);        // Pan-tilt returns to center
        avTime = millis();
        avStep = AV_TURN;
      }
      break;

    case AV_TURN:
      if (millis() - avTime >= 100) {
        if (leftDistance > rightDistance) left(255, 255);   // Turn toward the side with more space
        else                              right(255, 255);
        avTime = millis();
        avStep = AV_TURNING;
      }
      break;

    case AV_TURNING:
      if (millis() - avTime >= 500) { // Re-measure 0.5 s after turning
        stopMotor();
        avStep = AV_GO;
      }
      break;
  }
}

// ===== Mode switching =====
void setMode(String m) {
  if (m == "manual" || m == "line" || m == "avoid" || m == "follow") {
    bool changed = (carMode != m);   // Whether the mode actually changed
    carMode = m;
    stopMotor();      // Stop before changing mode
    avStep = AV_GO;
    lostStartTime = 0;
    panAngle = 90;
    myPan.write(90);  // Pan-tilt returns to center
    lastCmdTime = millis();

    Serial.print("Switched to mode: ");
    Serial.println(carMode);

    // Only refresh the display when the mode changes
    // Avoid repeatedly occupying the line-tracking I2C bus
    if (changed) showModeIcon();
  }
}

// ===== Web page request handling =====
void handleCmd() {
  // Mode switch request
  if (server.hasArg("mode")) {
    setMode(server.arg("mode"));
    server.send(200, "text/plain", "OK");
    return;
  }

  // Action command
  String moveCommand = server.arg("move");

  // Pan-tilt and gripper: available in any mode
  if (moveCommand == "servo_plus") {
    panAngle = min(180, panAngle + SERVO_STEP);   // Turn left
    myPan.write(panAngle);
  }
  else if (moveCommand == "servo_minus") {
    panAngle = max(0, panAngle - SERVO_STEP);     // Turn right
    myPan.write(panAngle);
  }
  else if (moveCommand == "claw_open") {
    servoAngle = CLAW_OPEN_ANGLE;
    myClaw.write(servoAngle);
  }
  else if (moveCommand == "claw_close") {
    servoAngle = CLAW_CLOSE_ANGLE;
    myClaw.write(servoAngle);
  }
  else {
    // Driving command: switch back to manual first, then execute
    // Order cannot be reversed, setMode includes a stop
    if (carMode != "manual") {
      setMode("manual");
    }
    lastCmdTime = millis();

    if (moveCommand == "forward")       forward(180, 180);
    else if (moveCommand == "backward") back(180, 180);
    else if (moveCommand == "left")     left(180, 180);
    else if (moveCommand == "right")    right(180, 180);
    else if (moveCommand == "stop")     stopMotor();
  }

  server.send(200, "text/plain", "OK");
}

// ===== Initialization =====
void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. Dot matrix: set both lines idle, then refresh
  // I2C is not initialized yet, call matrix_write directly
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  matrix_write(smile);

  // 2. Motors
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopMotor();

  // 3. Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // 4. Two servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  myPan.setPeriodHertz(50);
  myPan.attach(PAN_PIN, 500, 2500);
  myPan.write(90);

  myClaw.setPeriodHertz(50);
  myClaw.attach(CLAW_PIN, 500, 2500);
  myClaw.write(servoAngle);

  // 5. Line-tracking module
  Wire.begin();
  Wire.setClock(400000);
  pcf8574.begin();
  pcf8574.pinMode(OUTA, INPUT);
  pcf8574.pinMode(OUTB, INPUT);
  pcf8574.pinMode(OUTC, INPUT);
  pcf8574.pinMode(OUTD, INPUT);
  pcf8574.pinMode(OUTE, INPUT);

  // 6. Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Board IP: ");
  Serial.println(WiFi.localIP());

  // 7. Web page routes
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });
  server.on("/cmd", handleCmd);
  server.on("/distance", []() {
    server.send(200, "text/plain", String(getDistance()));
  });

  server.begin();
  Serial.println("Web server started, default manual mode");
}

// ===== Main loop =====
void loop() {
  server.handleClient();  // Handle web page requests

  if (carMode == "line") {
    runLine();
  } else if (carMode == "avoid") {
    runAvoidance();
  } else if (carMode == "follow") {
    runFollow();
  } else {
    // Manual mode: stop on timeout
    if (millis() - lastCmdTime > MANUAL_TIMEOUT) {
      stopMotor();
    }
  }
}
