### 项目 4：超声波避障

#### **1. 简介：**

![扫地机器人](media/intro_04.jpg){width=520px}

超声波当眼睛、舵机当脖子、电机当腿、点阵当脸——四个部件第一次一起上场：车自己看路，自己决定往哪边绕。

判断只有一条线：前方 15 厘米以内算障碍。越过这条线，车不会硬顶，而是停下来，把云台转到两侧各量一次，哪边空就往哪边走。两次测量之间留 300 毫秒，让舵机站稳，量出来的数才作数。

八个动作一环扣一环：直行、测距、停车、左看、右看、比较、转向、回中。顺序理清楚，后面写起来就顺。

立在车前的一本书就够试：前方空着时车直行、点阵是爱心；推到离书 15 厘米以内，它停下、感叹号闪一下，云台先转向一侧、点阵换成向左看的图标，量完再转另一侧、换成向右看，两边都比过，朝空间大的那边转半秒，回中，继续往前走。把书挪到车的另一侧，它就会朝反方向绕。

#### **2. 所需清单：**

本课用到的器件：

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_05.png){width=170px}<br><span style="font-size:0.85em">超声波模块（带外壳） ×1</span> | ![](../media/kit_13.png){width=170px}<br><span style="font-size:0.85em">9G 舵机（180°，配十字臂） ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> |
| ![](../media/kit_02.png){width=170px}<br><span style="font-size:0.85em">履带底盘总成 ×1</span> | ![](../media/kit_06.png){width=170px}<br><span style="font-size:0.85em">8×16 LED 点阵屏 ×1</span> | ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> |
> ⚠️ **这些器件在第 2 章已经装好在车上，本表只作对照，不用拆下来重装；下面的接线说明只标清接线方式，供对照，不必拆装。完整物料见 1.4 套件清单。**
#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 超声波 | Trig = IO13，Echo = IO12 |
| 云台舵机 | IO42，50 Hz，脉宽 500–2400 µs 对应 0–180° |
| 障碍阈值 | closeDistance = 15 cm（≤ 15 cm 判为有障碍） |
| 左/右测距角度 | 左 180°、右 0°，测完回中 90° |
| 舵机到位延时 | 每次转向后 300 ms |
| 直行速度 | goForward(150) |
| 转向速度 | spinLeft / spinRight(255)，持续 500 ms |
| 点阵引脚 | CLK = IO9，DIN = IO8 |
| 调试串口 | 115200 |

#### **4. 接线说明：**

| 模块 | 引脚 | 主控 GPIO |
| --- | --- | :---: |
| 超声波 | Trig / Echo | IO13 / IO12 |
| 云台舵机 | 信号 | IO42 |
| 左电机（A 路 AIN/AEN） | 方向 / 调速 | IO38 / IO21 |
| 右电机（B 路 BIN/BEN） | 方向 / 调速 | IO40 / IO41 |
| 点阵 | CLK / DIN | IO9 / IO8 |

> **注意**：接线图中的元器件只用于说明接线关系，实际接线时不需要把清单中带外壳的元器件拆开。

> **注意**：ESP32Servo 库用 `ESP32PWM::allocateTimer(0)` 为舵机单独分配定时器，避免与电机 PWM 冲突。

![](media/PRO4.png)

#### **5. 流程图：**

主循环只有一条判断链：先测前方，够远就走；不够远就停车、左右各看一眼，挑宽的那边转过去，再回到测距。

![](media/avoid_flow.png)

#### **6. 测试代码：**

```cpp
#include <ESP32Servo.h>  // 舵机库

Servo myServo;  // 云台舵机

// ===== 点阵屏引脚 =====
#define CLK_Pin 9   // 时钟，接点阵 CLK
#define DIN_Pin 8   // 数据，接点阵 DIN

// ===== 超声波引脚 =====
const int trigPin = 13;  // 触发，IO13
const int echoPin = 12;  // 回响，IO12

// 舵机信号，GPIO42
const int servoPin = 42;

// ===== 距离阈值 =====
// 小于 15 cm 视为障碍，停车后左右测距
const long closeDistance = 15;

long rightDistance = 0;  // 右侧距离
long leftDistance = 0;   // 左侧距离

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // 左电机方向
#define MOTOR_AEN 21  // 左电机调速
#define MOTOR_BIN 40  // 右电机方向
#define MOTOR_BEN 41  // 右电机调速

// ===== 字模（16 列 × 8 行）=====
// 爱心：直行
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
  0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

// 感叹号：前方有障碍
unsigned char alert_sign[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f,
  0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 向左看：左转测距
unsigned char look_left[] = {
  0x00, 0x00, 0x3c, 0x4e, 0x4e, 0x42, 0x3c, 0x00,
  0x00, 0x3c, 0x4e, 0x4e, 0x42, 0x3c, 0x00, 0x00
};

// 向右看：右转测距
unsigned char look_right[] = {
  0x00, 0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x3c, 0x00,
  0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x3c, 0x00, 0x00
};

// 向下箭头：后退
unsigned char arrow_down[] = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x7e, 0xfe,
  0xfe, 0x7e, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00
};

// 向左箭头：左转
unsigned char arrow_left[] = {
  0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0xff, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00
};

// 向右箭头：右转
unsigned char arrow_right[] = {
  0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18,
  0x18, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Serial.println("超声波避障启动");

  // 点阵引脚初始化并清屏
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);

  pinMode(trigPin, OUTPUT);  // Trig 输出
  pinMode(echoPin, INPUT);   // Echo 输入

  // 方向脚输出
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);

  // 调速脚 PWM：1 kHz，8 位
  ledcAttach(MOTOR_BEN, 1000, 8);
  ledcAttach(MOTOR_AEN, 1000, 8);

  // 上电停车
  stopCar();

  // 舵机独用定时器，避免与电机 PWM 冲突
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);  // 舵机 50 Hz
  // 脉宽 500~2400 µs 对应 0~180°（SG90 类）
  // 换 MG995 等大舵机时可直接 attach(servoPin)
  myServo.attach(servoPin, 500, 2400);

  // 回中位并显示待机图案
  myServo.write(90);
  matrix_display(heart);
  delay(1000);
}

void loop() {
  // 测前方距离
  long distance = getDistance();

  Serial.print("前方距离：");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance <= closeDistance) {
    // 有障碍：停车并显示感叹号
    stopCar();
    matrix_display(alert_sign);
    delay(150);

    // 云台转向一侧，测这一侧距离
    matrix_display(look_left);
    myServo.write(180);            // 云台转到 180°
    delay(300);                    // 等舵机到位
    rightDistance = getDistance();
    delay(100);

    // 云台转向另一侧，测另一侧距离
    matrix_display(look_right);
    myServo.write(0);              // 云台转到 0°
    delay(300);
    leftDistance = getDistance();
    delay(100);

    myServo.write(90);             // 回正前方

    // 向空间较大的一侧转向
    if (rightDistance > leftDistance) {
      spinRight(255);
    } else {
      spinLeft(255);
    }
    delay(500);

  } else {
    // 无障碍，继续直行
    goForward(150);
  }

  delay(100);
}

// 测距，返回厘米
int getDistance() {
  long duration;  // 往返时间（µs）
  int distance;   // 距离（cm）

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

  // 超出 2~300 cm 置 0
  if (distance < 2 || distance > 300) {
    distance = 0;
  }

  return distance;
}

// ===== 电机控制 =====
// 速度带符号：正转 / 反转 / 0 停
void driveMotors(int rightSpeed, int leftSpeed) {
  rightSpeed  = constrain(rightSpeed,  -255, 255);  // 限幅 ±255
  leftSpeed = constrain(leftSpeed, -255, 255);

  // 右轮（B 路）
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);
    ledcWrite(MOTOR_BEN, rightSpeed);
  } else {
    digitalWrite(MOTOR_BIN, LOW);
    ledcWrite(MOTOR_BEN, -rightSpeed);  // 方向由方向脚决定，此处只给占空比
  }

  // 左轮（A 路）
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);
    ledcWrite(MOTOR_AEN, leftSpeed);
  } else {
    digitalWrite(MOTOR_AIN, LOW);
    ledcWrite(MOTOR_AEN, -leftSpeed);
  }
}

// 动作与点阵图案绑定
void goForward(int speed)  {
  matrix_display(heart);        // 前进：爱心
  driveMotors(speed, speed);
}

void goBackward(int speed) {
  matrix_display(arrow_down);   // 后退：向下箭头
  driveMotors(-speed, -speed);
}

void spinRight(int speed)  {
  matrix_display(arrow_left);   // 原地右转，点阵显示左箭头
  driveMotors(-speed, speed);
}

void spinLeft(int speed)   {
  matrix_display(arrow_right);  // 原地左转，点阵显示右箭头
  driveMotors(speed, -speed);
}

void stopCar() {
  driveMotors(0, 0);            // 停：占空比归零
}

// ===== 点阵驱动（软件模拟 I2C）=====
// 将 16 字节字模写入点阵屏显存
void matrix_display(unsigned char matrix_value[]) {
  // 0x40：写数据模式，地址自增
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0：显存起始地址 0
  IIC_start();
  IIC_send(0xC0);

  // 连写 16 列
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  // 0x8A：开显示，亮度最大
  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

// 起始条件：CLK 高电平期间 DIN 下降沿
void IIC_start() {
  digitalWrite(CLK_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(DIN_Pin, LOW);
  delayMicroseconds(3);
}

// 发送 1 字节，低位在前：CLK 低时置数据，上升沿采样
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

// 停止条件：CLK 高电平期间 DIN 上升沿
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
```

#### **7. 代码解释：**

**(1) 舵机对象和它独占的引脚**

```cpp
#include <ESP32Servo.h>  // 舵机库

Servo myServo;  // 云台舵机

const int servoPin = 42;
```

`Servo myServo;` 声明一个舵机对象，后面所有动作都通过它发。ESP32 没有 Arduino Uno 那种硬件 Servo 定时器，必须用 `ESP32Servo.h`，装的是 `Servo.h` 编译会直接报错。

**(2) 一条阈值，两个存距离的变量**

```cpp
const long closeDistance = 15;

long rightDistance = 0;  // 右侧距离
long leftDistance = 0;   // 左侧距离
```

`closeDistance = 15` 是这课唯一的判断线：小于等于 15 cm 就进避障分支，否则直行。第 5 课的跟随车会用四档阈值，这课先保持"要么走要么躲"的简单模型。

两个距离变量用 `long` 而不是 `int`，是为了和 `getDistance()` 内部的中间量对齐——那里的 `duration` 是 `long`，统一用宽的类型省得来回转换。

**(3) `getDistance()`：把第 3 课的测距包成函数**

```cpp
int getDistance() {
  long duration;  // 往返时间（µs）
  int distance;   // 距离（cm）
```

函数内部和上一课一模一样：拉低 → 10 µs 脉冲 → `pulseIn()` → ×0.034÷2 → 超量程置 0，区别只在最后 `return distance;`。

抽成函数是因为这课要**连测三次**（前、左、右）。写成函数，主循环里三行调用就够；摊开写就是三份一模一样的代码，改一处容易漏另外两处。`duration` 和 `distance` 定义在函数内部，每次调用都是新的，不会互相污染。

**(4) setup 里的舵机初始化**

```cpp
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);  // 舵机 50 Hz
  myServo.attach(servoPin, 500, 2400);
```

`allocateTimer(0)` 给舵机单独分一个硬件定时器——电机已经占了 LEDC 通道，不分开会抢资源，表现是舵机抖动或者电机转速乱跳。`setPeriodHertz(50)` 是舵机的标准周期：20 ms 一帧，脉宽决定角度。

`attach(pin, 500, 2400)` 把脉宽范围定在 500～2400 µs。SG90 这类小舵机常见范围是 500～2400 或 500～2500，写成 500～2400 转不到极限角度，但也不容易顶到机械限位扫齿。换 MG995 这类大舵机时可以直接 `attach(servoPin)` 用库的默认值。

**(5) loop 的骨架：先测一次，再分两条路**

```cpp
void loop() {
  long distance = getDistance();

  Serial.print("前方距离：");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance <= closeDistance) {
```

整课的逻辑就这一个分叉：距离大于 15 cm 走 `else` 里的 `goForward(150)`，小于等于 15 cm 进避障分支。`delay(100)` 在 `loop()` 末尾，所以每轮至少 100 ms，不会把串口刷爆。

这里的 0 有两种来源：前面真有东西贴脸，或者前面太空旷没回波（第 3 课定下"超量程返回 0"的约定）。两种都满足 `<= 15`，都会让车停下来东张西望。这不是 bug，是超声波本身分不清——真想区分得另加判断（比如记录 `pulseIn()` 有没有超时），本套教程不展开。

**(6) 避障分支：停车、左右各看一次**

```cpp
    stopCar();
    matrix_display(alert_sign);
    delay(150);

    matrix_display(look_left);
    myServo.write(180);            // 云台转到 180°
    delay(300);                    // 等舵机到位
    rightDistance = getDistance();
    delay(100);

    matrix_display(look_right);
    myServo.write(0);              // 云台转到 0°
    delay(300);
    leftDistance = getDistance();
    delay(100);

    myServo.write(90);             // 回正前方
```

先停车、点阵换感叹号、等 150 ms 停稳——车有惯性，不停稳就转头，测到的方向是斜的。

`write(180)` 这一段配套的图案是 `look_left`（向左看），存进去的变量却叫 `rightDistance`；后面 `write(0)` 配 `look_right`，变量叫 `leftDistance`。**两个名字里有一个是反的**，报错不会有，但会影响下面选边那句判断。想确认把车架空、手动 `write(180)` 看一眼云台朝哪边，比读代码快。

两个 `delay(300)` 不能省：`myServo.write()` 只是把目标角度告诉舵机，舵机自己要转过去——9G 舵机转 180° 大约 0.3 s。不等它到位就测距，量到的还是上一个方向的距离。

后面那两个 `delay(100)` 是第二次等待：舵机停稳之后，上一发超声波在腔体里还有余响，留一点时间让声波散掉，否则读数会偏小。

**(7) 选边：往空间大的那边转**

```cpp
    if (rightDistance > leftDistance) {
      spinRight(255);
    } else {
      spinLeft(255);
    }
    delay(500);
```

比较两侧读数，往大的那边转，速度给满 255。平手时走 `else`——往左转。这不是笔误，是给"两边都堵死"留一个确定的行为，总比原地不动强。

`delay(500)` 是转动的持续时间。代码里没有编码器，转多少度只能靠"速度 × 时间"估：500 ms 满速实测约 60～90°，电量下降、地面变滑时同样的时间转出来的角度会变小。这是本课唯一能调的量，转不够就加时间。

#### **8. 运行现象：**

上电后云台回中、点阵显示爱心，小车直行，串口每轮打印**前方距离：XX cm**：

1. **无障碍**（距离 > 15 cm）：保持直行，点阵爱心。
2. **遇到障碍**（距离 ≤ 15 cm）：停车，点阵感叹号闪一下；云台先转向一侧、点阵显示**向左看**图标并测距，再转向另一侧、显示**向右看**图标并测距；比较两侧距离，朝空间更大的一侧转 500 ms，随后回中继续直行。

小车能绕开前方障碍、不会一直顶着障碍物，点阵状态跟着动作换，就跑对了。

> 【图片待补】运行现象实拍：直行、遇障停车、左看测距、右看测距、转向绕行五个状态各拍一张。

> 【图片待补】串口距离输出截图，展示遇障前后的数值变化。

#### **9. 常见问题解决：**

(1) 前方明明空着，车却停车、亮感叹号、左右张望：`getDistance()` 超量程时返回 0，而 0 也满足 `<= 15 cm`，被当成了障碍。把判断改成 `distance > 0 && distance <= closeDistance`，把 0 挡在外面。

(2) 云台转过去了，测的却不是正侧方：`write(180)` 之后没等舵机到位就测距。那两个 `delay(300)` 不能省，等云台停稳再读，方向才对得上。

(3) 直行时车头一直歪着、超声波斜着看：测完左右两侧忘了 `write(90)` 回中，把这句补在比较完距离之后。

(4) 左右判断总是反的：`look_left` 图案配的是 `rightDistance`，两个名字里有一个是反的。把车架空、手动 `write(180)` 看一眼云台朝哪边，比读代码快。
