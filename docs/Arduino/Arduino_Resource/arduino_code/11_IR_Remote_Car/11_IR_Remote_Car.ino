#include <IRremote.hpp>  // Infrared (IR) decoding library

// ===== Pins =====
#define IR_RECEIVE_PIN 48  // IR receiver, IO48

#define MOTOR_AIN 38  // Channel A direction
#define MOTOR_AEN 21  // Channel A speed control
#define MOTOR_BIN 40  // Channel B direction
#define MOTOR_BEN 41  // Channel B speed control

// ===== Speed =====
#define SPEED_RUN  180  // Straight travel
#define SPEED_TURN 180  // Spin in place

// ===== Remote control key values =====
// After changing the remote control, update these values to match the Serial Monitor output
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64

// Auto-stop after command timeout (ms) to avoid losing control if the remote disconnects
#define STOP_AFTER 1500

unsigned long lastKeyTime = 0;  // Timestamp of the last valid key press

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("IR remote control car started; control it with the direction pad");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  stopCar();  // Stop on power on
}

void loop() {
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

    if (holding) {
      // A long press only refreshes the timeout timer
      lastKeyTime = millis();
    } else if (isKnownKey(cmd)) {
      handleKey(cmd);
      lastKeyTime = millis();
    }

    IrReceiver.resume();
  }

  // Stop if no valid key press for more than 1.5 s
  if (millis() - lastKeyTime > STOP_AFTER) {
    stopCar();
  }
}

// Key handling
void handleKey(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:
      Serial.println("Forward");
      goForward(SPEED_RUN);
      break;
    case KEY_DOWN:
      Serial.println("Backward");
      goBackward(SPEED_RUN);
      break;
    case KEY_LEFT:
      Serial.println("Turn left");
      spinLeft(SPEED_TURN);
      break;
    case KEY_RIGHT:
      Serial.println("Turn right");
      spinRight(SPEED_TURN);
      break;
    case KEY_STOP:
      Serial.println("Stop");
      stopCar();
      break;
    default:
      break;
  }
}

// Whether the key is a defined key
bool isKnownKey(uint8_t cmd) {
  return cmd == KEY_UP || cmd == KEY_DOWN || cmd == KEY_LEFT ||
         cmd == KEY_RIGHT || cmd == KEY_STOP;
}

// ===== Motor driver =====
// Channel A drives the left wheel, channel B drives the right wheel, same as Lesson 7
// If left and right are reversed, swap the two sections below
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // Channel B: right wheel
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);
  }

  // Channel A: left wheel
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

void goForward(int speed)  { driveMotors(speed, speed); }    // Forward: both wheels rotate forward at the same speed
void goBackward(int speed) { driveMotors(-speed, -speed); }  // Backward: both wheels rotate in reverse at the same speed
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // Turn left: left wheel backward, right wheel forward
void spinRight(int speed)  { driveMotors(speed, -speed); }   // Turn right: left wheel forward, right wheel backward
void stopCar()             { driveMotors(0, 0); }            // Stop: duty cycle set to zero
