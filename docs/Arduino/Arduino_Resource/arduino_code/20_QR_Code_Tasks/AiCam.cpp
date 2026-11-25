#include "AiCam.h"

// ===== Constructor and initialization =====
AiCam::AiCam(uint8_t rxPin, uint8_t txPin) : _rxPin(rxPin), _txPin(txPin) {
  _serial = &Serial2;  // Use hardware serial 2, independent of the USB download port
}

// Start serial, called once from setup()
void AiCam::begin() {
  _serial->begin(CAM_BAUD, SERIAL_8N1, _rxPin, _txPin);  // 9600 8N1, specify RX/TX pins
  delay(500);                                            // Wait for module power on to stabilize

  Serial.println("Vision module serial started");
  sendCmd("GET:MODE");    // Query current mode
  delay(50);
  sendCmd("GET:OUTPUT");  // Query output format
  delay(50);
}

// ===== Send commands =====
void AiCam::sendCmd(const String &cmd) {
  if (cmd.length() == 0) return;  // Do not send empty commands
  _serial->print(cmd);            // Command content
  _serial->print("\r\n");         // End with CRLF
  Serial.print("Send -> ");
  Serial.println(cmd);            // Also print to USB serial
}

// ===== Switch mode =====
void AiCam::setMode(const String &mode) {
  String m = mode;       // Work on a copy
  m.trim();              // Remove leading/trailing spaces
  m.toLowerCase();       // Normalize to lowercase
  _curMode = m;          // Record mode for bare-data parsing

  if (m == "face") {
    sendCmd("MODE:FACE");    // Face detection
  } else if (m == "color") {
    sendCmd("MODE:COLOR");   // Color recognition
  } else if (m == "qr") {
    sendCmd("MODE:QR");      // QR code
  } else if (m == "card") {
    sendCmd("MODE:CARD");    // Task card
  } else if (m == "line") {
    sendCmd("MODE:LINE");    // Camera line tracking
  } else {
    Serial.print("Unknown mode: ");
    Serial.println(mode);    // Print mode name
  }
}

// ===== Parse frames =====
// Compatible with both "QR:FORWARD" and "AICAM:QR:FORWARD" formats
bool AiCam::parseLine(const String &line, String &mode, String &data) {
  int first = line.indexOf(':');        // First colon
  if (first == -1) return false;        // No colon, invalid frame

  int second = line.indexOf(':', first + 1);  // Find second colon
  if (second == -1) {
    mode = line.substring(0, first);          // One colon: mode before it
    data = line.substring(first + 1);         // Data after it
  } else {
    mode = line.substring(first + 1, second); // Two colons: mode in the middle
    data = line.substring(second + 1);        // Data at the end
  }
  return true;
}

// ===== Update buffer =====
void AiCam::updateData(const String &mode, const String &data) {
  if (mode.equalsIgnoreCase("FACE")) {
    int comma = data.indexOf(',');          // Format: x,y
    if (comma != -1) {
      _data.faceX = data.substring(0, comma).toInt();      // X
      _data.faceY = data.substring(comma + 1).toInt();     // Y
      _data.faceValid = (_data.faceX > 1);                 // X>1 means detected
    }
  } else if (mode.equalsIgnoreCase("COLOR")) {
    if (data.length() == 0 || data.equalsIgnoreCase("NONE")) {
      _data.color = "";          // No target, clear buffer
      _data.colorValid = false;  // Avoid keeping previous frame color
    } else {
      _data.color = data;        // Store color name
      _data.colorValid = true;   // Valid this frame
    }
  } else if (mode.equalsIgnoreCase("QR")) {
    String v = data;      // Work on a copy
    v.trim();             // Remove leading/trailing whitespace

// Clear buffer when empty, otherwise the same task would trigger repeatedly
    if (v.length() == 0 || v.equalsIgnoreCase("NONE")) {
      _data.qr = "";          // Clear content
      _data.qrValid = false;  // Mark invalid
    } else {
      _data.qr = v;           // Store content
      _data.qrValid = true;
    }
  } else if (mode.equalsIgnoreCase("CARD")) {
    _data.card = data;           // Store the whole line
    _data.cardValid = true;
  } else if (mode.equalsIgnoreCase("LINE")) {
    int firstComma = data.indexOf(',');            // Format: offset,angle,valid flag
    if (firstComma != -1) {
      int secondComma = data.indexOf(',', firstComma + 1);
      if (secondComma != -1) {
        _data.lineOffset = data.substring(0, firstComma).toInt();              // Offset
        _data.lineAngle  = data.substring(firstComma + 1, secondComma).toInt();// Angle
        _data.lineValid  = data.substring(secondComma + 1).toInt();            // Valid flag
      } else {
        Serial.println("LINE data missing the third field");
      }
    }
  } else {
    Serial.print("Unknown report mode: ");
    Serial.println(mode);        // Print unknown mode
  }
}

// ===== Read serial =====
// Accumulate a line byte by byte, parse on \r or \n; must be called every loop or the buffer overflows and drops data
void AiCam::readSerial() {
  while (_serial->available()) {         // Drain the buffer
    char c = (char)_serial->read();      // Read one byte

    if (c == '\r' || c == '\n') {        // End of line
      if (_rxLine.length() > 0) {        // Skip empty lines
        String mode, data;
        if (parseLine(_rxLine, mode, data)) {
          updateData(mode, data);        // Only update buffer on successful parse
        } else if (_curMode == "qr") {
// QR mode also accepts bare data without a prefix
          _data.qr = _rxLine;
          _data.qrValid = true;
        }
        _rxLine = "";                    // Clear buffer
      }
    } else {
      _rxLine += c;                      // Accumulate characters
    }
  }
}

// ===== Getter interface =====
String AiCam::getColor()  { return _data.color; }
bool AiCam::isColorValid() { return _data.colorValid; }

void AiCam::clearColor() {
  _data.color = "";          // Clear content
  _data.colorValid = false;  // Mark invalid
}

// Discard stale data queued in the serial buffer
// While the car moves (grab, turn, deliver) serial is not read, so seconds of old frames pile up in the buffer,
// reading them afterward re-emits old color events and the car grabs at empty space again
void AiCam::flushInput() {
  while (_serial->available()) {
    _serial->read();  // Discard byte by byte
  }
  _rxLine = "";        // Discard partial line
}

int AiCam::getFaceX()     { return _data.faceX; }
int AiCam::getFaceY()     { return _data.faceY; }
bool AiCam::isFaceValid() { return _data.faceValid; }

String AiCam::getQrCode() { return _data.qr; }
String AiCam::getCard()   { return _data.card; }

void AiCam::clearQr() {
  _data.qr = "";          // Clear content
  _data.qrValid = false;  // Mark invalid
}

int AiCam::getLineOffset() { return _data.lineOffset; }
int AiCam::getLineAngle()  { return _data.lineAngle; }
int AiCam::getLineValid()  { return _data.lineValid; }
