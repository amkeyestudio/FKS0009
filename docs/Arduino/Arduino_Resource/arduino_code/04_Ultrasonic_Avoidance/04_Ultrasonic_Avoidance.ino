#include <ESP32Servo.h>  // Servo library

Servo myServo;  // Pan-tilt servo

// ===== Dot matrix display pins =====
#define CLK_Pin 9   // Clock, connect to dot matrix CLK
#define DIN_Pin 8   // Data, connect to dot matrix DIN

// ===== Ultrasonic module pins =====
const int trigPin = 13;  // Trigger, IO13
const int echoPin = 12;  // Echo, IO12

// Servo signal, GPIO42
const int servoPin = 42;

// ===== Distance thresholds =====
// Less than 15 cm counts as an obstacle; stop and measure left/right
const long closeDistance = 15;

long rightDistance = 0;  // Right-side distance
long leftDistance = 0;   // Left-side distance

// ===== Motor pins =====
#define MOTOR_AIN 38  // Left motor direction
#define MOTOR_AEN 21  // Left motor speed control
#define MOTOR_BIN 40  // Right motor direction
#define MOTOR_BEN 41  // Right motor speed control

// ===== Font bitmaps (16 columns x 8 rows) =====
// Heart: forward
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// Exclamation mark: obstacle ahead
unsigned char alert_sign[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f,
  0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Look left: measure distance turning left
unsigned char look_left[] = {
  0x00, 0x00, 0x3c, 0x4e, 0x4e, 0x42, 0x3c, 0x00,
  0x00, 0x3c, 0x4e, 0x4e, 0x42, 0x3c, 0x00, 0x00
};

// Look right: measure distance turning right
unsigned char look_right[] = {
  0x00, 0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x3c, 0x00,
  0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x3c, 0x00, 0x00
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

void setup() {
  Serial.begin(115200);
  Serial.println("Ultrasonic obstacle avoidance started");

  // Initialize dot matrix pins and clear display
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  pinMode(trigPin, OUTPUT);  // Trig output
  pinMode(echoPin, INPUT);   // Echo input

  // Direction pins as output
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);

  // Speed pins PWM: 1 kHz, 8-bit
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  // Stop on power on
  stopCar();

  // Dedicated servo timer to avoid conflict with motor PWM
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);  // Servo 50 Hz
  // Pulse width 500~2400 µs corresponds to 0~180° (SG90 type)
  // For larger servos like MG995, just use attach(servoPin)
  myServo.attach(servoPin, 500, 2400);

  // Return to center and show the idle pattern
  myServo.write(90);
  matrix_display(heart);
  delay(1000);
}

void loop() {
  // Measure the distance ahead
  long distance = getDistance();

  Serial.print("Distance ahead: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance <= closeDistance) {
    // Obstacle: stop and show the exclamation mark
    stopCar();
    matrix_display(alert_sign);
    delay(150);

    // Turn pan-tilt to one side and measure that side
    matrix_display(look_left);
    myServo.write(180);            // Pan-tilt turns to 180°
    delay(300);                    // Wait for the servo to reach position
    rightDistance = getDistance();
    delay(100);

    // Turn pan-tilt to the other side and measure that side
    matrix_display(look_right);
    myServo.write(0);              // Pan-tilt turns to 0°
    delay(300);
    leftDistance = getDistance();
    delay(100);

    myServo.write(90);             // Return to face forward

    // Turn toward the side with more space
    if (rightDistance > leftDistance) {
      spinRight(255);
    } else {
      spinLeft(255);
    }
    delay(500);

  } else {
    // No obstacle, keep moving forward
    goForward(150);
  }

  delay(100);
}

// Measure distance, return centimeters
int getDistance() {
  long duration;  // Round-trip time (µs)
  int distance;   // Distance (cm)

  // Pull low first for a clean start level
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // 10 µs high pulse triggers emission
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Echo HIGH pulse width is the round-trip time
  duration = pulseIn(echoPin, HIGH);

  // Speed of sound 0.034 cm/µs, divide by 2 to remove the return trip
  distance = duration * 0.034 / 2;

  // Set 0 if outside 2~300 cm
  if (distance < 2 || distance > 300) {
    distance = 0;
  }

  return distance;
}

// ===== Motor control =====
// Signed speed: forward / reverse / 0 stop
void driveMotors(int rightSpeed, int leftSpeed) {
  rightSpeed  = constrain(rightSpeed,  -255, 255);  // Clamp to ±255
  leftSpeed = constrain(leftSpeed, -255, 255);

  // Right wheel (channel B)
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);  // Direction is set by the direction pin; here only the duty cycle is given
  }

  // Left wheel (channel A)
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

// Each action is bound to a dot matrix pattern
void goForward(int speed)  {
  matrix_display(heart);        // Forward: heart
  driveMotors(speed, speed);
}

void goBackward(int speed) {
  matrix_display(arrow_down);   // Backward: down arrow
  driveMotors(-speed, -speed);
}

void spinRight(int speed)  {
  matrix_display(arrow_left);   // Spot turn right, dot matrix shows left arrow
  driveMotors(-speed, speed);
}

void spinLeft(int speed)   {
  matrix_display(arrow_right);  // Spot turn left, dot matrix shows right arrow
  driveMotors(speed, -speed);
}

void stopCar() {
  driveMotors(0, 0);            // Stop: duty cycle set to zero
}

// ===== Dot matrix driver (software I2C) =====
// Write the 16-byte font bitmap to the dot matrix display buffer
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

  // 0x8A: turn on display, maximum brightness
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// Start condition: DIN falling edge while CLK is HIGH
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// Send 1 byte, LSB first: set data when CLK is LOW, sample on rising edge
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

// Stop condition: DIN rising edge while CLK is HIGH
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