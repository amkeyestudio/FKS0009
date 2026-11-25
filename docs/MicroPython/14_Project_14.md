### 项目 14：Wi-Fi 控车

#### **1. 简介：**

![用手机操控机器人](media/intro_14.jpg){width=520px}

手机当遥控器，这次控制的是车。页面上一组方向键，按住车走，松开车停。比红外遥控器好用在于 Wi-Fi 没有方向性——隔着墙、背对着车也能控制，只要还在同一个局域网里。

代价是延迟不固定：一个指令要经路由器转一圈，快的时候二十几毫秒，忙起来上百毫秒，还可能丢包。所以规则和红外遥控车一样：1.5 秒没有新指令就自动停车。

按键方式也不一样。红外按一下发一帧，网页按钮是按下、松开各发一次，所以前进和停止是两条独立指令，松手时浏览器一定补一条停。

按住上，两轮同速前进；按左，两轮一正一反原地左转；松手立停。把手机 Wi-Fi 关掉，车会在 1.5 秒后自己停下来。

#### **2. 所需清单：**

本课用到的器件：

|     |     |
| :---: | :---: |
| ![](../media/kit_04.png){width=200px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> | ![](../media/kit_02.png){width=200px}<br><span style="font-size:0.85em">履带底盘总成 ×1</span> |
| ![](../media/kit_03.png){width=200px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> | <span style="font-size:0.85em">2.4 GHz 无线路由器或手机热点 ×1</span> |
> ⚠️ **全是第 2 章装好的器件，本表只作对照，不用拆；下面的接线说明只标清接线方式，供对照，不必拆装。完整物料见 1.4 套件清单。**

#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 网页端口 | 80 |
| 直行速度 | 180 |
| 原地转速度 | 180 |
| 指令超时 | 1500 ms |
| 指令格式 | `/cmd?d=f|b|l|r|s` |
| 左轮 AIN / AEN | IO38 / IO21 |
| 右轮 BIN / BEN | IO40 / IO41 |

#### **4. 接线说明：**

| 信号 | 主控 GPIO |
| --- | :---: |
| 左轮 AIN（方向） | IO38 |
| 左轮 AEN（调速） | IO21 |
| 右轮 BIN（方向） | IO40 |
| 右轮 BEN（调速） | IO41 |

这一课不接传感器，只接电机。Wi-Fi 是片内功能。

![](media/PRO14.png)

#### **5. 流程图：**

从手指按下到轮子转起来，中间隔着浏览器、路由器和板子三层：

![](media/wifi_car_flow.png)

#### **6. 测试代码：**

> ⚠️ **上传代码前，先把 `mini_tracked_tank_robot.py`、`mini_tracked_tank_robot_web.py` 上传到板子的 `lib/` 目录**（方法见第 5.5 节），否则导入库时会报 `ImportError`。

```python
import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer

WIFI_SSID = '你的WiFi账号'
WIFI_PASS = '你的WiFi密码'

SPEED_RUN = 180
SPEED_TURN = 180
STOP_AFTER = 1500          # 指令超时（ms）自动停车

INDEX_HTML = """<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>迷你履带坦克 V4.0 - 遥控车</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;
     background:#f2f3f5;margin:0;padding:16px}
.card{background:#fff;border-radius:14px;padding:16px;max-width:420px;margin:0 auto}
h2{margin:4px 0 16px;font-size:20px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{height:72px;font-size:24px;border:none;border-radius:14px;color:#fff;
       user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none}
.f{background:#4caf50}.b{background:#f44336}.l{background:#ff9800}
.r{background:#2196f3}.s{background:#607d8b}
.blank{background:transparent}
p{font-size:15px;color:#888;margin-top:14px}
</style></head><body>
<div class="card">
<h2>按住方向键开车</h2>
<div class="grid">
<div class="blank"></div>
<button class="f" onpointerdown="go('f')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">↑</button>
<div class="blank"></div>
<button class="l" onpointerdown="go('l')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">←</button>
<button class="s" onclick="go('s')">■</button>
<button class="r" onpointerdown="go('r')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">→</button>
<div class="blank"></div>
<button class="b" onpointerdown="go('b')" onpointerup="go('s')" onpointercancel="go('s')" onpointerleave="go('s')">↓</button>
<div class="blank"></div>
</div>
<p>松手自动停，1.5 秒收不到指令也会停</p>
</div>
<script>
function go(d){ fetch('/cmd?d='+d).catch(function(){}); }
</script>
</body></html>"""

car = MiniTrackedTankRobot()
car.Motor_init()
car.stop_motor()

last_cmd = time.ticks_ms()


def on_index(params):
    return INDEX_HTML


def on_cmd(params):
    global last_cmd
    last_cmd = time.ticks_ms()
    cmd = params.get('d', 's')

    if cmd == 'f':
        car.forward(SPEED_RUN, SPEED_RUN)
    elif cmd == 'b':
        car.back(SPEED_RUN, SPEED_RUN)
    elif cmd == 'l':
        car.left(SPEED_TURN, SPEED_TURN)
    elif cmd == 'r':
        car.right(SPEED_TURN, SPEED_TURN)
    else:
        car.stop_motor()
    return 'OK'


# ==================== 连接 Wi-Fi ====================
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
print('正在连接', WIFI_SSID)
wlan.connect(WIFI_SSID, WIFI_PASS)
while not wlan.isconnected():
    time.sleep(0.5)
    print('.', end='')
print()
print('板子的 IP：' + wlan.ifconfig()[0])

server = WebServer(80)
server.route('/', on_index)
server.route('/cmd', on_cmd)

print('遥控车已就绪')

while True:
    server.handle()

    # 超时没有指令就停车
    if time.ticks_diff(time.ticks_ms(), last_cmd) > STOP_AFTER:
        car.stop_motor()
```

#### **7. 代码解释：**

**(1) 引入库与速度参数**

```python
import network
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from mini_tracked_tank_robot_web import WebServer
```

仍用 `network` 连 Wi-Fi、`WebServer` 开网页，电机驱动在 `MiniTrackedTankRobot` 里。前面三个速度/超时常量集中放在一起：直行和转向都用 180，`STOP_AFTER = 1500` 是指令超时（毫秒）。嫌车太快或太慢，改这几个数即可。

**(2) 初始化电机与计时基准**

```python
car = MiniTrackedTankRobot()
car.Motor_init()
car.stop_motor()

last_cmd = time.ticks_ms()
```

`Motor_init()` 初始化电机引脚，`stop_motor()` 一上电先把车停住，避免复位瞬间电机乱转。`last_cmd` 记最后一次收到指令的时间，用 `ticks_ms()` 取毫秒数——后面**1.5 秒没指令就停车**全靠它。

**(3) 网页回调：首页与指令**

```python
def on_index(params):
    return INDEX_HTML

def on_cmd(params):
    global last_cmd
    last_cmd = time.ticks_ms()
    cmd = params.get('d', 's')

    if cmd == 'f':
        car.forward(SPEED_RUN, SPEED_RUN)
    elif cmd == 'b':
        car.back(SPEED_RUN, SPEED_RUN)
    elif cmd == 'l':
        car.left(SPEED_TURN, SPEED_TURN)
    elif cmd == 'r':
        car.right(SPEED_TURN, SPEED_TURN)
    else:
        car.stop_motor()
    return 'OK'
```

`on_index()` 返回首页 HTML。`on_cmd()` 处理 `/cmd?d=X`：每次收到指令先把 `last_cmd` 刷成当前时间（等于告诉主循环"刚收到指令"），再按 `d` 分发——`f/b/l/r` 分别调 `car.forward/back/left/right`，其他值（含网页松手时发的 `s`）一律 `stop_motor()`。默认值 `'s'` 保证参数缺失时车是停的。

**(4) 网页按钮：按下走、松手停**

```python
function go(d){ fetch('/cmd?d='+d).catch(function(){}); }
```

网页上每个方向键都绑了 `onpointerdown` 和 `onpointerup`：按住发 `go('f')` 前进，松开（以及取消、手指移出）发 `go('s')` 停车。`go()` 就是拼出 `/cmd?d=f` 这样的请求发给板子。`.catch()` 吞掉网络异常——松手那个包丢了也不影响页面。这样比**点一下走一步**更接近真实遥控。

**(5) 连接 Wi-Fi 与注册路由**

```python
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
print('正在连接', WIFI_SSID)
wlan.connect(WIFI_SSID, WIFI_PASS)
while not wlan.isconnected():
    time.sleep(0.5)
    print('.', end='')
print()
print('板子的 IP：' + wlan.ifconfig()[0])

server = WebServer(80)
server.route('/', on_index)
server.route('/cmd', on_cmd)
```

前三课通用的连网段落：`STA_IF` 客户端模式连 2.4 G 路由器，循环打点等 `isconnected()`，`ifconfig()[0]` 拿 IP 打印。然后 `WebServer(80)` 在 80 端口服务，`route('/')` 返回页面、`route('/cmd')` 收行车指令。

**(6) 主循环与超时保护**

```python
while True:
    server.handle()

    # 超时没有指令就停车
    if time.ticks_diff(time.ticks_ms(), last_cmd) > STOP_AFTER:
        car.stop_motor()
```

`server.handle()` 反复轮询网页请求。第二句是安全兜底：`ticks_diff()` 算**现在距上次指令过了多久**，超过 `STOP_AFTER`（1500 ms）就 `stop_motor()`。Wi-Fi 会丢包、手机会锁屏、人会走出信号范围，这些情况下**松手**的停车指令可能永远发不到板子；有了超时，车最长走 1.5 秒就会自己停下，不会一路冲出去。改 `STOP_AFTER` 可以调这个保护的灵敏程度。

#### **8. 运行现象：**

Shell 打印：

```text
正在连接 你的WiFi名字
.....
WiFi 已连接
板子的 IP：192.168.1.23
遥控车已就绪
```

手机页面上一组方向键，按住不放车持续走，松手立停。

| 操作 | 串口 | 车 |
| --- | --- | --- |
| 按 ↑ | 前进 | 两轮正转 |
| 按 ↓ | 后退 | 两轮反转 |
| 按 ← | 左转 | 原地左转 |
| 按 → | 右转 | 原地右转 |
| 松手 / 按停 | 停车 | 立即停 |
| 强行关掉手机 Wi-Fi | 无输出 | 1.5 s 后自己停 |

> 【图片待补】手机上方向键页面与小车行驶的同框实拍图。

#### **9. 常见问题解决：**

**Wi-Fi 连不上、网页打不开，按这个顺序查：**

(1) **账号密码换了没有**：代码开头的 `WIFI_SSID`、`WIFI_PASS` 还是占位符就运行，一定连不上，改成自己路由器的名称和密码。

(2) **Shell 里一行字都没有**：按一下板子上的 RESET 键再试，可能是上一版程序卡住了。

![](media/RESET.png){width=340px}

(3) **Shell 一直冒点、拿不到 IP**：Wi-Fi 连接失败。路由器必须是 **2.4 GHz**，5 GHz 板子不支持；没有 2.4 G 就开手机热点（热点默认 2.4 G）。

(4) **拿到 IP 但页面打不开**：地址要带 `http://`，IP 以 Shell 打印的为准；手机必须连同一个 Wi-Fi，两边 IP 前三段要一样。

(5) **网段对、还是打不开**：进路由器后台把**AP 隔离**关掉，开了它同一 Wi-Fi 下的设备互相访问不了。

(6) Shell 报 `ImportError: no module named 'mini_tracked_tank_robot_web'`，是 WebServer 库没进板子。把 `mini_tracked_tank_robot.py` 和 `mini_tracked_tank_robot_web.py` 用 Thonny 保存到板子的 `lib/` 下再运行。

(7) 车动起来一顿一顿的，多半是路由器信号弱或者 2.4 G 信道太挤。把手机和车挪近点，或者进路由器后台换个信道（1、6、11 三个互不重叠）。

(8) 转弯时一侧轮子明显慢，先查电池——Wi-Fi 发射瞬间电流能到 300 mA，电量低时电压被拉垮，PWM 输出跟着塌，一侧就转不动了。给电池充饱或换一块再试。
