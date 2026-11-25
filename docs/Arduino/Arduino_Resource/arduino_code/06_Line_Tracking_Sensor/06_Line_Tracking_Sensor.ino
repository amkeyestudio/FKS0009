#include <Wire.h>
#include <PCF8574.h>

PCF8574 pcf8574(0x20);

#define OUTA P4  // Far left
#define OUTB P0  // Left
#define OUTC P1  // Center
#define OUTD P2  // Right
#define OUTE P3  // Far right

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

  int A = !val.p4; // Far left
  int B = !val.p0; // Left
  int C = !val.p1; // Center
  int D = !val.p2; // Right
  int E = !val.p3; // Far right

  Serial.printf("A:%d  B:%d  C:%d  D:%d  E:%d\n", A, B, C, D, E);

  delay(200);
}
