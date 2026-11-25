// Ultrasonic module pins
const int trigPin = 13;  // Trigger, IO13
const int echoPin = 12;  // Echo, IO12

// Measurement variables
long duration;   // Round-trip time (µs)
int distance;    // Distance (cm)

void setup() {
  // Baud rate 115200
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT); // Trig output
  pinMode(echoPin, INPUT);  // Echo input
}

void loop() {
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
    
  // Measuring range 2~300 cm, set 0 if out of range
  if(distance < 2 || distance > 300){
    distance = 0;
  }

  // Serial output
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Measure once per 1 s
  delay(1000);
}
