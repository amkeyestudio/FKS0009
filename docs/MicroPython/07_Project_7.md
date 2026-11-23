### 项目 7：循迹行驶

#### **1. 简介：**

把车摆上黑色循迹跑道，按下复位，它自己沿着线跑完一整圈：直道加速、缓弯减速贴回去、直角弯原地拧过去。上一课只是把五路读数打到 Shell，这一课才让这些读数真正去开车。

车怎么知道线在哪边？五个探头各有 0 和 1，组合起来有 32 种，实际跑起来会遇到十来种。代码把五路压成一个五位二进制数 `state`，用 `switch` 一次判完——比写十几个 `if` 清爽得多。这是全课的核心，读懂它就读懂了循迹。

真正难的是「线突然没了」。接缝、反光、过弯抬头，都会让五路瞬间全 0，此时车不知道该往哪拐。代码记着上一次线在左还是在右（`lastDir`），给它 1.5 s 按原方向继续找；找到了接着跑，超时还没找到才停车认输。少了这一步，车会一路走走停停。

#### **2. 所需清单：**

本课用到的器件：

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_02.png){width=170px}<br><span style="font-size:0.85em">履带底盘总成（含循迹传感器） ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> | ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> |
> ⚠️ **本课不新增器件，全都是第 2 章装好的东西，不用拆；下面的接线说明只标清接线方式，供对照，不必拆装。完整物料见 1.4 套件清单。**

#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 直行速度 | 115 |
| 微调速度 | 内侧 85、外侧 115 |
| 急转速度 | 内侧 45、外侧 125 |
| 原地转速度 | 内侧反转 95、外侧正转 125 |
| 丢线记忆时限 | 1500 ms |
| 控制周期 | 5 ms |
| 速度范围 | −255 ～ +255（负数即反转） |

#### **4. 接线说明：**

| 信号 | 主控 GPIO |
| --- | :---: |
| SDA（循迹） | IO8 |
| SCL（循迹） | IO9 |
| 左轮 AIN / AEN | IO38 / IO21 |
| 右轮 BIN / BEN | IO40 / IO41 |

#### **5. 流程图：**

五位编码 `state` 是整个程序的分岔口，先看这张图再读 `switch`：

![](media/track_car_flow.svg)

#### **6. 测试代码：**

> ⚠️ **上传代码前，先把 `mini_tracked_tank_robot.py` 上传到板子的 `lib/` 目录**（方法见第 5.5 节），否则导入库时会报 `ImportError`。

```python
# 项目 7：循迹行驶
# 接线：PCF8574 的 SCL=IO9、SDA=IO8；电机 40/41/38/21
# 现象：五路探头居中直行；偏左左调、偏右右调；完全脱线按最后方向找线，1.5 秒还没找到就停车

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Line_init(9, 8)

BASE_SPEED = 115           # 直行基础速度
RECOVER_IN = 95            # 急转时内侧轮反转速度
RECOVER_OUT = 125          # 急转时外侧轮正转速度

last_dir = 0               # 0 直行，1 线在左，2 线在右
lost_start = 0             # 丢线起始时刻，0 表示未丢线

car.stop_motor()
print('循迹开始')
time.sleep(2)              # 对齐 Arduino：上电 2 秒后开始循迹

while True:
    A, B, C, D, E = car.Line_get()
    state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E

    if state != 0:
        lost_start = 0     # 压到线，清丢线计时

    if state in (0b00100, 0b01110, 0b11111):
        car.drive(BASE_SPEED, BASE_SPEED)          # 居中直行
        last_dir = 0

    elif state == 0b01100:
        car.drive(85, BASE_SPEED)                  # 偏左：左轮降速
        last_dir = 1

    elif state in (0b01000, 0b11100):
        car.drive(45, RECOVER_OUT)                 # 偏左较大
        last_dir = 1

    elif state in (0b11000, 0b10000):
        car.drive(-RECOVER_IN, RECOVER_OUT)        # 严重偏左：原地左转
        last_dir = 1

    elif state == 0b00110:
        car.drive(BASE_SPEED, 85)                  # 偏右：右轮降速
        last_dir = 2

    elif state in (0b00010, 0b00111):
        car.drive(RECOVER_OUT, 45)                 # 偏右较大
        last_dir = 2

    elif state in (0b00011, 0b00001):
        car.drive(RECOVER_OUT, -RECOVER_IN)        # 严重偏右：原地右转
        last_dir = 2

    elif state == 0b00000:
        # 五路全部脱线：按记忆方向找线
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < 1500:
            if last_dir == 1:
                car.drive(-RECOVER_IN, RECOVER_OUT)    # 线原在左
            elif last_dir == 2:
                car.drive(RECOVER_OUT, -RECOVER_IN)    # 线原在右
            else:
                car.stop_motor()                       # 原为直行，停车
        else:
            car.stop_motor()                           # 超时未找到，停车

    # 其余组合（如 0b01111、0b11110）是探头过渡态，保持上一条电机指令不动

    time.sleep_ms(5)
```

#### **7. 代码解释：**

**(1) 引入板级库，初始化循迹与电机**

```python
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Motor_init()
car.Line_init(9, 8)
```

`MiniTrackedTankRobot` 把电机、循迹、舵机都封在一个类里，`Motor_init()` 先把电机引脚设好，`Line_init(9, 8)` 再告诉库循迹的 SCL 接 IO9、SDA 接 IO8——顺序不能反。换接线就改这两个数字。

**(2) 速度常量、丢线状态与起步延时**

```python
BASE_SPEED = 115           # 直行基础速度
RECOVER_IN = 95            # 急转时内侧轮反转速度
RECOVER_OUT = 125          # 急转时外侧轮正转速度

last_dir = 0               # 0 直行，1 线在左，2 线在右
lost_start = 0             # 丢线起始时刻，0 表示未丢线

car.stop_motor()
print('循迹开始')
time.sleep(2)              # 对齐 Arduino：上电 2 秒后开始循迹
```

三个速度常量集中在开头，调车的快慢只改这几行：`BASE_SPEED` 管直行，`RECOVER_IN/OUT` 管急转时内侧反转和外侧正转。`last_dir` 记住线最后在哪边，`lost_start` 记丢线起点、0 表示没丢。起步先 `stop_motor()` 再 `sleep(2)`，是给自己两秒把手放开，免得一上电车就窜出去。

**(3) 五路读数压成一个五位二进制数**

```python
A, B, C, D, E = car.Line_get()
state = (A << 4) | (B << 3) | (C << 2) | (D << 1) | E

if state != 0:
    lost_start = 0     # 压到线，清丢线计时
```

`Line_get()` 一次返回五个探头的 0/1，左移按位或后拼成一个 0～31 的 `state`：A 是最高位、E 是最低位，哪几路压线看对应的 1 在哪。后面所有判断都只看这一个数。只要 `state` 不为 0（还压着线），就把 `lost_start` 清零，表示这次丢线的计时作废。

**(4) 居中直行与偏左分级修正**

```python
if state in (0b00100, 0b01110, 0b11111):
    car.drive(BASE_SPEED, BASE_SPEED)          # 居中直行
    last_dir = 0

elif state == 0b01100:
    car.drive(85, BASE_SPEED)                  # 偏左：左轮降速
    last_dir = 1

elif state in (0b01000, 0b11100):
    car.drive(45, RECOVER_OUT)                 # 偏左较大
    last_dir = 1

elif state in (0b11000, 0b10000):
    car.drive(-RECOVER_IN, RECOVER_OUT)        # 严重偏左：原地左转
    last_dir = 1
```

`car.drive(左轮, 右轮)` 两个参数是左右轮速度，负数表示反转。中间三路压线（`0b00100` 等）说明车在线上，两轮同速直行。线往左偏按程度分三级：先让左轮降到 85 轻微回正，偏得多让左轮降到 45、右轮提到 125，最严重时左轮反转 95、右轮正转 125，原地往左拧。每一级都把 `last_dir` 记成 1，给后面丢线找线留方向。改这些速度就能调转向的急缓。

**(5) 偏右分级修正**

```python
elif state == 0b00110:
    car.drive(BASE_SPEED, 85)                  # 偏右：右轮降速
    last_dir = 2

elif state in (0b00010, 0b00111):
    car.drive(RECOVER_OUT, 45)                 # 偏右较大
    last_dir = 2

elif state in (0b00011, 0b00001):
    car.drive(RECOVER_OUT, -RECOVER_IN)        # 严重偏右：原地右转
    last_dir = 2
```

偏右和偏左完全对称：右轮依次降到 85、45，最后反转 95，同时左轮保持或升到 125，`last_dir` 记成 2。注意 `drive()` 的参数顺序是 `drive(左, 右)`，写反了车会往反方向拐。

**(6) 丢线找线与控制周期**

```python
elif state == 0b00000:
    # 五路全部脱线：按记忆方向找线
    if lost_start == 0:
        lost_start = time.ticks_ms()

    if time.ticks_diff(time.ticks_ms(), lost_start) < 1500:
        if last_dir == 1:
            car.drive(-RECOVER_IN, RECOVER_OUT)    # 线原在左
        elif last_dir == 2:
            car.drive(RECOVER_OUT, -RECOVER_IN)    # 线原在右
        else:
            car.stop_motor()                       # 原为直行，停车
    else:
        car.stop_motor()                           # 超时未找到，停车

# 其余组合（如 0b01111、0b11110）是探头过渡态，保持上一条电机指令不动

time.sleep_ms(5)
```

五路全 0 是脱线，第一次进来用 `time.ticks_ms()` 记下时刻。`time.ticks_diff()` 算到现在过了多久：不到 1500 ms 就照 `last_dir` 往原方向转着找线，超过就 `stop_motor()` 停车认输。`ticks_ms/ticks_diff` 是 MicroPython 标准库的毫秒计时，溢出也能正确算差。没列到的过渡态（如 `0b01111`）什么都不做，保持上一次的电机指令，车身不会一抖一停。最后 `sleep_ms(5)` 定住 5 ms 的控制周期，调小转向更灵敏、调大更稳但反应慢。

#### **8. 运行现象：**

Thonny 下方的 Shell 只在开头打印一次「循迹开始」，跑起来后不再输出——主循环里没有 `print()`，5 ms 一轮的节奏留给电机。

| 场景 | 现象 |
| --- | --- |
| 车压中线 | 直行，速度 115 |
| 缓弯 | 内侧轮降速，车身平滑贴回 |
| 直角弯 | 内侧轮短暂反转，原地拧过去 |
| 冲出线头 | 原地转向找线，1.5 s 内找到就继续 |
| 彻底脱线 | 1.5 s 后停车 |

> 【图片待补】小车在 A2 寻迹跑道上完整跑一圈的实拍图（可拍弯道与直道各一张）。

#### **9. 常见问题解决：**

(1) 运行报 `ImportError: no module named 'mini_tracked_tank_robot'`，是板级库没进板子，用 Thonny 的「保存到板子」把库文件放到 lib/ 目录，重跑即可。

(2) 车在直线上左右画龙，先把 `BASE_SPEED` 从 115 往下降，比如 100 再试，同时看看探头是不是离地太远——放得高反射弱，方向也容易飘。

(3) 过缓弯时车切内道直接冲出去，把 `RECOVER_OUT` 从 125 提到 135，让外侧轮转得更快，车身就能贴回线上。

(4) 直角弯里车在原地拧却拧不过去，把 `RECOVER_IN` 从 95 提到 110，反转那一侧更有力。

(5) 整个车往反方向拐，先查 A、B 两路电机线是不是接反了，把两个电机的 2P 插头对调就行，不用改代码；要是线没问题，再查 `car.drive()` 的两个参数，它规定是 `drive(左, 右)`，写反了也会反着走。
