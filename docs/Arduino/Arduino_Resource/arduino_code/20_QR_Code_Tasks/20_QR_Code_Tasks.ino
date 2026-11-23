// 第 20 课：二维码任务卡
// 扫到卡片后按内容走一段，四张卡：FORWARD / BACKWARD / LEFT / RIGHT
// 云台上电先转到 90° 保证镜头水平，本课之后不再转动
// 卡片用 ASCII 英文：中文在 UTF-8 / GBK 下字节数不同，与模块回传比对会失败

#include <ESP32Servo.h>
#include "AiCam.h"  // 视觉模块驱动

// ===== 电机引脚 =====
#define MOTOR_AIN 38  // A 路方向：左轮
#define MOTOR_AEN 21  // A 路调速：左轮
#define MOTOR_BIN 40  // B 路方向：右轮
#define MOTOR_BEN 41  // B 路调速：右轮

// ===== 云台舵机 =====
// 摄像头与超声波共壳装在云台上，靠该舵机改变朝向
#define PAN_SERVO_PIN 42  // 云台舵机，第 8、18 课
#define PAN_CENTER    90  // 正前方，镜头水平

// ===== 视觉模块串口 =====
// 主控 RX←模块 TX，主控 TX→模块 RX，需共地
#define CAM_RX 18  // 主控接收脚
#define CAM_TX 17  // 主控发送脚

// ===== 点阵引脚 =====
#define CLK_Pin 9  // 时钟，接模块 CLK
#define DIN_Pin 8  // 数据，接模块 DIN

// ===== 任务参数 =====
// 直行 150：低于约 70 有启动死区，150 可稳定起步
#define SPEED_MOVE     150  // 直行 / 后退速度

// 转向用两侧不同速而非反转：反转只原地打圈，本课需要边走边转
#define SPEED_TURN_OUT 150  // 转向外侧轮速度
#define SPEED_TURN_IN   50  // 转向内侧轮速度

// 单卡动作时长，决定行进距离
#define ACTION_TIME   1000  // 到点自动停

// ===== 调试开关 =====
// 1 打印扫到的内容与字节数，0 关闭
#define DEBUG_OUT 1

// ===== 表情编号 =====
// 用编号便于 showFace() 判断表情是否变化，避免重复写屏
#define FACE_EYES  0  // 待机：眼睛
#define FACE_SMILE 1  // 识别到卡：笑脸

// 16 字节对应 16 列，每字节 8 位对应一列 8 行，bit0 为最上行

// 待机：两只方眼，第 4/5、10/11 列亮第 2~4 行
unsigned char eyes[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
  0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
};

// 笑脸
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

AiCam aiCam(CAM_RX, CAM_TX);  // 视觉模块实例
Servo panServo;               // 云台舵机

// 上次执行的卡片内容，用于区分持续举卡与换新卡
String lastQr = "";

// 当前表情，-1 未显示
int curFace = -1;

void setup() {
#if DEBUG_OUT
  // 调试串口，DEBUG_OUT 为 0 时不编译
  Serial.begin(115200);
#endif

  // 两线拉高，总线置空闲
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  showFace(FACE_EYES);  // 显示待机表情

  // 电机引脚
  pinMode(MOTOR_BIN, OUTPUT);      // 左轮方向
  pinMode(MOTOR_AIN, OUTPUT);      // 右轮方向
  ledcAttach(MOTOR_BEN, 1000, 8);  // 左轮 PWM：1 kHz，8 位
  ledcAttach(MOTOR_AEN, 1000, 8);  // 右轮 PWM
  stopCar();                       // 上电停车

  // 云台：上电先把镜头摆到正前方
  ESP32PWM::allocateTimer(0);                 // 分配 LEDC 定时器
  panServo.setPeriodHertz(50);                // 50 Hz
  panServo.attach(PAN_SERVO_PIN, 500, 2500);  // IO42，脉宽 500~2500 µs
  panServo.write(PAN_CENTER);                 // 转到正前方
  delay(300);                                 // 等舵机到位

  // 启动视觉模块，二维码模式
  aiCam.begin();          // 打开 Serial2 并绑定引脚
  aiCam.setMode("qr");    // 发送 MODE:QR

  delay(1000);  // 等模式切换完成，避免读到切换期杂数据
}

// ===== 主循环 =====
// 每轮：读串口 → 判断新卡 → 执行或停车
void loop() {
  aiCam.readSerial();  // 读完串口并更新缓存

  String qr = aiCam.getQrCode();  // 当前二维码内容，空串表示无卡

#if DEBUG_OUT
  // 内容变化时打印原文与字节数
  // 四张卡分别 7 / 8 / 4 / 5 字节
  static String lastPrinted = "";        // static 跨轮次保持
  if (qr.length() > 0 && qr != lastPrinted) {
    lastPrinted = qr;
    Serial.print("扫到二维码：");
    Serial.print(qr);
    Serial.print("  字节数 ");
    Serial.println(qr.length());
  }
#endif

  // 无卡
  // 清空 lastQr 解除锁定
  if (qr.length() == 0) {
    lastQr = "";          // 允许下一张卡触发
    stopCar();            // 停车
    showFace(FACE_EYES);  // 待机表情
    delay(20);
    return;               // 结束本轮
  }

  // 同一张卡未移开
  // 跳过，不重复执行
  if (qr == lastQr) {
    delay(20);
    return;
  }

  // 新卡
  // 先记录再执行，避免执行期间重复触发
  lastQr = qr;    // 记录本卡
  doTask(qr);     // 执行动作，内部阻塞 1 s

  delay(20);
}

// ===== 任务执行 =====
// 四张卡各执行一段动作，到时间自动停
//   FORWARD  两轮同速正转
//   BACKWARD 两轮同速反转
//   LEFT     左 50 / 右 150，向左画弧
//   RIGHT    左 150 / 右 50，向右画弧
void doTask(String qr) {
  showFace(FACE_SMILE);  // 显示笑脸

  // 统一转大写后比较
  String key = qr;
  key.trim();        // 去首尾空白
  key.toUpperCase(); // 转大写

  bool known = true;  // 是否为已知卡

  if (key == "FORWARD") {
    goForward(SPEED_MOVE);                        // 两轮同速正转
  } else if (key == "BACKWARD") {
    goBackward(SPEED_MOVE);                       // 两轮同速反转
  } else if (key == "LEFT") {
    driveMotors(SPEED_TURN_IN, SPEED_TURN_OUT);   // 左慢右快，向左画弧
  } else if (key == "RIGHT") {
    driveMotors(SPEED_TURN_OUT, SPEED_TURN_IN);   // 左快右慢，向右画弧
  } else {
    known = false;  // 非四张卡，不做动作
  }

  // 仅已知卡驱动电机
  if (known) {
    delay(ACTION_TIME);  // 走满设定时长
    stopCar();           // 到点停车
  }
}

// ===== 点阵显示 =====
// 仅表情变化时写屏
// 写屏要走 20 多字节软件 I2C，频繁调用会拖慢主循环
void showFace(int face) {
  if (face == curFace) return;  // 表情未变
  curFace = face;               // 记录当前表情

  if (face == FACE_EYES) {
    matrix_display(eyes);   // 待机
  } else {
    matrix_display(smile);  // 笑脸
  }
}

// 将 16 字节字模写入点阵屏显存
// 流程：写数据命令 → 起始地址 → 连发 16 字节 → 开显示
void matrix_display(unsigned char matrix_value[]) {
  // 0x40：写数据模式，地址自增
  IIC_start();
  IIC_send(0x40);
  IIC_end();

  // 0xC0：起始地址 0
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
    ledcWrite(MOTOR_BEN, -rightSpeed); // 取绝对值，方向由方向脚决定
  }

  // B 路：右轮
  if (leftSpeed >= 0) {
    digitalWrite(MOTOR_AIN, HIGH);    // 拉高：正转
    ledcWrite(MOTOR_AEN, leftSpeed);  // 占空比 = 速度
  } else {
    digitalWrite(MOTOR_AIN, LOW);     // 拉低：反转
    ledcWrite(MOTOR_AEN, -leftSpeed); // 占空比取绝对值
  }
}

// 基本动作
// 单速度参数，两轮同值
void goForward(int speed)  { driveMotors(speed, speed); }    // 前进：左右同速正转
void goBackward(int speed) { driveMotors(-speed, -speed); }  // 后退：左右同速反转
void spinLeft(int speed)   { driveMotors(-speed, speed); }   // 左转：原地转
void spinRight(int speed)  { driveMotors(speed, -speed); }   // 右转：原地转
void stopCar()             { driveMotors(0, 0); }            // 停止：占空比归零
