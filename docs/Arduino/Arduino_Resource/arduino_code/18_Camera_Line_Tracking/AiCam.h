#ifndef AICAM_H
#define AICAM_H

#include <Arduino.h>

// ===== Serial parameters =====
// Module fixed at 9600 8N1
#define CAM_BAUD 9600

// ===== Data buffer =====
// The module processes images internally and reports results over serial only
struct CamData {
  int faceX = 0;            // Face center X
  int faceY = 0;            // Face center Y
  bool faceValid = false;   // Detected in this frame

  String color;             // Color name, e.g. RED
  bool colorValid = false;  // Recognized in this frame

  String qr;                // QR code content
  bool qrValid = false;     // Recognized in this frame

  String card;              // Task card ID
  bool cardValid = false;   // Recognized in this frame

  int lineOffset = 0;       // Offset: negative left, positive right
  int lineAngle = 0;        // Angle: negative left tilt, positive right tilt
  int lineValid = 0;        // Valid flag, 5 means line recognized
};

// ===== Driver class =====
// Wraps serial I/O and parsing so application code need not handle serial details
class AiCam {
public:
  AiCam(uint8_t rxPin, uint8_t txPin);  // Controller RX/TX pins

  void begin();                          // Start serial, called once from setup()
  void sendCmd(const String &cmd);       // Send a command
  void setMode(const String &mode);      // Switch mode: face / color / qr / card / line
  void readSerial();                     // Read reports, called each loop()

  String getColor();                     // Get recognized color name
  bool isColorValid();                   // Recognized in this frame
  void clearColor();                     // Clear color buffer
  void flushInput();                     // Clear serial backlog

  int getFaceX();                        // Get face center X
  int getFaceY();                        // Get face center Y
  bool isFaceValid();                    // Detected in this frame

  String getQrCode();                    // Get QR code content
  String getCard();                      // Get task card ID

  int getLineOffset();                   // Get line tracking offset
  int getLineAngle();                    // Get line tracking angle
  int getLineValid();                    // Get line tracking valid flag

private:
  uint8_t _rxPin;                        // Controller receive pin
  uint8_t _txPin;                        // Controller transmit pin
  HardwareSerial *_serial;               // Hardware serial, points to Serial2

  CamData _data;                         // Data buffer
  String _rxLine;                        // Receive buffer, accumulated by line

  bool parseLine(const String &line, String &mode, String &data);  // Split into mode and data
  void updateData(const String &mode, const String &data);         // Write to buffer by mode
};

#endif
