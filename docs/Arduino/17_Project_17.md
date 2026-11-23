### 项目 17：AI 视觉模块：颜色追踪与抓取

#### **1. 简介：**

从这一课开始，车能「看见」颜色了。云台上那个小摄像头不是把画面传回来让主控算——它自己就是一台小电脑，图像识别在模块内部完成，串口上只回一个词：`RED` 或 `YELLOW`。

这个设计决定了整课代码的样子。主控不处理图像，只读一行字符串，所以你能用一块没有摄像头接口的开发板做视觉任务。代价是模块只会告诉你「看到了什么颜色」，不会告诉你「这个东西在哪」——除了人脸模式，其他模式都只回内容不回坐标。

本课的任务是：车前放一个红色或黄色物块，车低速靠近、夹起来、按颜色决定左转还是右转、送到对应区域放下、再倒回起点。整个流程八步，全是定长时间的动作，没有闭环控制。

![产线上的分拣机械臂](media/intro_17.jpg){width=520px}

#### **2. 所需清单：**

本课用到的器件：

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_09.jpg){width=170px}<br><span style="font-size:0.85em">TinyVision AI 视觉摄像头 ×1</span> | ![](../media/kit_13.png){width=170px}<br><span style="font-size:0.85em">9G 舵机（180°） ×2</span> | ![](../media/kit_08.png){width=170px}<br><span style="font-size:0.85em">大型机械夹（半成品） ×1</span> |
| ![](../media/kit_05.png){width=170px}<br><span style="font-size:0.85em">超声波模块（带外壳） ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> | ![](../media/kit_06.png){width=170px}<br><span style="font-size:0.85em">8×16 LED 点阵屏 ×1</span> |
| ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> |  |  |
#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 模块 | TinyVision AI 摄像头 |
| 通信 | 硬件串口 2，9600 8N1 |
| 主控 RX / TX | IO18 / IO17 |
| 识别模式 | `color` |
| 回传格式 | `COLOR:RED` / `COLOR:YELLOW` / `COLOR:NONE` |
| 支持颜色 | 红、黄（模块内置） |
| 接近速度 | 150 |
| 转向 / 运送速度 | 180 |
| 接近时长 | 350 ms |
| 转向时长 | 600 ms |
| 运送时长 | 800 ms |

#### **4. 接线说明：**

后四课的模块都装在云台上、都跟着舵机转，只换镜头朝向：第 19、20 课用装法 B，镜头水平朝前；第 17、18 课用装法 A，镜头朝地面。换课前按 2.2.9 重装一次模块即可，UART 线接在扩展板右侧排针上，朝向变了也不用重接。


| 视觉模块 | 主控 | 说明 |
| --- | :---: | --- |
| TX | IO18 | 模块发，主控收 |
| RX | IO17 | 主控发，模块收 |
| VCC | 5V | 模块供电 |
| GND | GND | 必须共地 |

**装法A：**

![](media/A-1.png)

![](media/A-2.png)

![](media/A-3.png)

![](media/A-4.png)

![](media/IMG_9076.jpg)

![](media/A-6.png)

TX 接 RX、RX 接 TX，接成 TX-TX 也能通，但模块收不到主控的指令，`setMode()` 全部失效。串口打印「发送 -> MODE:COLOR」却毫无反应，就是这里接错了。

#### **5. 流程图：**

识别到颜色后的八步动作是顺序执行的，去程五步、回程三步：

![](media/color_pick_flow.svg)

#### **6. 测试代码：**

> **本课工程有三个文件**：`17_Color_Tracking.ino`、`AiCam.h`、`AiCam.cpp`。视觉模块的串口收发和字符串解析都在 `AiCam.cpp` 里，三个文件放在同一个文件夹，少一个编译不过。

```cpp
#include <ESP32Servo.h>
#include "AiCam.h"  // 视觉模块驱动

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 舵机引脚 =====
#define CLAW_SERVO_PIN 11  // 夹爪舵机，第 9 课
#define PAN_SERVO_PIN  42  // 云台舵机，第 8 课

// ===== 视觉模块串口 =====
// 主控 RX←模块 TX，主控 TX→模块 RX，需共地
#define CAM_RX 18  // 主控接收脚
#define CAM_TX 17  // 主控发送脚

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 速度 =====
#define SPEED_RUN      180  // 直线运送速度
#define SPEED_TURN     180  // 原地转向速度
#define SPEED_APPROACH 150  // 接近目标速度（低速）

// ===== 角度 =====
#define CLAW_OPEN   140  // 张开
#define CLAW_CLOSE   90  // 夹紧
#define PAN_CENTER   90  // 云台居中

// ===== 动作时长（ms，按场地与电压微调）=====
#define T_APPROACH 350  // 接近目标时长
#define T_TURN     600  // 原地转 90° 时长
#define T_DELIVER  800  // 运送时长

// ===== 表情编号 =====
#define FACE_EYES   0  // 眼睛：待命
#define FACE_SMILE  1  // 笑脸：识别到目标或完成投放
#define FACE_HEART  2  // 爱心：已夹取

// 待机：两只方眼，第 4/5、10/11 列亮第 2~4 行
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// 笑脸：16 字节对应 16 列，每字节 8 位对应一列 8 行
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// 爱心：格式同笑脸
unsigned char heart[] = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x7C,
  0x7C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // 视觉模块实例
Servo clawServo;              // 夹爪舵机
Servo panServo;               // 云台舵机

int curFace = -1;  // 当前表情，-1 表示未显示

void setup() {
  Serial.begin(115200);

  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);

  // 两个舵机各占一个定时器，避免与电机 PWM 冲突
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  // 云台：50 Hz，脉宽 500~2500 µs
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);

  // 夹爪：50 Hz，脉宽 500~2500 µs
  clawServo.setPeriodHertz(50);
  clawServo.attach(CLAW_SERVO_PIN, 500, 2500);

  // 摄像头随云台，回中朝正前
  panServo.write(PAN_CENTER);
  // 夹爪张开待命
  clawServo.write(CLAW_OPEN);

  // 电机引脚
  pinMode(MOTOR_BIN, OUTPUT);
  pinMode(MOTOR_AIN, OUTPUT);
  ledcAttach(MOTOR_BEN, 1000, 8);  // 1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);
  stopCar();

  // 启动视觉模块，颜色识别模式
  aiCam.begin();
  aiCam.setMode("color");

  // 显示待机表情
  showFace(FACE_EYES);

  Serial.println("颜色追踪车已就绪，把红色或黄色物块放到车前");
  delay(1000);  // 等舵机到位与模块切换完成
}

void loop() {
  aiCam.readSerial();  // 每轮读取串口

  String color = aiCam.getColor();  // 当前识别颜色

  // 无有效目标或非红黄：停车等待
  if (!aiCam.isColorValid() || (color != "RED" && color != "YELLOW")) {
    stopCar();
    showFace(FACE_EYES);  // 待机表情
    return;
  }

  Serial.print("识别到目标：");
  Serial.println(color);

  showFace(FACE_SMILE);  // 显示笑脸
  aiCam.clearColor();    // 清缓存，避免重复触发

  pickAndDeliver(color);  // 执行抓取投放流程

  aiCam.flushInput();   // 丢弃动作期间积压数据
  aiCam.clearColor();   // 再清缓存
  showFace(FACE_EYES);  // 回到待机
  delay(2000);          // 冷却 2 s 再找下一目标
}

// ===== 抓取与投放 =====
// 去程：靠近 → 夹取 → 转向 → 运送 → 放下
// 回程按相反顺序退回起点
void pickAndDeliver(String color) {
  // 去程
  // 1. 低速靠近
  Serial.println("1 靠近");
  goForward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();

  // 2. 夹取
  Serial.println("2 夹取");
  clawServo.write(CLAW_CLOSE);
  delay(400);           // 等舵机到位
  showFace(FACE_HEART); // 显示爱心

  // 3. 按颜色转向：红左、黄右
  Serial.println("3 转向投放区");
  if (color == "RED") {
    spinLeft(SPEED_TURN);  // 红色左转
  } else {
    spinRight(SPEED_TURN);  // 黄色右转
  }
  delay(T_TURN);
  stopCar();

  // 4. 运送
  Serial.println("4 运送");
  goForward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();

  // 5. 松爪卸货
  Serial.println("5 放下");
  clawServo.write(CLAW_OPEN);
  delay(300);

  // 回程
  // 6. 倒回转向点
  Serial.println("6 后退到转向点");
  goBackward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();

  // 7. 回正
  Serial.println("7 回正");
  if (color == "RED") {
    spinRight(SPEED_TURN);  // 反向转回
  } else {
    spinLeft(SPEED_TURN);  // 反向转回
  }
  delay(T_TURN);
  stopCar();

  // 8. 倒回起点
  Serial.println("8 后退回起点");
  goBackward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();

  showFace(FACE_SMILE);  // 显示笑脸
  Serial.println("投放完成，已回到起点");
}

// ===== 点阵显示 =====
// 仅表情变化时写屏
void showFace(int face) {
  if (face == curFace) return;  // 表情未变
  curFace = face;

  if (face == FACE_EYES) {
    matrix_display(eyes);   // 待命
  } else if (face == FACE_SMILE) {
    matrix_display(smile);  // 笑脸
  } else {
    matrix_display(heart);  // 爱心
  }
}

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
    digitalWrite(CLK_Pin, LOW);  // 拉低时钟，准备数据
    delayMicroseconds(3);

    // 从 bit0 起逐位发送
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);

    digitalWrite(CLK_Pin, HIGH); // 拉高时钟，模块采样
    delayMicroseconds(3);

    send_data = send_data >> 1;  // 右移，取出下一位
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

// ===== 电机驱动 =====
// A 路接左轮、B 路接右轮（与第 7 课一致）
// leftSpeed 走 B 路，rightSpeed 走 A 路
// 先限幅 ±255，再取绝对值给占空比
void driveMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  -255, 255);  // 右轮限幅
  rightSpeed = constrain(rightSpeed, -255, 255);  // 左轮限幅

  // A 路：左轮
  if (rightSpeed >= 0) {
    digitalWrite(MOTOR_BIN, HIGH);     // 拉高：正转
    ledcWrite(MOTOR_BEN, rightSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_BIN, LOW);      // 拉低：反转
    ledcWrite(MOTOR_BEN, -rightSpeed); // 取绝对值
  }

  // B 路：右轮
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // 拉高：正转
    ledcWrite(MOTOR_AEN, leftSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // 拉低：反转
    ledcWrite(MOTOR_AEN, -leftSpeed); // 取绝对值
  }
}

// 基本动作
void goForward(int speed)  { driveMotors(speed, speed); }    // 前进：左右同速正转
void goBackward(int speed) { driveMotors(-speed, -speed); }  // 后退：左右同速反转
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // 左转：左后退右前进
void spinRight(int speed)  { driveMotors(speed, -speed); }   // 右转：左前进右后退
void stopCar()             { driveMotors(0, 0); }            // 停止：占空比归零
```

#### **7. 代码解释：**

**(1) 视觉模块接在两个串口脚上**

```cpp
#include "AiCam.h"  // 视觉模块驱动

#define CAM_RX 18  // 主控接收脚
#define CAM_TX 17  // 主控发送脚
```

18 和 17 是主控这一侧的收发脚：主控从 18 接收（接模块的 TX），从 17 发送（接模块的 RX）。

这个模块走 9600 波特率，**传不了图像**——它在内部把画面处理完，只把"看到什么颜色"这句话发回来。所以主控这边做的事，本质是"读一句话，然后按这句话做机械动作"。

**(2) 速度和三个时长**

```cpp
#define SPEED_RUN      180  // 直线运送速度
#define SPEED_TURN     180  // 原地转向速度
#define SPEED_APPROACH 150  // 接近目标速度（低速）

#define T_APPROACH 350  // 接近目标时长
#define T_TURN     600  // 原地转 90° 时长
#define T_DELIVER  800  // 运送时长
```

接近目标用 150 而不是 180——慢一点，350 ms 走出来的距离落点更准。三个 `T_` 是这一课所有"走多远、转多少度"的答案，全是毫秒数。

这三个数只能照着车在实地上标：同样的 600 ms，电量足时转 90°，电量低时可能只转 70°，表现是"上午能对准、下午对不准"。要稳定，只能引入视觉反馈，让角度由"看到什么"决定，而不是由"转了多久"决定。

**(3) `loop()`：每轮先读串口**

```cpp
  aiCam.readSerial();  // 每轮读取串口

  String color = aiCam.getColor();  // 当前识别颜色
```

`readSerial()` 把视觉模块吐出来的字符攒成完整的一行再解析，不能指望一次就收到整帧——串口是按字节到的，一帧 `COLOR:RED` 可能跨两次 `loop()`。

`getColor()` 取的是解析出来的结果，没识别到就是空串。

**(4) 没识别到目标就直接 `return`**

```cpp
  if (!aiCam.isColorValid() || (color != "RED" && color != "YELLOW")) {
    stopCar();
    showFace(FACE_EYES);  // 待机表情
    return;
  }
```

两个条件缺一不可：`isColorValid()` 判断"这一帧有没有解析成功"，后面那半句判断"颜色是不是红或黄"。只判前者，绿色、蓝色也会被当成目标抓。

`return` 这一句很重要：没有目标就直接结束本轮，后面的抓取流程一行都不碰。用 `return` 而不是把整个流程包进 `if` 里，是为了让主流程少缩进一层，读起来平。

**(5) 开始动作前先清缓存**

```cpp
  showFace(FACE_SMILE);  // 显示笑脸
  aiCam.clearColor();    // 清缓存，避免重复触发

  pickAndDeliver(color);  // 执行抓取投放流程
```

不清这一下，`getColor()` 会一直返回刚才那个 `"RED"`，抓完回来立刻又被判成"看到红色"，无限循环抓同一处空气。

**(6) 去程第一步：低速靠近**

```cpp
  Serial.println("1 靠近");
  goForward(SPEED_APPROACH);
  delay(T_APPROACH);
  stopCar();
```

从看到目标到伸手夹，中间这段不靠视觉反馈，就靠"以 150 的速度走 350 ms"。走多远完全由这两个数决定。

**(7) 去程第二步：夹取**

```cpp
  Serial.println("2 夹取");
  clawServo.write(CLAW_CLOSE);
  delay(400);           // 等舵机到位
  showFace(FACE_HEART); // 显示爱心
```

`write()` 只是下指令，400 ms 是给爪子合拢的时间——第 9 课逐度走要 0.75 秒，这里一步到位会快一些，但也要给足时间，不然车就开始转向而爪子还没夹住球。

**(8) 去程第三到第五步：转向、运送、卸货**

```cpp
  Serial.println("3 转向投放区");
  if (color == "RED") {
    spinLeft(SPEED_TURN);  // 红色左转
  } else {
    spinRight(SPEED_TURN);  // 黄色右转
  }
  delay(T_TURN);
  stopCar();
```

红色往左、黄色往右，这是这套场地的规则，也是全课唯一"根据看到的内容做不同决定"的地方。转头同样靠 `T_TURN 600` 估角度，没有编码器。

接着 `goForward(SPEED_RUN)` + `delay(T_DELIVER)` 送到投放区，再 `clawServo.write(CLAW_OPEN)` + `delay(300)` 松爪。

**(9) 回程三步，是去程的镜像**

```cpp
  Serial.println("6 后退到转向点");
  goBackward(SPEED_RUN);
  delay(T_DELIVER);
  stopCar();
```

倒回、反向转回、再倒回起点——三段的时长和去程完全一样，方向反过来。理论上能回到出发位置，实际会差几厘米，跑几轮就偏出去了。

这就是"定长动作"的代价：没有编码器、没有测距，走多远全看毫秒数。地毯上摩擦大，同样的 350 ms 只走一半；电池满电和快没电，速度能差三成。

**(10) 动作结束后要"倒垃圾"**

```cpp
  aiCam.flushInput();   // 丢弃动作期间积压数据
  aiCam.clearColor();   // 再清缓存
  showFace(FACE_EYES);  // 回到待机
  delay(2000);          // 冷却 2 s 再找下一目标
```

这两句是这一课最容易漏的地方。抓取那几秒里，视觉模块一直在发帧，主控一个字节都没读，全堆在串口缓冲区里（默认 256 字节，早满了）。回来一读，先读到的是一大堆过期数据——里面很可能还包含刚才那个 `COLOR:RED`。

`flushInput()` 把积压全丢掉，`clearColor()` 再把解析出来的颜色标志清一次，双保险。少了这两句，现象是：抓完一个，车在原地又"看到"同一个目标，反复抓空气。

2 秒冷却不是给车歇的，是给操作者的：抓完一个物块，人有时间把它拿走或者摆下一个。不然车会立刻扑向视野里剩下的东西，看起来像失控。

这一课全部用 `delay()`，这几秒里网页、超声波、遥控全都不响应——这是本课最明显的限制，也是下一课改用状态机的原因。

#### **8. 运行现象：**

上电后串口打印：

```text
视觉模块串口已启动
发送 -> GET:MODE
发送 -> GET:OUTPUT
发送 -> MODE:COLOR
颜色追踪车已就绪，把红色或黄色物块放到车前
```

把红色物块放到车前 20 cm，车会：

| 步骤 | 串口 | 动作 |
| --- | --- | --- |
| 1 | 识别到目标：RED | 点阵笑脸，低速前进 350 ms |
| 2 | 1 靠近 | 停 |
| 3 | 2 夹取 | 夹爪合到 90°，点阵爱心 |
| 4 | 3 转向投放区 | 左转 600 ms |
| 5 | 4 运送 | 前进 800 ms |
| 6 | 5 放下 | 夹爪张开 |
| 7 | 6 后退到转向点 | 后退 800 ms |
| 8 | 7 回正 | 右转 600 ms |
| 9 | 8 后退回起点 | 后退 350 ms |
| 10 | 投放完成，已回到起点 | 点阵笑脸 |

黄色物块走同样的流程，只是第三步改成右转。

> 【图片待补】串口打印完整十步流程的截图。

#### **9. 常见问题解决：**

(1) 一直打印「识别到目标」但车不动——检查 `T_APPROACH` 是不是被改成了 0，或者电池电压不足导致电机带不动。

(2) 抓空——`T_APPROACH` 的 350 ms 与场地摩擦有关，地毯上要加长到 500 ms。这是定长动作的固有缺陷，想稳定得加超声波闭环。

(3) 认错颜色——模块对光照敏感，暖色灯光下黄色容易被认成红色。换白光灯或在自然光下测试。
