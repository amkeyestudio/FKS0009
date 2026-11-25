// ===== Pins =====
// Dot matrix display
#define CLK_Pin 9   // Clock
#define DIN_Pin 8   // Data

// Motor driver
#define MOTOR_AIN 38  // Left motor direction
#define MOTOR_AEN 21  // Left motor speed control
#define MOTOR_BIN 40  // Right motor direction
#define MOTOR_BEN 41  // Right motor speed control

// ===== Speed parameters =====
#define SPEED_RUN  150  // Forward speed, 0~255
#define SPEED_TURN 150  // Spot-turn speed, 0~255

// ===== Dot matrix font bitmaps (16 columns x 8 rows) =====
// Heart: forward
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// Down arrow: backward
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7e, 0xfe,
  0xfe, 0x7e, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// Left arrow: turn left
unsigned char arrow_left[] = {
  0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0xff, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00
};

// Right arrow: turn right
unsigned char arrow_right[] = {
  0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00
};

// Smile: stopped/idle, scrolling during speed change
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40, 
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// ===== Dot matrix driver =====
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

void IIC_send(unsigned char send_data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLK_Pin, LOW);
    delayMicroseconds(3);
    digitalWrite(DIN_Pin, (send_data & 0x01) ? HIGH : LOW);
    delayMicroseconds(3);
    digitalWrite(CLK_Pin, HIGH);
    delayMicroseconds(3);
    send_data >>= 1;
  }
}

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

void matrix_display(unsigned char matrix_value[]) {
  IIC_start();
  IIC_send(0x40); // Write data mode, auto-increment address
  IIC_end();
  
  IIC_start();
  IIC_send(0xC0); // Start writing at address 0
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();
  
  IIC_start();
  IIC_send(0x8A); // Turn on display, maximum brightness
  IIC_end();
}

// Display the font bitmap shifted left by shift columns
void matrix_display_scroll(unsigned char base_pattern[], int shift) {
  unsigned char scroll_data[16];
  for (int i = 0; i < 16; i++) {
    scroll_data[i] = base_pattern[(i + shift) % 16];
  }
  matrix_display(scroll_data);
}

// ===== Motor driver =====
// Signed speed: forward / reverse / 0 stop
void driveMotors(int rightSpeed, int leftSpeed) {
  rightSpeed  = constrain(rightSpeed,  -255, 255);
  leftSpeed = constrain(leftSpeed, -255, 255);

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

// Each action also refreshes the dot matrix pattern
void goForward(int speed) {
  matrix_display(heart);        // Forward: heart
  driveMotors(speed, speed);
}

void goBackward(int speed) {
  matrix_display(arrow_down);   // Backward: down arrow
  driveMotors(-speed, -speed);
}

void spinRight(int speed) {
  matrix_display(arrow_left);   // Spot turn right, dot matrix shows left arrow
  driveMotors(-speed, speed);
}

void spinLeft(int speed) {
  matrix_display(arrow_right);  // Spot turn left, dot matrix shows right arrow
  driveMotors(speed, -speed);
}

void stopCar() {
  matrix_display(smile);        // Stop: smile
  driveMotors(0, 0);
}

// ===== Initialization =====
void setup() {
  Serial.begin(115200);

  // Clear dot matrix display on power on
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  // Direction pins as output, speed pins use PWM
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  pinMode(MOTOR_BEN, OUTPUT);
  pinMode(MOTOR_AEN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz, 8-bit
  ledcAttach(MOTOR_AEN, 1000, 8);

  // Stop and show smile on power on
  stopCar();
  Serial.println("Motor driving experiment starts, action in 1 second");
  delay(1000);
}

// ===== Main loop =====
void loop() {
  // Forward for 2 s
  Serial.println("Forward (show heart)");
  goForward(SPEED_RUN);
  delay(2000);

  stopCar();
  delay(500);

  // Backward for 2 s
  Serial.println("Backward (show down arrow)");
  goBackward(SPEED_RUN);
  delay(2000);

  stopCar();
  delay(500);

  // Spot turn right for 1.5 s
  Serial.println("Spot turn right");
  spinRight(SPEED_TURN);
  delay(1500);

  stopCar();
  delay(500);

  // Spot turn left for 1.5 s
  Serial.println("Spot turn left");
  spinLeft(SPEED_TURN);
  delay(1500);

  stopCar();
  delay(1000);

  // Speed control: duty cycle 0→255→0, dot matrix scrolls along
  int scroll_step = 0;

  Serial.println("Accelerate: duty cycle 0 -> 255");
  for (int duty = 0; duty <= 255; duty += 5) {
    driveMotors(duty, duty);
    if (duty % 20 == 0) {
      matrix_display_scroll(heart, scroll_step++);
    }
    delay(20);
  }

  Serial.println("Decelerate: duty cycle 255 -> 0");
  for (int duty = 255; duty >= 0; duty -= 5) {
    driveMotors(duty, duty);
    if (duty % 20 == 0) {
      matrix_display_scroll(heart, scroll_step++);
    }
    delay(20);
  }

  stopCar();
  Serial.println("Stop, next round in 2 seconds");
  delay(2000);
}
