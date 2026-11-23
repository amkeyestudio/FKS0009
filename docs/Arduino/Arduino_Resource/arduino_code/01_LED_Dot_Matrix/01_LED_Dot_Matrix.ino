// 点阵屏信号线（对应接线表）
#define CLK_Pin 9   // 时钟，接模块 CLK
#define DIN_Pin 8   // 数据，接模块 DIN

// 笑脸字模：16 字节对应 16 列，每字节 8 位对应一列 8 行
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40, 
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// 爱心字模，格式同笑脸
unsigned char heart[] = {
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
    0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  
  // 两线拉高，总线置空闲
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  
  // 上电清屏，避免残留
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);
}

void loop() {
  // 显示爱心 3 s
  matrix_display(heart);
  delay(3000);
  
  // 显示笑脸 2 s
  matrix_display(smile);
  delay(2000);
  
  // 滚动：字模整体左移 1 列
  for (int shift = 0; shift < 16; shift++) {
    unsigned char scroll_data[16];
    for (int i = 0; i < 16; i++) {
      // 取模回绕，移满 16 列回到原图
      scroll_data[i] = smile[(i + shift) % 16]; 
    }
    matrix_display(scroll_data);
    delay(150); // 滚动间隔，越小越快
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
  
  // 0x8A：开显示，亮度 8/16
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
