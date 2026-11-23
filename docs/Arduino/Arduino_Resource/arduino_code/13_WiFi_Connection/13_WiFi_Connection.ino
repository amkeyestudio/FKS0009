#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "webpage.h"  // 网页内容

// ===== Wi-Fi =====
// 仅支持 2.4 GHz
const char* WIFI_SSID = "你的WiFi名字";
const char* WIFI_PASS = "你的WiFi密码";

// ===== 引脚 =====
// 云台舵机；安装方向为 0° 指右、180° 指左
#define PAN_SERVO_PIN 42  // 云台舵机

const int trigPin = 13;  // 超声波触发
const int echoPin = 12;  // 超声波回响

WebServer server(80);  // 网页服务，80 端口
Servo panServo;
int panAngle = 90;     // 当前云台角度，上电回中

// 本课只用舵机与超声波，不驱动电机

void setup() {
  Serial.begin(115200);
  delay(500);

  // 超声波
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // 云台舵机
  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(90);

  // Station 模式连接路由器
  Serial.print("正在连接 ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // 阻塞等待连接成功
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi 已连接");
  Serial.print("板子的 IP：");
  Serial.println(WiFi.localIP());   // 手机浏览器访问此地址

  // 网页路由
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  server.on("/pan", []() {
    int a = server.arg("angle").toInt();
    a = constrain(a, 0, 180);   // 限幅 0~180
    panAngle = a;
    panServo.write(a);
    server.send(200, "text/plain", String(a));  // 回传当前角度
  });

  server.on("/distance", []() {
    server.send(200, "text/plain", String(getDistance()));
  });

  server.begin();
  Serial.println("网页服务已启动，手机浏览器打开上面的 IP");
}

void loop() {
  server.handleClient();  // 必须持续调用
}

// 测距
int getDistance() {
  long duration;
  int distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 25 ms 超时，防止无回波时阻塞
  duration = pulseIn(echoPin, HIGH, 25000);
  distance = duration * 0.034 / 2;

  if (distance < 2 || distance > 300) {
    distance = 0;
  }
  return distance;
}

