### 项目 6：循迹传感器

#### **1. 简介：**

把车翻过来，朝地面那一面有一排五只黑色小方块。每只里面是一对红外管：一只往下照，一只看反射回来的光——地面白就反光，黑就吸掉，接收管把这个差别变成高电平或低电平。

五只探头要是各占一根引脚，主控又得腾地方，所以它们全挂在一颗 PCF8574 上，只用两根线（IO8、IO9）把读数送回来。这五路其实是一个字节里的五个位，拼起来就是从 0 到 31 的一个数。

先不急着让车跑，只把五路读数打到 Shell 上，每 200 毫秒一行。车放在白纸上五个都是 0；用手指或黑胶带挡在哪颗探头下面，对应那一位就变 1；整排压在黑线上，五个全变 1。

五个 0 和 1 从左到右排开，就是车对脚下地面的全部认知——下一课开车要靠它。

#### **2. 所需清单：**

本课用到的器件：

|   |   |   |
| :---: | :---: | :---: |
| ![](../media/kit_02.png){width=170px}<br><span style="font-size:0.85em">履带底盘总成（含循迹传感器） ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> | ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> |

> ⚠️ **这套传感器在第 2 章装配底盘时已经固定好、线也走好了，本表只作对照，不用拆；下面的接线说明只标清接线方式，供对照，不必拆装。完整物料见 1.4 套件清单。**

#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 扩展芯片 | PCF8574，I2C 转 8 位并行 |
| I2C 地址 | 0x20（A2 A1 A0 全接地） |
| SDA / SCL | IO8 / IO9 |
| 总线速率 | 400 kHz（快速模式） |
| 使用通道 | 5 路：P4 P0 P1 P2 P3（左 → 右） |
| 电平逻辑 | 压黑线读 0，代码取反后为 1 |
| 打印周期 | 200 ms |

#### **4. 接线说明：**

| 信号 | 主控 GPIO | 说明 |
| --- | :---: | --- |
| SDA | IO8 | 与点阵 DIN 共用，本课不用点阵 |
| SCL | IO9 | 与点阵 CLK 共用，本课不用点阵 |
| VCC | 5V | 由扩展板供给 |
| GND | GND | 共地 |

#### **5. 流程图：**

读取流程只有三步，但位序和取反两处最容易弄错：

![](media/track_read_flow.png)

#### **6. 测试代码：**

> ⚠️ **上传代码前，先把 `mini_tracked_tank_robot.py` 上传到板子的 `lib/` 目录**（方法见第 5.5 节），否则导入库时会报 `ImportError`。

```python
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
car.Line_init(9, 8)

time.sleep(1)

while True:
    A, B, C, D, E = car.Line_get()
    print('A:{}  B:{}  C:{}  D:{}  E:{}'.format(A, B, C, D, E))
    time.sleep_ms(200)
```

#### **7. 代码解释：**

**(1) 引入板级库、建对象**

```python
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

car = MiniTrackedTankRobot()
```

和前面几课一样，从板级库取出 `MiniTrackedTankRobot` 类建 `car` 对象，循迹读数由它提供。库文件要先传到板子 `lib/` 目录，否则导入报错。

**(2) 初始化循迹**

```python
car.Line_init(9, 8)

time.sleep(1)
```

`Line_init(9, 8)` 把循迹用的 I2C 设成 SCL=IO9、SDA=IO8（PCF8574 的地址 0x20 在库里写死），顺序 SCL 在前、SDA 在后。后面的 `time.sleep(1)` 等 1 秒，让 PCF8574 上电稳定后再读，避免开头几帧读到乱值。

**(3) 读五路探头**

```python
A, B, C, D, E = car.Line_get()
```

`Line_get()` 一次返回五个探头的电平，按从左到右赋给 A～E。库内部已经把电平取反，所以这里 1 表示压到黑线、0 表示白线，直接用就行，不用自己再取反。

**(4) 打印与延时**

```python
print('A:{}  B:{}  C:{}  D:{}  E:{}'.format(A, B, C, D, E))
time.sleep_ms(200)
```

`format` 把五个值填进字符串，Shell 一次打一行，便于对着看。`time.sleep_ms(200)` 让每 200 ms 打印一次，想看得更细就调小它。把手指挡在某颗探头下，对应位置会从 0 跳成 1。

#### **8. 运行现象：**

上电后Shell 每 200 ms 打印一行，五个数字对应从左到右五个探头：

```text
A:0  B:0  C:1  D:0  E:0
A:0  B:1  C:1  D:0  E:0
A:0  B:1  C:0  D:1  E:0
A:1  B:1  C:1  D:1  E:1
```

把车放在白纸上，五个都是 0；用手指或黑胶带挡在某一颗探头下面，对应位置变 1；把整排压在黑线上，五个全变 1。

> 【图片待补】整车压在黑色循迹线上、串口五行全为 1 的实拍图（含串口画面）。

#### **9. 常见问题解决：**

(1) 在 Thonny 里点运行报 `ImportError: no module named 'mini_tracked_tank_robot'`，是板级库没保存到板子。用 Thonny 的**保存到板子**把 `mini_tracked_tank_robot.py` 放进 lib/ 目录，再运行一次就好了。

(2) Thonny 下方的 Shell 里五个数一直是 0，手指挡上去也不变，先查循迹板的 4P 插头有没有插到底，再看 SDA / SCL 是不是插反——代码里 `Line_init(9, 8)` 是 SCL=IO9、SDA=IO8 的顺序，I2C 接反不会烧板子，但一个字节也读不回来。

(3) 读数偶尔跳变，多半是探头离地太高或者环境光太强。循迹靠反射光工作，探头离地保持在 5～8 mm，正午太阳直射会让它一直读到白，挪到阴影里或者遮一下光再试。

(4) Shell 报 `NameError: name 'A' isn't defined`，是还没执行 `A, B, C, D, E = car.Line_get()` 就拿 A 去 print 了，把读探头的那句补到打印前面。
