// 超声波引脚
const int trigPin = 13;  // 触发，IO13
const int echoPin = 12;  // 回响，IO12

// 测量中间量
long duration;   // 往返时间（µs）
int distance;    // 距离（cm）

void setup() {
  // 波特率 115200
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT); // Trig 输出
  pinMode(echoPin, INPUT);  // Echo 输入
}

void loop() {
  // 先拉低，保证起始电平干净
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // 10 µs 高脉冲触发发波
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Echo 高电平宽度即往返时间
  duration = pulseIn(echoPin, HIGH);

  // 声速 0.034 cm/µs，除以 2 去掉回程
  distance = duration * 0.034 / 2;
    
  // 量程 2~300 cm，超范围置 0
  if(distance < 2 || distance > 300){
    distance = 0;
  }

  // 串口输出
  Serial.print("距离：");
  Serial.print(distance);
  Serial.println(" cm");

  // 1 s 测一次
  delay(1000);
}
