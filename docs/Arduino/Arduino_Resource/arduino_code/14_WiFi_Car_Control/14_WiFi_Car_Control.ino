#include <WiFi.h>
#include <WebServer.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi =====
// Only supports 2.4 GHz
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";


// ===== Pins =====
#define MOTOR_AIN 38  // Channel A direction
#define MOTOR_AEN 21  // Channel A speed control
#define MOTOR_BIN 40  // Channel B direction
#define MOTOR_BEN 41  // Channel B speed control

// ===== Speed =====
#define SPEED_RUN  180  // Forward
#define SPEED_TURN 180  // Spin in place

// Auto-stop when a command times out (ms)
// Prevents loss of control on packet loss or when the phone screen locks
#define STOP_AFTER 1500

WebServer server(80);
unsigned long lastCmdTime = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Block until the connection succeeds
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Board IP: ");
  Serial.println(WiFi.localIP());   // Open this address in a phone browser

  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // Single entry point for web page buttons; d is the direction
  server.on("/cmd", []() {
    String d = server.arg("d");
    runCmd(d);
    server.send(200, "text/plain", "ok");
  });

  server.begin();
  Serial.println("Remote control car is ready");
}

void loop() {
  server.handleClient();  // Must be called continuously

  // Stop on timeout
  if (millis() - lastCmdTime > STOP_AFTER) {
    stopCar();
  }
}

// Command dispatch
void runCmd(String d) {
  lastCmdTime = millis();   // Refresh the timeout timer

  if (d == "f") {
    Serial.println("Forward");
    goForward(SPEED_RUN);
  } else if (d == "b") {
    Serial.println("Backward");
    goBackward(SPEED_RUN);
  } else if (d == "l") {
    Serial.println("Turn left");
    spinLeft(SPEED_TURN);
  } else if (d == "r") {
    Serial.println("Turn right");
    spinRight(SPEED_TURN);
  } else {
    Serial.println("Stop");
    stopCar();
  }
}

// ===== Motor driver =====
// Channel A drives the left wheel, channel B the right wheel, as in Lesson 7
// If left and right are reversed, swap the two sections below
void driveMotors(int leftSpeed, int rightSpeed) {
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

void goForward(int speed)  { driveMotors(speed, speed); }
void goBackward(int speed) { driveMotors(-speed, -speed); }
void spinLeft(int speed)   { driveMotors(-speed, speed); }
void spinRight(int speed)  { driveMotors(speed, -speed); }
void stopCar()             { driveMotors(0, 0); }

