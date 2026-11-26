# mini_tracked_tank_robot.py - hardware driver for the Mini Tracked Tank Robot
# Covers: motors, pan-tilt servo, claw servo, 8x16 dot matrix, five-channel line tracking (PCF8574), ultrasonic ranging
# Usage: car = MiniTrackedTankRobot(), then initialise the peripherals you need

from machine import Pin, PWM, I2C, time_pulse_us
import time


class MiniTrackedTankRobot:

    def __init__(self):
        self.i2c = None          # I2C used by the line sensors, shared with the dot matrix on io8/io9

        # Channel A (38/21) = left wheel, channel B (40/41) = right wheel, same pin assignment as the Arduino lesson
        # Direction pins low and a 0 duty PWM on the speed pins: a pull-down alone leaks current on EN,
        # and once the servo moves the supply ripple makes the H-bridge output wobble and the wheels creep forward;
        # a 0 duty PWM is a steady low level from the LEDC peripheral that shuts the output down completely.
        self.ain = Pin(38, Pin.OUT, value=0)
        self.bin = Pin(40, Pin.OUT, value=0)
        self.aen = PWM(Pin(21), freq=20000, duty_u16=0)
        self.ben = PWM(Pin(41), freq=20000, duty_u16=0)

        # Hold both servo signal pins low so they do not wobble with other channels while floating
        self._parked = [Pin(42, Pin.OUT, value=0), Pin(11, Pin.OUT, value=0)]

        self._last = (0, 0)      # Last motor speeds sent, used to tell whether the car is moving

    # ==================== Motors ====================
    # Channel A (38/21) drives the left wheel, channel B (40/41) the right wheel
    # The public API always takes (left, right)
    def Motor_init(self):
        # The pins are created in __init__, here both channels are zeroed
        self.ain.value(0)
        self.bin.value(0)
        self.aen.duty_u16(0)
        self.ben.duty_u16(0)
        self._last = (0, 0)

    def set_motor(self, left_speed, right_speed):
        # Only the duty is written (0-255), the direction pins decide the direction
        left_speed = max(0, min(255, int(left_speed)))
        right_speed = max(0, min(255, int(right_speed)))
        self.aen.duty_u16(left_speed * 257)      # Channel A = left wheel
        self.ben.duty_u16(right_speed * 257)     # Channel B = right wheel
        self._last = (left_speed, right_speed)

    def drive(self, left_speed, right_speed):
        # Signed speed: forward / reverse / 0 to stop
        left_speed = max(-255, min(255, int(left_speed)))
        right_speed = max(-255, min(255, int(right_speed)))

        if left_speed >= 0:                      # Channel A: left wheel
            self.ain.value(1)
            self.aen.duty_u16(left_speed * 257)
        else:
            self.ain.value(0)
            self.aen.duty_u16(-left_speed * 257)

        if right_speed >= 0:                     # Channel B: right wheel
            self.bin.value(1)
            self.ben.duty_u16(right_speed * 257)
        else:
            self.bin.value(0)
            self.ben.duty_u16(-right_speed * 257)

        self._last = (left_speed, right_speed)

    def forward(self, left_speed, right_speed):
        # Forward: both direction pins high
        self.ain.value(1)
        self.bin.value(1)
        self.set_motor(left_speed, right_speed)

    def back(self, left_speed, right_speed):
        # Backward: both direction pins low
        self.ain.value(0)
        self.bin.value(0)
        self.set_motor(left_speed, right_speed)

    def left(self, left_speed, right_speed):
        # Spin left in place: left wheel reverse, right wheel forward
        self.ain.value(0)
        self.bin.value(1)
        self.set_motor(left_speed, right_speed)

    def right(self, left_speed, right_speed):
        # Spin right in place: left wheel forward, right wheel reverse
        self.ain.value(1)
        self.bin.value(0)
        self.set_motor(left_speed, right_speed)

    def stop_motor(self):
        # Clear the duty and pull the direction pins low so both inputs sit at a defined level
        self.set_motor(0, 0)
        self.ain.value(0)
        self.bin.value(0)

    def quiet_motor(self):
        # The driver board is easily disturbed by servo movement, so hold the motors at 0 while the car is idle
        if self._last == (0, 0):
            self.ain.value(0)
            self.bin.value(0)
            self.aen.duty_u16(0)
            self.ben.duty_u16(0)

    # ==================== Servos ====================
    def Servo_init(self, SERVO_PIN=42):
        # Pan-tilt servo, centred at power-up
        self.servo = PWM(Pin(SERVO_PIN), freq=50)
        self.Servo_set_angle(90)

    def Servo_set_angle(self, angle):
        # 0-180 deg, pulse width 500-2500 us
        angle = max(0, min(180, angle))
        pulse = 500 + angle * 2000 / 180
        self.servo.duty_u16(int(pulse / 20000 * 65535))
        self.quiet_motor()

    def Claw_init(self, CLAW_PIN=11):
        # Claw servo
        self.claw = PWM(Pin(CLAW_PIN), freq=50)

    def Claw_set_angle(self, angle):
        # 90 deg closed, 140 deg open
        angle = max(0, min(180, angle))
        pulse = 500 + angle * 2000 / 180
        self.claw.duty_u16(int(pulse / 20000 * 65535))
        self.quiet_motor()

    # ==================== 8x16 dot matrix ====================
    def Matrix_init(self, CLK_PIN=9, DIN_PIN=8):
        # The matrix and line sensors share io8/io9: release I2C first, then switch both pins back to plain outputs
        if self.i2c is not None:
            try:
                self.i2c.deinit()
            except Exception:
                pass
            self.i2c = None
        self.clk = Pin(CLK_PIN, Pin.OUT, value=1)
        self.din = Pin(DIN_PIN, Pin.OUT, value=1)

    def _iic_start(self):
        # Start condition: DIN falling edge while CLK is high
        self.din.value(1)
        time.sleep_us(5)
        self.clk.value(1)
        time.sleep_us(5)
        self.din.value(0)
        time.sleep_us(5)
        self.clk.value(0)
        time.sleep_us(5)

    def _iic_send(self, data):
        # Send one byte, least significant bit first
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
        # Stop condition: DIN rising edge while CLK is high
        self.clk.value(0)
        time.sleep_us(5)
        self.din.value(0)
        time.sleep_us(5)
        self.clk.value(1)
        time.sleep_us(5)
        self.din.value(1)
        time.sleep_us(5)

    def Matrix_display(self, matrix_value):
        # Write 16 bytes of display RAM and turn the display on
        self._iic_start()
        self._iic_send(0x40)                 # 0x40: data write mode with auto-increment
        self._iic_end()

        self._iic_start()
        self._iic_send(0xC0)                 # 0xC0: display RAM starts at address 0
        for b in matrix_value[:16]:
            self._iic_send(b)
        self._iic_end()

        self._iic_start()
        self._iic_send(0x8A)                 # 0x8A: display on, brightness 8/16
        self._iic_end()

    def Matrix_off(self):
        # 0x88: display off, display RAM is kept
        self._iic_start()
        self._iic_send(0x88)
        self._iic_end()

    def Matrix_clear(self):
        self.Matrix_display(bytes(16))

    def Matrix_scroll(self, matrix_value, step_ms=150):
        # Shift the whole image left by 16 columns, wrapping around
        for i in range(16):
            self.Matrix_display(matrix_value[i:] + matrix_value[:i])
            time.sleep_ms(step_ms)

    # ==================== Five-channel line tracking (PCF8574) ====================
    def Line_init(self, SCL_PIN=9, SDA_PIN=8, port=0):
        # io8/io9 are shared with the matrix, call this before line tracking to hand the bus back to I2C
        self.i2c = I2C(port, scl=Pin(SCL_PIN), sda=Pin(SDA_PIN), freq=400000)
        try:
            self.i2c.writeto(0x20, b'\xff')   # Set every pin as an input
        except OSError as e:
            print('PCF8574 init failed: ', e)

    def Line_get(self):
        # Returns (A,B,C,D,E), 1 means the sensor is on the black line; the order is far left, left, centre, right, far right
        try:
            data = self.i2c.readfrom(0x20, 1)[0]
        except OSError:
            return 0, 0, 0, 0, 0
        return (1 - ((data >> 4) & 1),
                1 - ((data >> 0) & 1),
                1 - ((data >> 1) & 1),
                1 - ((data >> 2) & 1),
                1 - ((data >> 3) & 1))

    # ==================== Ultrasonic ====================
    def Ultrasonic_init(self, TRIG_PIN=13, ECHO_PIN=12):
        self.trig = Pin(TRIG_PIN, Pin.OUT, value=0)
        self.echo = Pin(ECHO_PIN, Pin.IN)

    def Ultrasonic_distance(self):
        # Returns centimetres; 0 when out of range or on timeout
        self.trig.value(0)
        time.sleep_us(2)
        self.trig.value(1)
        time.sleep_us(10)
        self.trig.value(0)

        duration = time_pulse_us(self.echo, 1, 25000)   # 25 ms timeout
        if duration < 0:
            return 0

        distance = duration * 0.034 / 2
        if distance < 2 or distance > 300:
            return 0
        return int(distance)
