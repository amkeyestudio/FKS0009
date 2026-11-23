#include <Wire.h>
#include <PCF8574.h>

PCF8574 pcf8574(0x20);

#define OUTA P4  // 最左
#define OUTB P0  // 左
#define OUTC P1  // 中
#define OUTD P2  // 右
#define OUTE P3  // 最右

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  pcf8574.begin();
  pcf8574.pinMode(OUTA, INPUT);
  pcf8574.pinMode(OUTB, INPUT);
  pcf8574.pinMode(OUTC, INPUT);
  pcf8574.pinMode(OUTD, INPUT);
  pcf8574.pinMode(OUTE, INPUT);

  delay(1000);
}

void loop() {
  PCF8574::DigitalInput val = pcf8574.digitalReadAll();

  int A = !val.p4; // 最左
  int B = !val.p0; // 左
  int C = !val.p1; // 中
  int D = !val.p2; // 右
  int E = !val.p3; // 最右

  Serial.printf("A:%d  B:%d  C:%d  D:%d  E:%d\n", A, B, C, D, E);

  delay(200);
}