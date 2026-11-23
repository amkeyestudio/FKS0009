### 项目 9：夹爪控制与抓取

#### **1. 简介：**

车头前面挂着一副爪子：两片亚克力爪片，由一个舵机带动。它只有两个状态——90° 合拢、140° 张开，却是后面抓取物块（第 17 课）和踢球（第 16 课）的基础。

这一课先让爪子空着开合几次，看清两个角度对应的开口，再塞一块橡皮、一个纸团进去，试试夹得住什么。夹爪不是万能的：开口只有二十多毫米，夹持力来自舵机的堵转扭矩，太软的会滑，太硬的会顶住。

要注意的是，夹爪用的舵机和第 8 课的云台是同一种 9G，但行程只走 90°～140° 这 50°，不是 0°～180°。上电先写到 90°，也不能写成 0°——那里是机械死点，齿轮会一直较劲。

![机械臂上的夹爪](media/intro_09.jpg){width=520px}

#### **2. 所需清单：**

本课用到的器件：

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_08.png){width=170px}<br><span style="font-size:0.85em">大型机械夹（半成品） ×1</span> | ![](../media/kit_13.png){width=170px}<br><span style="font-size:0.85em">9G 舵机（180°，配十字臂） ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> |
| ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> |  |  |
> ⚠️ **夹爪在第 2 章就装好了，本表只作对照，不用拆；下面的接线说明只标清接线方式，供对照，不必拆装。完整物料见 1.4 套件清单。**

#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 信号脚 | IO11 |
| 控制频率 | 50 Hz |
| 脉宽范围 | 500～2500 µs |
| 夹紧角度 | 90° |
| 张开角度 | 140° |
| 开合步进 | 每度一次，步间延时 15 ms |
| 单程耗时 | 约 0.75 s（50 度 × 15 ms） |

#### **4. 接线说明：**

| 舵机线 | 颜色（常见） | 接到 |
| --- | --- | --- |
| 信号 | 橙 / 黄 | IO11 |
| 电源 | 红 | 5V |
| 地 | 棕 / 黑 | GND |

![](media/7201.png)

![](media/PRO9.png)

#### **5. 流程图：**

夹爪没有分支，只有一段四拍循环：张开、等、夹紧、等。等的时间决定动作是否看得清。

![](media/gripper_flow.svg)

#### **6. 测试代码：**

> ⚠️ **上传代码前，先把 `mini_tracked_tank_robot.py` 上传到板子的 `lib/` 目录**（方法见第 5.5 节），否则导入库时会报 `ImportError`。

```python
# 项目 9：夹爪控制与抓取
# 接线：夹爪舵机信号线 = IO11
# 现象：夹爪在 90°（夹紧）和 140°（张开）之间来回开合

from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

STEP_MS = 15               # 每转 1 度的延时，越小越快
CLAW_OPEN = 140
CLAW_CLOSE = 90

car = MiniTrackedTankRobot()

# 本课只动夹爪，但电机和云台也要给固定电平：
# 驱动板输入悬空时 B 路轮子会自己转，云台信号悬空会被夹爪的 PWM 干扰
car.Motor_init()
car.stop_motor()
car.Servo_init(42)
car.Servo_set_angle(90)

car.Claw_init(11)
car.Claw_set_angle(CLAW_CLOSE)
time.sleep(1)

while True:
    for angle in range(CLAW_CLOSE, CLAW_OPEN + 1):     # 90° → 140° 张开
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)

    for angle in range(CLAW_OPEN, CLAW_CLOSE - 1, -1): # 140° → 90° 夹紧
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)
```

#### **7. 代码解释：**

**(1) 引入库、常量与步进**

```python
from mini_tracked_tank_robot import MiniTrackedTankRobot
import time

STEP_MS = 15               # 每转 1 度的延时，越小越快
CLAW_OPEN = 140
CLAW_CLOSE = 90

car = MiniTrackedTankRobot()
```

`CLAW_OPEN` 和 `CLAW_CLOSE` 把张开的 140°、夹紧的 90° 写成常量，改开口大小只动这两行。`STEP_MS` 仍是每度延时，调小开合更快。夹爪和云台用的是同一个库、同一种舵机，只是走的角度范围不同。

**(2) 固定电机与云台电平**

```python
# 本课只动夹爪，但电机和云台也要给固定电平：
# 驱动板输入悬空时 B 路轮子会自己转，云台信号悬空会被夹爪的 PWM 干扰
car.Motor_init()
car.stop_motor()
car.Servo_init(42)
car.Servo_set_angle(90)
```

和第 8 课反过来：本课主角是夹爪，电机和云台要稳住。`Motor_init()` 加 `stop_motor()` 防止 B 路轮子悬空自转，`Servo_init(42)` 加 `Servo_set_angle(90)` 把云台固定在 90°，免得信号悬空被夹爪的 PWM 干扰。

**(3) 初始化夹爪、先合到 90°**

```python
car.Claw_init(11)
car.Claw_set_angle(CLAW_CLOSE)
time.sleep(1)
```

`Claw_init(11)` 把夹爪信号脚设为 IO11，`Claw_set_angle(CLAW_CLOSE)` 上电先合到 90°——这是机械夹紧位，不能写成 0°，那里是死点，齿轮会一直较劲。`sleep(1)` 停 1 秒。

**(4) 90° 张开到 140°**

```python
while True:
    for angle in range(CLAW_CLOSE, CLAW_OPEN + 1):     # 90° → 140° 张开
        car.Claw_set_angle(angle)
        time.sleep_ms(STEP_MS)
    time.sleep_ms(500)
```

`range(CLAW_CLOSE, CLAW_OPEN + 1)` 从 90 数到 140（所以要 +1 才含 140），一度一次 `Claw_set_angle(angle)`，配合 `sleep_ms(STEP_MS)` 让爪子平滑张开。到头停 500 ms。

**(5) 140° 合回 90°**

```python
for angle in range(CLAW_OPEN, CLAW_CLOSE - 1, -1): # 140° → 90° 夹紧
    car.Claw_set_angle(angle)
    time.sleep_ms(STEP_MS)
time.sleep_ms(500)
```

步长 −1 从 140 倒数到 90（`CLAW_CLOSE - 1` 是为了含 90）。两段循环拼成不断开合。想夹得更紧可把 `CLAW_CLOSE` 减到 85，但别低于舵机的机械极限。

#### **8. 运行现象：**

上电时爪子先合到 90°，然后开始开合循环，Shell 打印：

```text
夹爪舵机已启动，开始在 90°~140° 之间来回开合...
```

| 阶段 | 现象 |
| --- | --- |
| 上电瞬间 | 合到 90°，爪子完全闭合 |
| 90° → 140° | 张开，约 0.75 s |
| 到 140° | 停 0.5 s |
| 140° → 90° | 合拢，约 0.75 s |
| 循环 | 不断开合 |

> 【图片待补】夹爪开合过程中间帧的实拍图（约 115°）。

#### **9. 常见问题解决：**

(1) Shell 报 `NameError: name 'CLAW_OPEN' isn't defined`，是角度常量没在开头定义就拿去用了，把 `CLAW_OPEN = 140`、`CLAW_CLOSE = 90` 补到代码前面。

(2) 上电爪子完全没反应，先查夹爪舵机的信号线是不是接在 IO11——`Claw_init(11)` 只认这个脚，插错或没接，爪子不会动。

(3) 运行报 `ImportError: no module named 'mini_tracked_tank_robot'`，是板级库没进板子，用 Thonny 的「保存到板子」把库文件放到 lib/ 目录再跑。

(4) 爪子张得太大夹不住东西，把 `CLAW_OPEN` 从 140 往回调小一点，比如 130，张角收一点夹得更稳；`CLAW_CLOSE` 别低于 85，顶到机械极限舵机会一直较劲发热。
