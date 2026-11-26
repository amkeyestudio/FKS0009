// ===== Dot matrix display pins =====
#define CLK_Pin 9   // Clock, connect to dot matrix CLK
#define DIN_Pin 8   // Data, connect to dot matrix DIN

// ===== Ultrasonic pins =====
const int trigPin = 13;  // Trigger pin, connected to IO13
const int echoPin = 12;  // Echo pin, connected to IO12

// ===== Distance ranges (cm) =====
// Following is divided into ranges by three thresholds
const long safeDistance = 30;   // 30-50: following
const long closeDistance = 10;  // Less than 10: back up
const long maxDistance = 50;    // Greater than 50: target lost

// ===== Motor pins =====
#define MOTOR_AIN 38  // Left motor direction
#define MOTOR_AEN 21  // Left motor speed
#define MOTOR_BIN 40  // Right motor direction
#define MOTOR_BEN 41  // Right motor speed

// ===== Font bitmaps (16 columns x 8 rows) =====
// Heart: follow forward
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// Smiley: distance OK, stop
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Down arrow: too close, back up
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7e, 0xfe,
  0xfe, 0x7e, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// Question mark: target lost
unsigned char question_mark[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x51,
  0x59, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Serial.println("Ultrasonic following started");

  // Initialize dot matrix pins and clear the display
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  pinMode(trigPin, OUTPUT);  // Trig output
  pinMode(echoPin, INPUT);   // Echo input

  // Direction pin output
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);

  // Speed pin PWM: 1 kHz, 8-bit
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  // Stop and show smiley on power on
  stopMotor();
  matrix_display(smile);
}

void loop() {
  // Measure the distance ahead
  long distance = getDistance();

  Serial.print("Current distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > safeDistance && distance <= maxDistance) {
    // 30-50 cm: follow forward
    forward(150, 150);

  } else if (distance >= closeDistance && distance <= safeDistance) {
    // 10-30 cm: stop
    stopMotor();
    matrix_display(smile);

  } else if (distance < closeDistance && distance > 0) {
    // Less than 10 cm: back up
    // Differential left/right speed; the car drifts slightly when backing up
    back(105, 150);

  } else {
    // Greater than 50 cm or invalid reading: stop and show a question mark
    stopMotor();
    matrix_display(question_mark);
  }

  // Measure once every 100 ms
  delay(100);
}

// Measure distance, return centimeters
int getDistance() {
  long duration;  // Round-trip time (µs)
  int distance;   // Distance (cm)

  // Pull low first for a clean start level
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // 10 µs HIGH pulse triggers the wave
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Echo HIGH width is the round-trip time
  duration = pulseIn(echoPin, HIGH);

  // Speed of sound 0.034 cm/µs, divide by 2 to remove the return trip
  distance = duration * 0.034 / 2;

  // Out of 2-300 cm range, set to 0
  if (distance < 2 || distance > 300) {
    distance = 0;
  }

  return distance;
}

// ===== Motor control =====

// Directly write left/right wheel PWM (0-255)
void setMotor(int rightSpeed, int leftSpeed) {
  ledcWrite(MOTOR_BEN, rightSpeed);
  ledcWrite(MOTOR_AEN, leftSpeed);
}

// Forward: direction pin HIGH and show heart
void forward(int rightSpeed, int leftSpeed) {
  matrix_display(heart);
  digitalWrite(MOTOR_BIN, HIGH);
  digitalWrite(MOTOR_AIN, HIGH);
  setMotor(rightSpeed, leftSpeed);
}

// Backward: direction pin LOW and show down arrow
void back(int rightSpeed, int leftSpeed) {
  matrix_display(arrow_down);
  digitalWrite(MOTOR_BIN, LOW);
  digitalWrite(MOTOR_AIN, LOW);
  setMotor(rightSpeed, leftSpeed);
}

// Stop: set speed to 0
void stopMotor() {
  setMotor(0, 0);
}

// ===== Dot matrix driver (software-simulated I2C) =====

// Write the 16-byte font bitmap into the dot matrix display buffer
void matrix_display(unsigned char matrix_value[]) {
  // 0x40: write data mode, auto-increment address
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0: display buffer start address 0
  IIC_start();
  IIC_send(0xC0);

  // Write 16 columns consecutively
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8A: display on, maximum brightness
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// Start condition: falling edge of DIN while CLK is HIGH
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// Send one byte, LSB first: set data while CLK is LOW, sample on rising edge
void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(3);

    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(3);

    send_data = send_data >> 1;
  }
}

// Stop condition: rising edge of DIN while CLK is HIGH
void IIC_end() {
  digitalWrite(CLK_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
}
