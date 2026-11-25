// Dot matrix display signal lines (see wiring table)
#define CLK_Pin 9   // Clock, connect to module CLK
#define DIN_Pin 8   // Data, connect to module DIN

// Smile font bitmap: 16 bytes for 16 columns, 8 bits per byte for 8 rows per column
unsigned char smile[] = {
  0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40, 
  0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
};

// Heart font bitmap, same format as smile
unsigned char heart[] = {
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x7c,
    0x7c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  pinMode(CLK_Pin, OUTPUT);
  pinMode(DIN_Pin, OUTPUT);
  
  // Pull both lines high to leave the bus idle
  digitalWrite(CLK_Pin, HIGH);
  digitalWrite(DIN_Pin, HIGH);
  
  // Clear display on power on to avoid residue
  unsigned char clear_data[16] = {0};
  matrix_display(clear_data);
}

void loop() {
  // Show heart for 3 s
  matrix_display(heart);
  delay(3000);
  
  // Show smile for 2 s
  matrix_display(smile);
  delay(2000);
  
  // Scroll: shift the font bitmap left by 1 column
  for (int shift = 0; shift < 16; shift++) {
    unsigned char scroll_data[16];
    for (int i = 0; i < 16; i++) {
      // Wrap around the bitmap; after 16 columns it returns to the original
      scroll_data[i] = smile[(i + shift) % 16]; 
    }
    matrix_display(scroll_data);
    delay(150); // Scroll interval, smaller is faster
  }
}

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
  
  // 0x8A: turn on display, brightness 8/16
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
    digitalWrite(CLK_Pin, LOW);  // Pull clock low, prepare data
    delayMicroseconds(3);
    
    // Send bit by bit starting from bit0
    if (send_data & 0x01) {
      digitalWrite(DIN_Pin, HIGH);
    } else {
      digitalWrite(DIN_Pin, LOW);
    }
    delayMicroseconds(3);
    
    digitalWrite(CLK_Pin, HIGH); // Pull clock high, module samples
    delayMicroseconds(3);
    
    send_data = send_data >> 1;  // Shift right to get the next bit
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
