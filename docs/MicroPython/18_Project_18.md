### 项目 18：AI 视觉模块：摄像头巡线

#### **1. 简介：**

![路上的自动驾驶车](media/intro_18.jpg){width=520px}

第 7 课用五只贴地的红外探头循迹，这里把眼睛抬起来——改用摄像头看线。镜头朝地面，能看到车头前方十几厘米的线，比贴地探头看得远，可以提前打方向。

模块回传的不是图像，是三个数：偏差、角度、有效位。偏差说线偏出画面中心多少，角度说线歪了多少，有效位等于 5 就表示这一帧真的看到了线。有这两个量，就能算左右轮该差多少速。

修正用的是比例加微分：偏差大就多打方向，偏差小就轻轻修；再加一项看偏差变得快不快，提前收住，车身才不会冲过中线再来回摆。直行基础速度 130，修正量最多 ±90。

上电后点阵先是一双眼睛，等线进入视野才变笑脸、开始沿线走；线跑出画面时它按记忆的方向原地找，找回来接着走，找不回来就停下、换一张苦脸。

#### **2. 所需清单：**

本课用到的器件：

|     |     |     |
| :---: | :---: | :---: |
| ![](../media/kit_09.jpg){width=170px}<br><span style="font-size:0.85em">TinyVision AI 视觉摄像头 ×1</span> | ![](../media/kit_04.png){width=170px}<br><span style="font-size:0.85em">DRV8835 电机扩展板 ×1</span> | ![](../media/kit_02.png){width=170px}<br><span style="font-size:0.85em">履带底盘总成 ×1</span> |
| ![](../media/kit_06.png){width=170px}<br><span style="font-size:0.85em">8×16 LED 点阵屏 ×1</span> | ![](../media/kit_03.png){width=170px}<br><span style="font-size:0.85em">ESP32-S3 开发板 ×1</span> |  |

#### **3. 参数：**

| 项目 | 参数 |
| --- | --- |
| 识别模式 | `line` |
| 回传格式 | `LINE:偏差,角度,有效位` |
| 有效标志 | 5 = 识别到线 |
| 直行基础速度 | 130 |
| 比例系数 KP | 0.4 |
| 微分系数 KD | 0.6 |
| 修正上限 | ±90 |
| 微分限幅 | ±50 |
| 找线速度 | 90 |
| 找线死区 | 15 |
| 找线超时 | 600 ms |
| 控制周期 | 5 ms |

#### **4. 接线说明：**

| 视觉模块 | 主控 | 说明 |
| --- | :---: | --- |
| TX | IO18 | 回传偏差数据 |
| RX | IO17 | 下发模式指令 |
| VCC / GND | 5V / GND | 供电与共地 |

| 信号 | 主控 GPIO |
| --- | :---: |
| 左轮 AIN / AEN | IO38 / IO21 |
| 右轮 BIN / BEN | IO40 / IO41 |
| 点阵 CLK / DIN | IO9 / IO8 |


**装法A：**

![](media/A-1.png)

![](media/A-2.png)

![](media/A-3.png)

![](media/A-4.png)

![](media/IMG_9076.jpg)

![](media/A-6.png)

#### **5. 流程图：**

有线和没线是两条完全不同的路径，先进哪条由**有效位**决定：

![](media/camera_line_flow.svg)

#### **6. 测试代码：**

> ⚠️ **上传代码前，先把 `mini_tracked_tank_robot.py`、`aicam.py` 上传到板子的 `lib/` 目录**（方法见第 5.5 节），否则导入库时会报 `ImportError`。

```python
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

BASE_SPEED = 130           # 直行基础速度
KP = 0.4                   # 比例系数
KD = 0.6                   # 微分系数，抑制超调
TURN_MAX = 90              # 单侧修正上限
D_MAX = 50                 # 微分项限幅

SEARCH_SPEED = 90          # 找线转速
SEARCH_DEAD = 15           # 找线死区
LOST_TIMEOUT = 600         # 找线超时（ms）

LINE_OK = 5                # 模块第三字段为 5 表示识别到线

DEBUG = False              # 调试开关：改成 True 可打印视觉模块回传的循线数据

FACE_EYES = 0
FACE_SMILE = 1
FACE_SURPRISE = 2
FACE_SAD = 3

eyes = bytes([
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])
surprise = bytes([
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x00, 0xC0,
    0xC0, 0x00, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x00
])
sad = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x80, 0x40,
    0x40, 0x80, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00
])

car = MiniTrackedTankRobot()
car.Motor_init()
car.Matrix_init(9, 8)
car.stop_motor()

cam = AiCam()
cam.begin(tx=17, rx=18)
cam.setMode(cam.LINE)

cur_face = -1
last_error = 0
has_last = False
lost_start = 0
dbg_prev = None            # 上次打印的循线数据，只在新数据到达时打印一次


def show_face(face):
    global cur_face
    if face == cur_face:
        return
    cur_face = face
    if face == FACE_EYES:
        car.Matrix_display(eyes)
    elif face == FACE_SMILE:
        car.Matrix_display(smile)
    elif face == FACE_SURPRISE:
        car.Matrix_display(surprise)
    else:
        car.Matrix_display(sad)


show_face(FACE_EYES)
time.sleep(1)              # 等模块切换模式完成

while True:
    cam.read()

    offset = cam.lineOffset()      # 负偏左，正偏右
    valid = cam.lineValid()

    if DEBUG:
        cur = (offset, valid, cam.lineAngle())
        if cur != dbg_prev:
            print('循线数据：offset=%d angle=%d valid=%d' % (cur[0], cur[2], cur[1]))
            dbg_prev = cur

    if valid == LINE_OK:
        # 识别到线：PD 换算差速
        lost_start = 0
        show_face(FACE_SMILE)

        error = offset

        d_error = 0
        if has_last:
            d_error = error - last_error
            if d_error > D_MAX:
                d_error = D_MAX
            elif d_error < -D_MAX:
                d_error = -D_MAX

        correction = int(error * KP + d_error * KD)

        if correction > TURN_MAX:
            correction = TURN_MAX
        elif correction < -TURN_MAX:
            correction = -TURN_MAX

        car.drive(BASE_SPEED + correction, BASE_SPEED - correction)

        last_error = error
        has_last = True

    else:
        # 未识别到线：按上次偏差方向找线
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < LOST_TIMEOUT:
            show_face(FACE_SURPRISE)
            if last_error > SEARCH_DEAD:
                car.drive(SEARCH_SPEED, -SEARCH_SPEED)     # 线原在右
            elif last_error < -SEARCH_DEAD:
                car.drive(-SEARCH_SPEED, SEARCH_SPEED)     # 线原在左
            else:
                car.stop_motor()                           # 方向不明
        else:
            show_face(FACE_SAD)
            car.stop_motor()

    time.sleep_ms(5)
```

#### **7. 代码解释：**

**(1) 引入库与 PD 参数**

```python
import time
from mini_tracked_tank_robot import MiniTrackedTankRobot
from aicam import AiCam

BASE_SPEED = 130           # 直行基础速度
KP = 0.4                   # 比例系数
KD = 0.6                   # 微分系数，抑制超调
TURN_MAX = 90              # 单侧修正上限
D_MAX = 50                 # 微分项限幅

SEARCH_SPEED = 90          # 找线转速
SEARCH_DEAD = 15           # 找线死区
LOST_TIMEOUT = 600         # 找线超时（ms）

LINE_OK = 5                # 模块第三字段为 5 表示识别到线

DEBUG = False              # 调试开关：改成 True 可打印视觉模块回传的循线数据
```

`KP` 把"偏了多少"换成转向力度，`KD` 看偏差的变化趋势，专门压住快回到线时冲过头（超调）。两个 `*_MAX` 是保险：修正量再大也别把一侧轮子拉到极限，否则原地打滑、线反而丢。`LINE_OK = 5` 是和视觉模块约定好的"识别到线"状态值，不是普通的真/假。调参先动 `KP` 再动 `KD`；`DEBUG` 改成 `True` 能打印模块回传的原始数据，调完记得改回 `False`。

**(2) 四种表情字模**

```python
eyes = bytes([
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00
])
smile = bytes([
    0x00, 0x00, 0x1C, 0x02, 0x02, 0x02, 0x5C, 0x40,
    0x40, 0x5C, 0x02, 0x02, 0x02, 0x1C, 0x00, 0x00
])
surprise = bytes([
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x00, 0xC0,
    0xC0, 0x00, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x00
])
sad = bytes([
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x80, 0x40,
    0x40, 0x80, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00
])
```

字模格式同第 1 课。这里用四种表情反馈车的判断：笑脸＝稳稳跟线，惊讶＝丢线正在找，哭脸＝找超时停车，眼睛＝上电待机。

**(3) 初始化与状态变量**

```python
car = MiniTrackedTankRobot()
car.Motor_init()
car.Matrix_init(9, 8)
car.stop_motor()

cam = AiCam()
cam.begin(tx=17, rx=18)
cam.setMode(cam.LINE)

cur_face = -1
last_error = 0
has_last = False
lost_start = 0
dbg_prev = None            # 上次打印的循线数据，只在新数据到达时打印一次
```

视觉模块切到 `LINE`（巡线）模式，串口脚和前一课一样。`last_error` 和 `has_last` 是 PD 里微分项要用的"上一帧偏差"，第一帧没有上一帧就不算微分；`lost_start` 记丢线的起点；`dbg_prev` 让调试打印只在新数据到来时出现一次，避免刷屏。

**(4) 读模块回传的循线数据**

```python
while True:
    cam.read()

    offset = cam.lineOffset()      # 负偏左，正偏右
    valid = cam.lineValid()

    if DEBUG:
        cur = (offset, valid, cam.lineAngle())
        if cur != dbg_prev:
            print('循线数据：offset=%d angle=%d valid=%d' % (cur[0], cur[2], cur[1]))
            dbg_prev = cur
```

`offset` 是线相对画面中心的横向偏差，负在左、正在右；`valid` 表示这一帧有没有可靠地看到线，只有等于 `LINE_OK` 才算数。`lineAngle()` 这里只用于调试打印，真正参与控制的只有 `offset`，所以先调 `KP/KD` 时不必管角度。

**(5) 把 PD 换算成左右轮差速**

```python
    if valid == LINE_OK:
        # 识别到线：PD 换算差速
        lost_start = 0
        show_face(FACE_SMILE)

        error = offset

        d_error = 0
        if has_last:
            d_error = error - last_error
            if d_error > D_MAX:
                d_error = D_MAX
            elif d_error < -D_MAX:
                d_error = -D_MAX

        correction = int(error * KP + d_error * KD)

        if correction > TURN_MAX:
            correction = TURN_MAX
        elif correction < -TURN_MAX:
            correction = -TURN_MAX

        car.drive(BASE_SPEED + correction, BASE_SPEED - correction)

        last_error = error
        has_last = True
```

看到线先清 `lost_start`（说明没丢线）。P 项 `error * KP`：线偏右就给右轮加速、左轮减速，车往右修；D 项 `d_error * KD` 是这一帧偏差比上一帧变化了多少，偏差正快速减小时它反向抵消一部分，防止冲过线后来回摆，`D_MAX` 防止某帧跳变把 D 项撑爆。`correction` 再被 `TURN_MAX` 限幅，最后用 `drive(BASE+修正, BASE-修正)` 做差速——两轮速度和不变、只改差值，车才平顺沿弧线转弯，而不是忽快忽慢。末尾把这一帧存进 `last_error`，供下一帧算微分。

**(6) 丢线：按记忆方向找线，超时停车**

```python
    else:
        # 未识别到线：按上次偏差方向找线
        if lost_start == 0:
            lost_start = time.ticks_ms()

        if time.ticks_diff(time.ticks_ms(), lost_start) < LOST_TIMEOUT:
            show_face(FACE_SURPRISE)
            if last_error > SEARCH_DEAD:
                car.drive(SEARCH_SPEED, -SEARCH_SPEED)     # 线原在右
            elif last_error < -SEARCH_DEAD:
                car.drive(-SEARCH_SPEED, SEARCH_SPEED)     # 线原在左
            else:
                car.stop_motor()                           # 方向不明
        else:
            show_face(FACE_SAD)
            car.stop_motor()

    time.sleep_ms(5)
```

丢线不是立刻停车，而是用 `last_error` 记住线原来在哪边，朝那边原地转（两轮反向）去找，最多找 `LOST_TIMEOUT`（600 ms）。`SEARCH_DEAD` 是死区：偏差太小说明方向不明，硬转反而乱，干脆停住。超过 600 ms 还没找到就彻底停车、显示哭脸。末尾 `sleep_ms(5)` 是主循环节拍：太长来不及跟线，太短又白占 CPU。

#### **8. 运行现象：**

Thonny 下方的 Shell 只在开头打印一次模块启动信息，跑起来后只在数据变化时才打一行循线数据。

| 状态 | 点阵 | 行为 |
| --- | --- | --- |
| 待机（刚上电） | 眼睛 | 等待线进入视野 |
| 识别到线 | 笑脸 | PD 循线，速度 130 |
| 丢线找线中 | 瞪眼 | 按记忆方向原地转 |
| 找线超时 | 苦脸 | 停车 |

> 【图片待补】小车用摄像头循迹跑完整圈的实拍图（直道 + 弯道各一张）。

调参按这个顺序：

1. **先调 KP**。把 KD 设为 0，从 0.2 开始往上加，直到车能跟线但过弯画龙。
2. **再加 KD**。从 0.3 起加，观察画龙是否收敛。KD 太大车会抖——每轮修正过猛，像抽筋。
3. **最后调速度**。`BASE_SPEED` 从 130 提到 150，如果弯道冲出去，说明速度超过了视野能覆盖的范围，退回来。

摄像头循迹对光线比红外敏感得多。地面反光、阴影边缘都可能被认成线。测试时用均匀的白炽灯或自然光，别在窗边有强反光的地方跑。

#### **9. 常见问题解决：**

(1) 一运行就报 `ImportError`，找不到 `aicam` 或 `mini_tracked_tank_robot`，是库文件没进板子的 `lib/`。用 Thonny 把 `aicam.py` 和 `mini_tracked_tank_robot.py` 另存到板子文件系统的 `lib/` 目录，再重新运行。

(2) `AiCam` 初始化卡住、Shell 里读不到画面数据，先查接线：视觉模块 TX 接 IO18、RX 接 IO17，两根交叉接，波特率 9600。模块压根没回应按 `OSError: [Errno 19] ENODEV` 处理，基本是线松或接反，理顺了再**运行/保存到板子**。

(3) 车过弯画龙、左右摆，调 `KP` 和 `KD`。现在 `KP = 0.4`、`KD = 0.6`，先固定 KD 把 KP 往下压到车能跟线，再加 KD 收敛画龙；KD 给太大车会发抖像抽筋，退回来一点。

(4) 老丢线、转半天找不回来，看 `SEARCH_DEAD = 15`、`SEARCH_SPEED = 90` 和 `LOST_TIMEOUT = 600`：死区太小车会来回找，超时短了就直接停车。地面反光或阴影边缘也常被认成线，换均匀白光再测。
