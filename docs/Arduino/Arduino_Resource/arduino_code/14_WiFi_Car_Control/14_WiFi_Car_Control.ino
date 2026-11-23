#include <WiFi.h>
#include <WebServer.h>
#include "webpage.h"  // 网页内容

// ===== Wi-Fi =====
// 仅支持 2.4 GHz
const char* WIFI_SSID = "你的WiFi名字";
const char* WIFI_PASS = "你的WiFi密码";


// ===== 引脚 =====
#define MOTOR_AIN 38  // A 路方向
#define MOTOR_AEN 21  // A 路调速
#define MOTOR_BIN 40  // B 路方向
#define MOTOR_BEN 41  // B 路调速

// ===== 速度 =====
#define SPEED_RUN  180  // 直行
#define SPEED_TURN 180  // 原地转

// 指令超时（ms）自动停车
// 网络丢包或手机锁屏时防止失控
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

  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  // 网页按钮统一入口，d 为方向
  server.on("/cmd", []() {
    String d = server.arg("d");
    runCmd(d);
    server.send(200, "text/plain", "ok");
  });

  server.begin();
  Serial.println("遥控车已就绪");
}

void loop() {
  server.handleClient();  // 必须持续调用

  // 超时停车
  if (millis() - lastCmdTime > STOP_AFTER) {
    stopCar();
  }
}

// 指令分发
void runCmd(String d) {
  lastCmdTime = millis();   // 刷新超时计时

  if (d == "f") {
    Serial.println("前进");
    goForward(SPEED_RUN);
  } else if (d == "b") {
    Serial.println("后退");
    goBackward(SPEED_RUN);
  } else if (d == "l") {
    Serial.println("左转");
    spinLeft(SPEED_TURN);
  } else if (d == "r") {
    Serial.println("右转");
    spinRight(SPEED_TURN);
  } else {
    Serial.println("停车");
    stopCar();
  }
}

// ===== 电机驱动 =====
// A 路接左轮、B 路接右轮，与第 7 课一致
// 若左右相反，互换下面两段
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

