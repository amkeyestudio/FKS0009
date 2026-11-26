# aicam.py - serial driver for the AI vision module
# The module does the image processing itself and only returns result text over the serial port; this class parses that text into data
# Reply format: AICAM:COLOR:RED or COLOR:RED; in QR mode the module may also return the content directly

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
        self._rx_line = b''         # Serial frame buffer, a frame ends on \r or \n

        self.faceXValue = 0
        self.faceYValue = 0
        self.faceValid = False

        self.colorValue = ''
        self.qrValue = ''
        self.cardValue = ''

        self.lineOffsetValue = 0
        self.lineAngleValue = 0
        self.lineValidValue = 0

    # ==================== Initialisation ====================
    def begin(self, tx=17, rx=18, baud=9600):
        # Controller TX to module RX, controller RX to module TX, with a common ground
        self.uart = UART(2, baudrate=baud, tx=tx, rx=rx, timeout=0)
        time.sleep_ms(500)              # Wait for the module to stabilise after power-up (same as the 500 ms in the Arduino version)

        # Send the query handshake to confirm the module is online (same as GET:MODE / GET:OUTPUT in the Arduino begin)
        self.uart.write(b'GET:MODE\r\n')
        time.sleep_ms(50)
        self.uart.write(b'GET:OUTPUT\r\n')
        time.sleep_ms(50)

    def flush(self):
        # Drop the stale data buffered in the serial port
        if self.uart is None:
            return
        while self.uart.any():
            self.uart.read()

    # ==================== Mode switching ====================
    def setMode(self, mode):
        if self.uart is None:
            return
        self.mode = mode
        cmd = ('MODE:%s\r\n' % mode).encode()
        for _ in range(3):          # Send three times so the module does not miss the command
            self.uart.write(cmd)
            time.sleep_ms(50)

    # ==================== Reading the serial port ====================
    def read(self):
        if self.uart is None:
            return
        # Read byte by byte, both \r and \n end a frame (same as readSerial in the Arduino version)
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
            # Bare data without a colon: in QR mode the module returns the card name directly
            # Filter out stray frames such as the no-card state (NONE) and pure numbers so they are not taken as a QR code
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
            # All four cards are pure alphabetic names; pure numbers, NONE and empty strings are stray no-card frames and are cleared
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

    # ==================== Clearing the buffer ====================
    def clearColor(self):
        self.colorValue = ''

    def clearQr(self):
        self.qrValue = ''

    # ==================== Accessors ====================
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
