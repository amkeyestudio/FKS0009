#include <IRremote.hpp>  // Infrared (IR) decoding library

// IR receiver, IO48
#define IR_RECEIVE_PIN 48

// Remote control key codes
// Codes may vary by batch; check the Serial Monitor output
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for the serial port to stabilize

  Serial.println("IR reception experiment started; point the remote control at the front of the car");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  Serial.println("Waiting for a key press...");
}

void loop() {
  // A decodable frame was received
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    static bool lastHolding = false;  // Whether the previous frame was a long press, to avoid repeated printing

    if (holding) {
      // A long press periodically resends the same code
      if (!lastHolding) {
        Serial.println("(Long press detected; no more per-frame printing)");
      }
    } else {
      Serial.print("Protocol: ");
      Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
      Serial.print("   Key code: ");
      Serial.print(cmd);
      Serial.print("(0x");
      Serial.print(cmd, HEX);
      Serial.print(")   ");
      Serial.println(keyName(cmd));
    }
    lastHolding = holding;

    IrReceiver.resume();  // Must be called, otherwise only one frame can be received
  }
}

// Convert key codes to names
const char* keyName(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:    return "Up";
    case KEY_DOWN:  return "Down";
    case KEY_LEFT:  return "Left";
    case KEY_RIGHT: return "Right";
    case KEY_STOP:  return "OK / Stop";
    default:        return "";
  }
}
