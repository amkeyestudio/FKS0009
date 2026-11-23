#ifndef AICAM_H
#define AICAM_H

#include <Arduino.h>

// ===== 串口参数 =====
// 模块固定 9600 8N1
#define CAM_BAUD 9600

// ===== 数据缓存 =====
// 模块内部完成图像处理，串口只回传结果
struct CamData {
  int faceX = 0;            // 人脸中心 X
  int faceY = 0;            // 人脸中心 Y
  bool faceValid = false;   // 本帧是否检测到

  String color;             // 颜色名，如 RED
  bool colorValid = false;  // 本帧是否识别到

  String qr;                // 二维码内容
  bool qrValid = false;     // 本帧是否识别到

  String card;              // 任务卡编号
  bool cardValid = false;   // 本帧是否识别到

  int lineOffset = 0;       // 偏差：负偏左，正偏右
  int lineAngle = 0;        // 角度：负左倾，正右倾
  int lineValid = 0;        // 有效标志，5 为识别到线
};

// ===== 驱动类 =====
// 封装串口收发与解析，业务代码不接触串口细节
class AiCam {
public:
  AiCam(uint8_t rxPin, uint8_t txPin);  // 传入主控 RX / TX 引脚

  void begin();                          // 启动串口，setup() 调用一次
  void sendCmd(const String &cmd);       // 发送一条指令
  void setMode(const String &mode);      // 切换模式：face / color / qr / card / line
  void readSerial();                     // 读取回传，loop() 每轮调用

  String getColor();                     // 取识别到的颜色名
  bool isColorValid();                   // 本帧是否识别到
  void clearColor();                     // 清空颜色缓存
  void flushInput();                     // 清空串口积压数据

  int getFaceX();                        // 取人脸中心 X
  int getFaceY();                        // 取人脸中心 Y
  bool isFaceValid();                    // 本帧是否检测到

  String getQrCode();                    // 取二维码内容
  String getCard();                      // 取任务卡编号

  int getLineOffset();                   // 取循线偏差
  int getLineAngle();                    // 取循线角度
  int getLineValid();                    // 取循线有效标志

private:
  uint8_t _rxPin;                        // 主控接收脚
  uint8_t _txPin;                        // 主控发送脚
  HardwareSerial *_serial;               // 硬件串口，指向 Serial2

  CamData _data;                         // 数据缓存
  String _rxLine;                        // 接收缓冲，按行累积

  bool parseLine(const String &line, String &mode, String &data);  // 拆分为模式与数据
  void updateData(const String &mode, const String &data);         // 按模式写进缓存
};

#endif
