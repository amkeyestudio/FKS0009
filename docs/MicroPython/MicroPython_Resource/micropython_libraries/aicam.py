# aicam.py - AI 视觉模块串口驱动
# 模块自己完成图像处理，串口只回传结果文字；本类负责把文字解析成数据
# 回传格式：AICAM:COLOR:RED 或 COLOR:RED，二维码模式还可能直接回内容

from machine import UART
import time


class AiCam:

    FACE = 'FACE'
    COLOR = 'COLOR'
    QR = 'QR'
    CARD = 'CARD'
    LINE = 'LINE'

    def __init__(self):
        self.uart = None
        self.mode = ''
        self._rx_line = b''         # 串口收帧缓冲，遇 \r 或 \n 结束一帧

        self.faceXValue = 0
        self.faceYValue = 0
        self.faceValid = False

        self.colorValue = ''
        self.qrValue = ''
        self.cardValue = ''

        self.lineOffsetValue = 0
        self.lineAngleValue = 0
        self.lineValidValue = 0

    # ==================== 初始化 ====================
    def begin(self, tx=17, rx=18, baud=9600):
        # 主控 TX 接模块 RX，主控 RX 接模块 TX，两边共地
        self.uart = UART(2, baudrate=baud, tx=tx, rx=rx, timeout=0)
        time.sleep_ms(500)              # 等模块上电稳定（对齐 Arduino 的 500ms）

        # 发查询握手，确认模块在线（对齐 Arduino begin 的 GET:MODE / GET:OUTPUT）
        self.uart.write(b'GET:MODE\r\n')
        time.sleep_ms(50)
        self.uart.write(b'GET:OUTPUT\r\n')
        time.sleep_ms(50)

    def flush(self):
        # 丢掉串口里积压的旧数据
        if self.uart is None:
            return
        while self.uart.any():
            self.uart.read()

    # ==================== 切换模式 ====================
    def setMode(self, mode):
        if self.uart is None:
            return
        self.mode = mode
        cmd = ('MODE:%s\r\n' % mode).encode()
        for _ in range(3):          # 连发三次，防止模块漏收
            self.uart.write(cmd)
            time.sleep_ms(50)

    # ==================== 读串口 ====================
    def read(self):
        if self.uart is None:
            return
        # 逐字节收，\r 或 \n 都算帧结束（对齐 Arduino readSerial）
        while self.uart.any():
            ch = self.uart.read(1)
            if not ch:
                break
            if ch == b'\r' or ch == b'\n':
                if self._rx_line:
                    try:
                        text = self._rx_line.decode('utf-8').strip()
                        self._rx_line = b''
                        if text:
                            self._parse(text)
                    except Exception:
                        self._rx_line = b''
            else:
                self._rx_line += ch

    def _parse(self, text):
        parts = text.split(':')
        if len(parts) >= 3:
            mode = parts[1].upper()
            data = ':'.join(parts[2:])
        elif len(parts) == 2:
            mode = parts[0].upper()
            data = parts[1]
        else:
            # 无冒号裸数据：二维码模式下模块直接回卡名
            # 挡掉无卡状态(NONE)、纯数字等杂帧，避免误当二维码
            if self.mode == self.QR:
                v = text.strip()
                has_letter = False
                for c in v:
                    if ('A' <= c <= 'Z') or ('a' <= c <= 'z'):
                        has_letter = True
                        break
                if v and v.upper() != 'NONE' and has_letter:
                    self.qrValue = v
                else:
                    self.qrValue = ''
            return

        if mode == 'FACE':
            xy = data.split(',')
            if len(xy) >= 2:
                try:
                    self.faceXValue = int(xy[0])
                    self.faceYValue = int(xy[1])
                    self.faceValid = self.faceXValue > 1
                except ValueError:
                    self.faceValid = False

        elif mode == 'COLOR':
            if data == '' or data.upper() == 'NONE':
                self.colorValue = ''
            else:
                self.colorValue = data

        elif mode == 'QR':
            v = data.strip()
            # 四张卡都是纯字母名；纯数字、NONE、空串都是无卡杂帧，一律清空
            has_letter = False
            for c in v:
                if ('A' <= c <= 'Z') or ('a' <= c <= 'z'):
                    has_letter = True
                    break
            if not v or v.upper() == 'NONE' or not has_letter:
                self.qrValue = ''
            else:
                self.qrValue = v

        elif mode == 'CARD':
            self.cardValue = data

        elif mode == 'LINE':
            values = data.split(',')
            if len(values) == 3:
                try:
                    self.lineOffsetValue = int(values[0])
                    self.lineAngleValue = int(values[1])
                    self.lineValidValue = int(values[2])
                except ValueError:
                    pass

    # ==================== 清缓存 ====================
    def clearColor(self):
        self.colorValue = ''

    def clearQr(self):
        self.qrValue = ''

    # ==================== 取值 ====================
    def color(self):
        return self.colorValue

    def qr(self):
        return self.qrValue

    def card(self):
        return self.cardValue

    def faceX(self):
        return self.faceXValue

    def faceY(self):
        return self.faceYValue

    def faceDetected(self):
        return self.faceValid

    def lineOffset(self):
        return self.lineOffsetValue

    def lineAngle(self):
        return self.lineAngleValue

    def lineValid(self):
        return self.lineValidValue
