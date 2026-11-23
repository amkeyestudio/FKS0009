#include "AiCam.h"

// ===== 构造与初始化 =====
AiCam::AiCam(uint8_t rxPin, uint8_t txPin) : _rxPin(rxPin), _txPin(txPin) {
  _serial = &Serial2;  // 使用硬件串口 2，与 USB 下载口互不干扰
}

// 启动串口，setup() 调用一次
void AiCam::begin() {
  _serial->begin(CAM_BAUD, SERIAL_8N1, _rxPin, _txPin);  // 9600 8N1，指定收发引脚
  delay(500);                                            // 等模块上电稳定

  Serial.println("视觉模块串口已启动");
  sendCmd("GET:MODE");    // 查询当前模式
  delay(50);
  sendCmd("GET:OUTPUT");  // 查询输出格式
  delay(50);
}

// ===== 发送指令 =====
void AiCam::sendCmd(const String &cmd) {
  if (cmd.length() == 0) return;  // 空指令不发送
  _serial->print(cmd);            // 指令内容
  _serial->print("\r\n");         // 以 CRLF 结尾
  Serial.print("发送 -> ");
  Serial.println(cmd);            // 同步打印到 USB 串口
}

// ===== 切换模式 =====
void AiCam::setMode(const String &mode) {
  String m = mode;       // 复制后处理，不改入参
  m.trim();              // 去首尾空格
  m.toLowerCase();       // 统一小写

  if (m == "face") {
    sendCmd("MODE:FACE");    // 人脸检测
  } else if (m == "color") {
    sendCmd("MODE:COLOR");   // 颜色识别
  } else if (m == "qr") {
    sendCmd("MODE:QR");      // 二维码
  } else if (m == "card") {
    sendCmd("MODE:CARD");    // 任务卡
  } else if (m == "line") {
    sendCmd("MODE:LINE");    // 摄像头巡线
  } else {
    Serial.print("未知模式：");
    Serial.println(mode);    // 打印模式名
  }
}

// ===== 拆帧 =====
// 兼容 "MODE:COLOR:RED" 与 "COLOR:RED" 两种格式
bool AiCam::parseLine(const String &line, String &mode, String &data) {
  int first = line.indexOf(':');        // 第一个冒号
  if (first == -1) return false;        // 无冒号，无效帧

  int second = line.indexOf(':', first + 1);  // 查第二个冒号
  if (second == -1) {
    mode = line.substring(0, first);          // 一个冒号：前为模式
    data = line.substring(first + 1);         // 后为数据
  } else {
    mode = line.substring(first + 1, second); // 两个冒号：中间为模式
    data = line.substring(second + 1);        // 末尾为数据
  }
  return true;
}

// ===== 写缓存 =====
void AiCam::updateData(const String &mode, const String &data) {
  if (mode.equalsIgnoreCase("FACE")) {
    int comma = data.indexOf(',');          // 格式：x,y
    if (comma != -1) {
      _data.faceX = data.substring(0, comma).toInt();      // X
      _data.faceY = data.substring(comma + 1).toInt();     // Y
      _data.faceValid = (_data.faceX > 1);                 // X>1 视为检测到
    }
  } else if (mode.equalsIgnoreCase("COLOR")) {
    if (data.length() == 0 || data.equalsIgnoreCase("NONE")) {
      _data.color = "";          // 无目标，清缓存
      _data.colorValid = false;  // 避免保留上一帧颜色
    } else {
      _data.color = data;        // 记录颜色名
      _data.colorValid = true;   // 本帧有效
    }
  } else if (mode.equalsIgnoreCase("QR")) {
    _data.qr = data;             // 整条存储
    _data.qrValid = true;
  } else if (mode.equalsIgnoreCase("CARD")) {
    _data.card = data;           // 整条存储
    _data.cardValid = true;
  } else if (mode.equalsIgnoreCase("LINE")) {
    int firstComma = data.indexOf(',');            // 格式：偏差,角度,有效位
    if (firstComma != -1) {
      int secondComma = data.indexOf(',', firstComma + 1);
      if (secondComma != -1) {
        _data.lineOffset = data.substring(0, firstComma).toInt();              // 偏差
        _data.lineAngle  = data.substring(firstComma + 1, secondComma).toInt();// 角度
        _data.lineValid  = data.substring(secondComma + 1).toInt();            // 有效位
      } else {
        Serial.println("LINE 数据缺少第三段");
      }
    }
  } else {
    Serial.print("未知回传模式：");
    Serial.println(mode);        // 打印未知模式
  }
}

// ===== 读串口 =====
void AiCam::readSerial() {
  while (_serial->available()) {         // 读完缓冲区
    char c = (char)_serial->read();      // 取一字节

    if (c == '\r' || c == '\n') {        // 行结束
      if (_rxLine.length() > 0) {        // 跳过空行
        String mode, data;
        if (parseLine(_rxLine, mode, data)) {
          updateData(mode, data);        // 解析成功才写缓存
        }
        _rxLine = "";                    // 清缓冲
      }
    } else {
      _rxLine += c;                      // 累积字符
    }
  }
}

// ===== 取值接口 =====
String AiCam::getColor()  { return _data.color; }
bool AiCam::isColorValid() { return _data.colorValid; }

void AiCam::clearColor() {
  _data.color = "";          // 清内容
  _data.colorValid = false;  // 置无效
}

// 清空串口积压数据，避免动作结束后读到过期帧
void AiCam::flushInput() {
  while (_serial->available()) {
    _serial->read();  // 逐字节丢弃
  }
  _rxLine = "";        // 丢弃半个行
}

int AiCam::getFaceX()     { return _data.faceX; }
int AiCam::getFaceY()     { return _data.faceY; }
bool AiCam::isFaceValid() { return _data.faceValid; }

String AiCam::getQrCode() { return _data.qr; }
String AiCam::getCard()   { return _data.card; }

int AiCam::getLineOffset() { return _data.lineOffset; }
int AiCam::getLineAngle()  { return _data.lineAngle; }
int AiCam::getLineValid()  { return _data.lineValid; }
