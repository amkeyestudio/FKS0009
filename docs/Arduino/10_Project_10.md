### Project 10: IR Receiving

#### **1. Introduction:**

![TV remote control](media/intro_10.jpg){width=520px}

The remotes for the TV, the air conditioner and toys all use infrared. Press a key and what leaves the remote isn't a steady beam — it's a stream of invisible light blinking 38,000 times a second. Your eye sees nothing, but the receiver turns that flicker into a clean sequence of 0s and 1s.

One press sends a whole frame: an address, a key value and a checksum. Hold the key down and the remote stops repeating the full frame — every so often it sends only a short repeat marker saying "still this key". A long press therefore leaves a completely different trace in the data than a single tap.

This lesson drives nothing yet; it only teaches the robot to understand the remote. Point it at the front of the car, press a direction key, and the serial port prints the protocol name and key value. Up, down, left, right and OK correspond to 70, 21, 68, 67 and 64.

Those values are not universal — the set in the files is only the sample we had on hand. The number your own remote prints on serial is the answer for that remote.

#### **2. Parts Needed:**

Parts used in this lesson:

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_12.png){width=170px}<br><span style="font-size:0.85em">17-key IR remote control (JMFP-4) ×1</span> | ![](../media/ir_receiver.png){width=170px}<br><span style="font-size:0.85em">IR receiver ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 motor expansion board ×1</span> |
| ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 development board ×1</span> |  |  |
> ⚠️ **The IR receiver is already mounted on the expansion board; this table is for reference only — there is nothing to take apart. The wiring notes below only show how the connections are made, for reference; no assembly or disassembly is required. For the complete parts list, see 1.4 Kit Parts List.**
#### **3. Parameters:**

| Item | Value |
| --- | --- |
| Receiver pin | IO48 |
| Carrier frequency | 38 kHz |
| Protocol | NEC |
| Serial baud rate | 115200 |
| Recorded key codes | Up 70, Down 21, Left 68, Right 67, OK 64 |
| Repeat-frame flag | `IRDATA_FLAGS_IS_REPEAT` |
| Required library | `IRremote` |

#### **4. Wiring:**

| Receiver pin | Connect to |
| --- | :---: |
| OUT / S | IO48 |
| VCC | 5V |
| GND | GND |

#### **5. Flowchart:**

Receiving and decoding are two different jobs: `IrReceiver.decode()` turns the light signal back into a frame of data, and only `decodedIRData` holds fields such as the protocol, the key code and whether it was a long press.

![](media/ir_decode_flow.png)

#### **6. Test Code:**

```cpp
#include <IRremote.hpp>  // IR decoding library

// IR receiver, IO48
#define IR_RECEIVE_PIN 48

// Remote control key code values
// Codes may differ by batch; refer to serial print
#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial to stabilize

  Serial.println("IR receiver experiment started, point the remote at the car");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  Serial.println("Waiting for a key...");
}

void loop() {
  // Received a decodable frame
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    static bool lastHolding = false;  // Whether the previous frame was a long press, to avoid duplicate printing

    if (holding) {
      // A long press periodically resends the same code
      if (!lastHolding) {
        Serial.println("(Long press active, no more per-frame printing)");
      }
    } else {
      Serial.print("Protocol: ");
      Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
      Serial.print("   Key code: ");
      Serial.print(cmd);
      Serial.print("(0x");
      Serial.print(cmd, HEX);
      Serial.print(")   ");
      Serial.println(keyName(cmd));
    }
    lastHolding = holding;

    IrReceiver.resume();  // Must be called, otherwise only one frame is received
  }
}

// Convert key code to name
const char* keyName(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:    return "Up";
    case KEY_DOWN:  return "Down";
    case KEY_LEFT:  return "Left";
    case KEY_RIGHT: return "Right";
    case KEY_STOP:  return "OK / Stop";
    default:        return "";
  }
}
```

#### **7. Code Explanation:**

**(1) Receiver pin and the five key-code macros**

```cpp
#define IR_RECEIVE_PIN 48

#define KEY_UP    70
#define KEY_DOWN  21
#define KEY_LEFT  68
#define KEY_RIGHT 67
#define KEY_STOP  64
```

`IR_RECEIVE_PIN 48` is the signal pin of the receiver. The five macros below are the codes of **this particular remote**, not universal values: with a brand-new remote of the same model the codes will most likely still be these; with a different model you'll need to read the hexadecimal values in the serial output and change these five lines.

That's why the comment at the top of the file warns that "codes may differ by batch; refer to serial print".

**(2) Waiting one second in `setup()`**

```cpp
  Serial.begin(115200);
  delay(1000);  // Wait for the serial port to stabilize

  Serial.println("IR reception experiment started; point the remote control at the front of the car");
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
```

Printing immediately after `Serial.begin()` can lose the first few characters — USB enumeration hasn't finished yet. `delay(1000)` gives the serial port time to settle.

The second argument of `IrReceiver.begin()`, `DISABLE_LED_FEEDBACK`, switches off the on-board LED's receive flicker. It works fine if you leave it enabled, but the LED blinks on every key press and can easily be confused with the power indicator while debugging.

**(3) `decode()` "asks once"; it doesn't "wait for a frame"**

```cpp
  if (IrReceiver.decode()) {
```

This call returns immediately: if a complete frame happened to be decoded in this pass it returns `true`, otherwise `false`, and execution continues.

Because it is **non-blocking**, the main loop can keep receiving IR while doing other work. The IR remote control car in Project 11 relies on exactly this — it can still check the timeout when no key arrives. There is no `delay()` in `loop()`; a pass takes only a few microseconds, so the jitter of a human key press is captured in full.

**(4) Two useful things from one frame**

```cpp
    uint8_t cmd = IrReceiver.decodedIRData.command;
    bool holding = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
```

`command` is the key code, 8 bits wide, and the same key always sends the same value. The `IRDATA_FLAGS_IS_REPEAT` bit in `flags` marks "this isn't a fresh press, it's a repeat frame from a long press".

Under the NEC protocol, holding a key doesn't keep sending full frames; the remote sends a very short repeat frame every 108 ms. So **a long press still delivers the same `cmd` — only the flags differ**. Without checking that bit, one long press would be treated as dozens of separate presses.

A complete frame lasts 67.5 ms (9 ms leader + 4.5 ms gap + 32 data bits).

When you get garbage, check `protocol` first: if it isn't NEC, the remote uses a different protocol. Then check whether `command` changes every time — if it does, the cause is usually an unstable power supply or the receiver being hit by strong direct light. Sunlight and incandescent lamps both contain energy near 38 kHz that can drown out the signal.

**(5) `static` lets a variable remember the previous frame**

```cpp
    static bool lastHolding = false;  // Whether the previous frame was a long press, to avoid duplicate printing
```

`static` keeps a local variable's value between two `loop()` calls — an ordinary local variable would be reinitialized to `false` every pass and couldn't "remember the previous frame".

Its job is to print "long press active" only once. Holding a key for one second produces nine repeat frames; without this guard, the serial output would be flooded with that line and the key codes you actually want would be pushed away.

**(6) The three print items**

```cpp
      Serial.print("Protocol: ");
      Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
      Serial.print("   Key code: ");
      Serial.print(cmd);
      Serial.print("(0x");
      Serial.print(cmd, HEX);
      Serial.print(")   ");
      Serial.println(keyName(cmd));
```

`getProtocolString()` turns a protocol number into a name such as `NEC`. The same code is printed in two bases: decimal for comparison with the `#define` values, and hexadecimal for comparison with tables in the remote's documentation — either works when editing the macros, but remember that the hex value carries a `0x` prefix.

The second argument of `Serial.print(cmd, HEX)` selects the base; leave it out and the value is decimal.

**(7) Without `resume()`, only the first frame arrives**

```cpp
    IrReceiver.resume();  // Must be called, otherwise only one frame is received
```

After the library receives a frame it stops and waits for you to process it; `resume()` tells it "done, go receive the next frame". Leave this line out and the symptom is very characteristic: the first key press produces output, then nothing responds, as if the receiver had broken. This is the single most commonly missed line in the lesson.

**(8) `keyName()` as a lookup table**

```cpp
const char* keyName(uint8_t cmd) {
  switch (cmd) {
    case KEY_UP:    return "Up";
    case KEY_DOWN:  return "Down";
    case KEY_LEFT:  return "Left";
    case KEY_RIGHT: return "Right";
    case KEY_STOP:  return "OK / Stop";
    default:        return "";
  }
}
```

The return type is `const char*`; `return "Up"` returns the address of a string constant, not a copy of the string. `default` returns an empty string, so an undefined code produces neither garbled output nor a crash.

A `switch` reads better than a chain of `if`s: one code maps to one name, and changing it touches only that line.

#### **8. What You'll See:**

After power on, the serial port prints:

```text
IR receiver experiment started, point the remote at the car
Waiting for a key...
```

Point the remote at the front of the car and press a direction key:

```text
Protocol: NEC   Key code: 70 (0x46)   Up
Protocol: NEC   Key code: 21 (0x15)   Down
Protocol: NEC   Key code: 68 (0x44)   Left
Protocol: NEC   Key code: 67 (0x43)   Right
Protocol: NEC   Key code: 64 (0x40)   OK / Stop
```

Hold **Up** down:

```text
Protocol: NEC   Key code: 70 (0x46)   Up
(Long press active, no more per-frame printing)
```

> [Image to be added] A screenshot of the complete Serial Monitor output after pressing the five direction keys in succession.

#### **9. Troubleshooting:**

If nothing responds, check these three things in order:

(1) Are there batteries in the remote, and has the plastic tab in the battery compartment been pulled out?

(2) Is the receiver's light-sensitive face blocked by the housing? It needs to **see** the remote — a layer of tea-colored acrylic is fine, a layer of black plastic is not.

(3) Distance. IR is effective up to about 8 m indoors, but when aimed straight at strong light (sunlight or an incandescent lamp) it gets drowned out and the range falls to under 1 m.
