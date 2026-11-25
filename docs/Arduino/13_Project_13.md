### Project 13: Wi-Fi Connection and Remote Control

#### **1. Introduction:**

![Wireless router](media/intro_13.jpg){width=520px}

The web page in Project 12 could only push text onto the dot matrix. This time the page reaches out and actually moves hardware: three buttons on the phone turn the pan-tilt, a fourth takes a distance reading, and the value comes straight back to the page — no more squinting at the serial port.

The link is short. Tap a button, the browser sends a request with parameters, and the board turns the servo or measures a distance, then sends the result back as plain text. A round trip on the LAN takes milliseconds, so the delay you actually notice is mostly the servo physically swinging into position.

The mini tracked tank robot itself never moves in this lesson — only the pan-tilt and the ultrasonic module obey the page; the motors wait until the next project. The page and the actions are also split into two files: one describes what the page looks like, the other what each button does, so you can change the interface without touching the actions.

Here's the trap: don't assume the inputs are well-behaved. A URL can be edited by hand, and a sensor with no echo can hold everything up. Both need a guard, or a harmless-looking button will jam the servo or freeze the page.

#### **2. Parts Needed:**

Parts used in this lesson:

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_13.png){width=200px}<br><span style="font-size:0.85em">9G servo (180°, with cross horn) ×1</span> | ![](../media/kit_05.png){width=200px}<br><span style="font-size:0.85em">Ultrasonic module (with housing) ×1</span> | ![](../media/kit_04.png){width=200px}<br><span style="font-size:0.85em">DRV8835 motor driver expansion board ×1</span> |
| ![](../media/kit_03.png){width=200px}<br><span style="font-size:0.85em">ESP32-S3 development board ×1</span> | <span style="font-size:0.85em">2.4 GHz wireless router or phone hotspot ×1</span> |  |
> ⚠️ **The pan-tilt and ultrasonic module were installed back in Chapter 2; this table is for reference only — there is nothing to take apart. The wiring notes below only show how the connections are made, for reference; no assembly or disassembly is required. For the complete parts list, see 1.4 Kit Parts List.**
#### **3. Parameters:**

| Item | Value |
| --- | --- |
| Wi-Fi mode | Station (STA), 2.4 GHz only |
| Web page port | 80 |
| Pan-tilt signal pin | IO42 |
| Pan-tilt range | 0°–180° (0° points right, 180° points left) |
| Ultrasonic Trig / Echo | IO13 / IO12 |
| Distance-measurement timeout | 25 ms |
| Command endpoints | `/pan?angle=N`, `/distance` |
| Response format | `text/plain` |

#### **4. Wiring:**

| Signal | Controller GPIO |
| --- | :---: |
| Pan-tilt servo signal | IO42 |
| Ultrasonic Trig | IO13 |
| Ultrasonic Echo | IO12 |

> **Note**: The components in the wiring diagram are for illustration only; you do not need to disassemble the housed components in the parts list when wiring.

![](media/PRO13.PNG)

#### **5. Flowchart:**

When a web request comes in, `server.handleClient()` looks up the matching callback by URL, runs it and returns:

![](media/wifi_web_flow.png)

#### **6. Test Code:**

> **Before uploading, change one thing**: replace `WIFI_SSID` and `WIFI_PASS` below with your own Wi-Fi name and password — otherwise it won't connect.
>
> Two more conditions must hold at the same time: the Wi-Fi must be **2.4 GHz** (the board doesn't support 5 GHz), and the phone and the robot must be on the **same LAN** — otherwise the browser can't open the board's IP.

> **This project has two files**: `13_WiFi_Connection.ino` and `webpage.h`. All the page HTML lives in `webpage.h`; the `.ino` keeps only the Arduino logic. Put both files in the same folder — if one is missing, compilation fails.

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "webpage.h"  // Web page content

// ===== Wi-Fi =====
// Only 2.4 GHz is supported
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

// ===== Pins =====
// Pan-tilt servo; mounted so 0° points right and 180° points left
#define PAN_SERVO_PIN 42  // Pan-tilt servo

const int trigPin = 13;  // Ultrasonic trigger
const int echoPin = 12;  // Ultrasonic echo

WebServer server(80);  // Web server on port 80
Servo panServo;
int panAngle = 90;     // Current pan-tilt angle, centers on power on

// This lesson uses only the servo and ultrasonic module, no motors

void setup() {
  Serial.begin(115200);
  delay(500);

  // Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Pan-tilt servo
  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(90);

  // Station mode connects to the router
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Block until connected successfully
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Board IP: ");
  Serial.println(WiFi.localIP());   // Open this address in a phone browser

  // Web page routes
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });

  server.on("/pan", []() {
    int a = server.arg("angle").toInt();
    a = constrain(a, 0, 180);   // Limit to 0-180
    panAngle = a;
    panServo.write(a);
    server.send(200, "text/plain", String(a));  // Return the current angle
  });

  server.on("/distance", []() {
    server.send(200, "text/plain", String(getDistance()));
  });

  server.begin();
  Serial.println("Web server started, open the above IP in a phone browser");
}

void loop() {
  server.handleClient();  // Must be called continuously
}

// Distance measurement
int getDistance() {
  long duration;
  int distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 25 ms timeout to avoid blocking when there is no echo
  duration = pulseIn(echoPin, HIGH, 25000);
  distance = duration * 0.034 / 2;

  if (distance < 2 || distance > 300) {
    distance = 0;
  }
  return distance;
}
```

#### **7. Code Explanation:**

**(1) The three libraries**

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "webpage.h"  // Web page content
```

The first two handle Wi-Fi and the web server, the third drives the servo. Quotes around `"webpage.h"` mean it's a file in the current folder, not something loaded from a library.

**(2) Credentials and pins**

```cpp
const char* WIFI_SSID = "Your WiFi name";
const char* WIFI_PASS = "Your WiFi password";

#define PAN_SERVO_PIN 42  // Pan-tilt servo

const int trigPin = 13;  // Ultrasonic trigger
const int echoPin = 12;  // Ultrasonic echo
```

The credentials must be changed to your own, and only 2.4 GHz is supported. The three pins are the same as in Projects 3 and 8: ultrasonic Trig 13, Echo 12, pan-tilt 42.

The "0° points right, 180° points left" in the comment describes the assembly orientation; Project 4 relies on that convention when deciding left from right.

**(3) The `server` object and two global states**

```cpp
WebServer server(80);  // Web server on port 80
Servo panServo;
int panAngle = 90;     // Current pan-tilt angle, centers on power on
```

The 80 in `WebServer server(80)` is the default HTTP port: when you type `http://IP` the browser connects to it automatically, so you don't need to write `:80` in the address.

`panAngle` is only written inside the `/pan` callback and never read elsewhere, so it looks redundant. It's reserved for Projects 14 and 15: those programs need to know "which way the pan-tilt is pointing" from somewhere else, for example to decide left versus right during obstacle avoidance. Storing it now lays the groundwork for later.

**(4) `setup()`: get the ultrasonic and servo ready first**

```cpp
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  ESP32PWM::allocateTimer(0);
  panServo.setPeriodHertz(50);
  panServo.attach(PAN_SERVO_PIN, 500, 2500);
  panServo.write(90);
```

Exactly like Project 8: allocate a timer, set 50 Hz, use 500–2500 µs, and return to center at 90° on power on. This lesson doesn't drive the motors, so there isn't a single `ledcAttach` or motor-pin line.

**(5) Connecting to Wi-Fi: blocking is allowed here**

```cpp
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Block until connected successfully
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
```

The `while` loop blocks until the connection succeeds, printing a dot every half second. Blocking here is acceptable — the web server hasn't started yet, so there are no requests to handle. If it never connects it keeps printing dots; when you see that, check the credentials, the 2.4 GHz band and whether the router has DHCP enabled.

Once connected, `WiFi.localIP()` is the address the phone needs to visit.

**(6) Home page: `send_P` reads from flash**

```cpp
  server.on("/", []() {
    server.send_P(200, "text/html; charset=utf-8", index_html);
  });
```

`server.on(path, function)` only registers, it doesn't execute: it records "when a request like `/` arrives, call this function" in the route table, and not one line of the handler runs at registration time. The real execution happens in `handleClient()` inside `loop()`.

The `_P` in `send_P()` means "send from PROGMEM": it streams the content out over the network while reading it, instead of copying the whole block into RAM first. `200` means "success" — set it to `404` and the page won't display however correct the content is; leave out `charset=utf-8` and any non-ASCII text on the page turns into boxes.

**(7) `/pan`: even a hand-edited URL can't break the servo**

```cpp
  server.on("/pan", []() {
    int a = server.arg("angle").toInt();
    a = constrain(a, 0, 180);   // Limit to 0-180
    panAngle = a;
    panServo.write(a);
    server.send(200, "text/plain", String(a));  // Return the current angle
  });
```

`server.arg("angle")` pulls the parameter out of the URL and `toInt()` converts the string to a number.

**Never drop the `constrain()` line**: the slider on the page is limited to a range, but a URL is something a person can type — enter `?angle=500` and the servo is driven against its stop until the gears grind. After clamping, whatever comes in, the servo receives a legal angle.

The response is plain text rather than a web page because this endpoint is called by JavaScript in the page; sending HTML back would just add an extra parsing step.

**(8) `/distance`: return a single number**

```cpp
  server.on("/distance", []() {
    server.send(200, "text/plain", String(getDistance()));
  });
```

`String(getDistance())` converts the integer to a string before sending. JavaScript in the page receives text such as `"25"` and a `parseInt` turns it into a number.

**(9) `pulseIn()` gains a third argument**

```cpp
  duration = pulseIn(echoPin, HIGH, 25000);
```

Project 3 used the two-argument version, which has a default timeout of one second. Here a 25 ms timeout is added — **the most important change in this lesson**.

It's all about blocking: when there's nothing ahead there is no echo, and the two-argument version waits a full second. During that second `handleClient()` never runs and the page on the phone freezes. 25 ms is enough for a 300 cm round trip (about 17.5 ms); beyond that it returns 0 and the main loop carries on immediately. The price is that targets past roughly four metres can't be measured — they were outside the range anyway.

**(10) Only one line in `loop()`**

```cpp
void loop() {
  server.handleClient();  // Must be called continuously
}
```

It listens on port 80, looks up the path of each incoming request in the route table and calls the matching function. It must run on every pass — add a `delay(1000)` and the phone page will hang for a second before responding.

This single line is also the foundation of Projects 14 and 15: those sketches insert motor control and a state machine around it, but `handleClient()` always stays at the very front.

#### **8. What You'll See:**

After power on, the serial port prints:

```text
Connecting to Your WiFi name
.....
WiFi connected
Board IP: 192.168.1.23
Web server started, open the above IP in a phone browser
```

Open that address on the phone and you'll see the page:

| Action | Result |
| --- | --- |
| Tap **Left 180°** | Pan-tilt turns left and the angle on the page becomes 180 |
| Tap **Center 90°** | Pan-tilt returns to straight ahead |
| Tap **Right 0°** | Pan-tilt turns right |
| Tap **Measure distance** | The value changes from `--` to the current distance (cm) |

> [Image to be added] A photo showing the phone page and the actual robot together, with the pan-tilt at both ends of its travel.

#### **9. Troubleshooting:**

**If Wi-Fi won't connect or the page won't open, check in this order:**

(1) **Did you change the credentials?** If `WIFI_SSID` and `WIFI_PASS` at the top of the code are still placeholders when you flash, it will never connect; replace them with your router's name and password.

(2) **Nothing at all on serial?** Press the RESET button on the board and try again — the previous sketch may be stuck.

![](media/RESET.png){width=340px}

(3) **Dots keep printing on serial, no IP?** Wi-Fi connection failed. The router must be **2.4 GHz**; the board doesn't support 5 GHz. If you have no 2.4 G network, turn on a phone hotspot (hotspots default to 2.4 G).

(4) **Got an IP but the page won't open?** The address must include `http://`, and the IP must be the one printed on serial; the phone must be on the same Wi-Fi, and the first three octets of both IPs must match.

(5) **Same subnet but still won't open?** Log in to the router settings and turn off **AP isolation**; when it's enabled, devices on the same Wi-Fi can't reach one another.

(6) The page opens but the buttons do nothing: open the browser's developer tools and look at the Network tab. If a request stays pending, something is blocking `loop()`; if it returns 404, the path in `server.on()` doesn't match the `fetch` address in the page (check whether there's a leading slash).
