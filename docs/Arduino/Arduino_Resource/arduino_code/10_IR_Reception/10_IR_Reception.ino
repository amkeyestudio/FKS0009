#include <IRremote.hpp>  // 红外解码库

// 红外接收头，IO48
#define IR_RECEIVE_PIN 48

// 遥控器按键码值
// 批次不同码值可能不同，以串口打印为准
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64

void setup() {
  Serial.begin(115200);
  delay(1000);  // 等串口稳定

  Serial.println("红外接收实验开始，把遥控器对准车头");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  Serial.println("等待按键……");
}

void loop() {
  // 收到一帧可解码信号
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    static bool lastHolding = false;  // 上一帧是否长按，避免重复打印

    if (holding) {
      // 长按会周期性重发同一码
      if (!lastHolding) {
        Serial.println("（长按中，后面不再逐帧打印）");
      }
    } else {
      Serial.print("协议：");
      Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
      Serial.print("   按键码：");
      Serial.print(cmd);
      Serial.print("（0x");
      Serial.print(cmd, HEX);
      Serial.print("）   ");
      Serial.println(keyName(cmd));
    }
    lastHolding = holding;

    IrReceiver.resume();  // 必须调用，否则只能收到一帧
  }
}

// 按键码转中文名
const char* keyName(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:    return "上";
    case KEY_DOWN:  return "下";
    case KEY_LEFT:  return "左";
    case KEY_RIGHT: return "右";
    case KEY_STOP:  return "OK / 停止";
    default:        return "";
  }
}
