# mini_tracked_tank_robot.py - Mini Tracked Tank Robot 迷你履带坦克 硬件驱动
# 包含：电机、云台舵机、夹爪舵机、8×16 点阵、五路循迹（PCF8574）、超声波
# 用法：car = MiniTrackedTankRobot()，再按需初始化各路外设

from machine import Pin, PWM, I2C, time_pulse_us
import time


class MiniTrackedTankRobot:

    def __init__(self):
        self.i2c = None          # 循迹用的 I2C，与点阵共用 io8/io9

        # A 路（38/21）= 左轮，B 路（40/41）= 右轮，与 Arduino 课的引脚定义一致
        # 方向脚拉低，调速脚给 0 占空比的 PWM：EN 端只靠下拉会有漏电流，
        # 舵机一转电源纹波上来，H 桥输出跟着抖，轮子会慢慢往前爬；
        # PWM 0 占空比是 LEDC 持续输出的确定低电平，能把输出彻底关断。
        self.ain = Pin(38, Pin.OUT, value=0)
        self.bin = Pin(40, Pin.OUT, value=0)
        self.aen = PWM(Pin(21), freq=20000, duty_u16=0)
        self.ben = PWM(Pin(41), freq=20000, duty_u16=0)

        # 两个舵机信号脚压低，避免悬空时跟着别的通道抖
        self._parked = [Pin(42, Pin.OUT, value=0), Pin(11, Pin.OUT, value=0)]

        self._last = (0, 0)      # 最近一次下发的电机速度，用来判断车是否在跑

    # ==================== 电机 ====================
    # A 路（38/21）接左轮，B 路（40/41）接右轮
    # 对外统一按“左、右”传参
    def Motor_init(self):
        # 引脚在 __init__ 里已经建好，这里把两路都归零
        self.ain.value(0)
        self.bin.value(0)
        self.aen.duty_u16(0)
        self.ben.duty_u16(0)
        self._last = (0, 0)

    def set_motor(self, left_speed, right_speed):
        # 只写占空比（0~255），方向由方向脚决定
        left_speed = max(0, min(255, int(left_speed)))
        right_speed = max(0, min(255, int(right_speed)))
        self.aen.duty_u16(left_speed * 257)      # A 路 = 左轮
        self.ben.duty_u16(right_speed * 257)     # B 路 = 右轮
        self._last = (left_speed, right_speed)

    def drive(self, left_speed, right_speed):
        # 带符号速度：正转 / 反转 / 0 停
        left_speed = max(-255, min(255, int(left_speed)))
        right_speed = max(-255, min(255, int(right_speed)))

        if left_speed >= 0:                      # A 路：左轮
            self.ain.value(1)
            self.aen.duty_u16(left_speed * 257)
        else:
            self.ain.value(0)
            self.aen.duty_u16(-left_speed * 257)

        if right_speed >= 0:                     # B 路：右轮
            self.bin.value(1)
            self.ben.duty_u16(right_speed * 257)
        else:
            self.bin.value(0)
            self.ben.duty_u16(-right_speed * 257)

        self._last = (left_speed, right_speed)

    def forward(self, left_speed, right_speed):
        # 前进：两个方向脚拉高
        self.ain.value(1)
        self.bin.value(1)
        self.set_motor(left_speed, right_speed)

    def back(self, left_speed, right_speed):
        # 后退：两个方向脚拉低
        self.ain.value(0)
        self.bin.value(0)
        self.set_motor(left_speed, right_speed)

    def left(self, left_speed, right_speed):
        # 原地左转：左轮反转、右轮正转
        self.ain.value(0)
        self.bin.value(1)
        self.set_motor(left_speed, right_speed)

    def right(self, left_speed, right_speed):
        # 原地右转：左轮正转、右轮反转
        self.ain.value(1)
        self.bin.value(0)
        self.set_motor(left_speed, right_speed)

    def stop_motor(self):
        # 占空比清零，方向脚也拉低，两个输入都停在确定电平
        self.set_motor(0, 0)
        self.ain.value(0)
        self.bin.value(0)

    def quiet_motor(self):
        # 舵机动作时驱动板容易受扰，车没在跑就把电机重新压回 0
        if self._last == (0, 0):
            self.ain.value(0)
            self.bin.value(0)
            self.aen.duty_u16(0)
            self.ben.duty_u16(0)

    # ==================== 舵机 ====================
    def Servo_init(self, SERVO_PIN=42):
        # 云台舵机，上电回中位
        self.servo = PWM(Pin(SERVO_PIN), freq=50)
        self.Servo_set_angle(90)

    def Servo_set_angle(self, angle):
        # 0~180 度，脉宽 500~2500 微秒
        angle = max(0, min(180, angle))
        pulse = 500 + angle * 2000 / 180
        self.servo.duty_u16(int(pulse / 20000 * 65535))
        self.quiet_motor()

    def Claw_init(self, CLAW_PIN=11):
        # 夹爪舵机
        self.claw = PWM(Pin(CLAW_PIN), freq=50)

    def Claw_set_angle(self, angle):
        # 90 度夹紧、140 度张开
        angle = max(0, min(180, angle))
        pulse = 500 + angle * 2000 / 180
        self.claw.duty_u16(int(pulse / 20000 * 65535))
        self.quiet_motor()

    # ==================== 8×16 点阵 ====================
    def Matrix_init(self, CLK_PIN=9, DIN_PIN=8):
        # 点阵与循迹共用 io8/io9：先把 I2C 释放，再把两根脚切回普通输出
        if self.i2c is not None:
            try:
                self.i2c.deinit()
            except Exception:
                pass
            self.i2c = None
        self.clk = Pin(CLK_PIN, Pin.OUT, value=1)
        self.din = Pin(DIN_PIN, Pin.OUT, value=1)

    def _iic_start(self):
        # 起始条件：CLK 高电平期间 DIN 下降沿
        self.din.value(1)
        time.sleep_us(5)
        self.clk.value(1)
        time.sleep_us(5)
        self.din.value(0)
        time.sleep_us(5)
        self.clk.value(0)
        time.sleep_us(5)

    def _iic_send(self, data):
        # 发送 1 字节，低位在前
        for _ in range(8):
            self.clk.value(0)
            time.sleep_us(5)
            self.din.value(1 if data & 0x01 else 0)
            time.sleep_us(5)
            self.clk.value(1)
            time.sleep_us(5)
            data >>= 1
        self.clk.value(0)
        time.sleep_us(5)

    def _iic_end(self):
        # 停止条件：CLK 高电平期间 DIN 上升沿
        self.clk.value(0)
        time.sleep_us(5)
        self.din.value(0)
        time.sleep_us(5)
        self.clk.value(1)
        time.sleep_us(5)
        self.din.value(1)
        time.sleep_us(5)

    def Matrix_display(self, matrix_value):
        # 写入 16 字节显存并开显示
        self._iic_start()
        self._iic_send(0x40)                 # 0x40：写数据模式，地址自增
        self._iic_end()

        self._iic_start()
        self._iic_send(0xC0)                 # 0xC0：显存起始地址 0
        for b in matrix_value[:16]:
            self._iic_send(b)
        self._iic_end()

        self._iic_start()
        self._iic_send(0x8A)                 # 0x8A：开显示，亮度 8/16
        self._iic_end()

    def Matrix_off(self):
        # 0x88：关显示，显存内容保留
        self._iic_start()
        self._iic_send(0x88)
        self._iic_end()

    def Matrix_clear(self):
        self.Matrix_display(bytes(16))

    def Matrix_scroll(self, matrix_value, step_ms=150):
        # 整幅图循环左移 16 列
        for i in range(16):
            self.Matrix_display(matrix_value[i:] + matrix_value[:i])
            time.sleep_ms(step_ms)

    # ==================== 五路循迹（PCF8574） ====================
    def Line_init(self, SCL_PIN=9, SDA_PIN=8, port=0):
        # 与点阵共用 io8/io9，进入循迹前必须调用本函数把总线交回 I2C
        self.i2c = I2C(port, scl=Pin(SCL_PIN), sda=Pin(SDA_PIN), freq=400000)
        try:
            self.i2c.writeto(0x20, b'\xff')   # 全部脚设为输入
        except OSError as e:
            print('PCF8574 初始化失败：', e)

    def Line_get(self):
        # 返回 (A,B,C,D,E)，1 表示压到黑线；顺序为最左、左、中、右、最右
        try:
            data = self.i2c.readfrom(0x20, 1)[0]
        except OSError:
            return 0, 0, 0, 0, 0
        return (1 - ((data >> 4) & 1),
                1 - ((data >> 0) & 1),
                1 - ((data >> 1) & 1),
                1 - ((data >> 2) & 1),
                1 - ((data >> 3) & 1))

    # ==================== 超声波 ====================
    def Ultrasonic_init(self, TRIG_PIN=13, ECHO_PIN=12):
        self.trig = Pin(TRIG_PIN, Pin.OUT, value=0)
        self.echo = Pin(ECHO_PIN, Pin.IN)

    def Ultrasonic_distance(self):
        # 返回厘米；超量程或超时返回 0
        self.trig.value(0)
        time.sleep_us(2)
        self.trig.value(1)
        time.sleep_us(10)
        self.trig.value(0)

        duration = time_pulse_us(self.echo, 1, 25000)   # 25 ms 超时
        if duration < 0:
            return 0

        distance = duration * 0.034 / 2
        if distance < 2 or distance > 300:
            return 0
        return int(distance)
