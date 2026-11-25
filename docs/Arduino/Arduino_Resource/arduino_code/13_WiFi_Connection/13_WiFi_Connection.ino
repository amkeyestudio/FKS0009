#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi =====
// Only 2.4 GHz is supported
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

// ===== Pins =====
// Pan-tilt servo; mounted so 0° points right and 180° points left
#define PAN_SERVO_PIN 42  // Pan-tilt servo

const int trigPin = 13;  // Ultrasonic trigger
const int echoPin = 12;  // Ultrasonic echo

WebServer server(80);  // Web server on port 80
Servo panServo;
int panAngle = 90;     // Current pan-tilt angle, centers on power on

// This lesson uses only the servo and ultrasonic module, no motors

void setup() {
  Serial.begin(115200);
  delay(500);

  // Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Pan-tilt servo
  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(90);

  // Station mode connects to the router
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Block until connected successfully
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Board IP: ");
  Serial.println(WiFi.localIP());   // Open this address in a phone browser

  // Web page routes
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  server.on("/pan", []() {
    int a = server.arg("angle").toInt();
    a = constrain(a, 0, 180);   // Limit to 0-180
    panAngle = a;
    panServo.write(a);
    server.send(200, "text/plain", String(a));  // Return the current angle
  });

  server.on("/distance", []() {
    server.send(200, "text/plain", String(getDistance()));
  });

  server.begin();
  Serial.println("Web server started, open the above IP in a phone browser");
}

void loop() {
  server.handleClient();  // Must be called continuously
}

// Distance measurement
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
    distance = 0;
  }
  return distance;
}
