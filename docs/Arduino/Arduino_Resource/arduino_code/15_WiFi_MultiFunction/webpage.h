#ifndef WEBPAGE_H
#define WEBPAGE_H

// 网页内容单独存放
// 该 HTML 由板子原样下发，在浏览器执行

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
  <title>迷你履带坦克 V4.0 - Wi-Fi 全功能车</title>

  <style>
    /* 页面整体 */
    body {
      font-family: -apple-system, "Microsoft YaHei", sans-serif;
      text-align: center;
      background: #f2f3f5;
      margin: 0;
      padding: 16px;
    }

    /* 卡片 */
    .card {
      background: #fff;
      border-radius: 14px;
      padding: 16px;
      max-width: 440px;
      margin: 0 auto;
    }

    h2 {
      margin: 4px 0 10px;
      font-size: 20px;
    }

    /* 距离显示 */
    .dist {
      font-size: 16px;
      color: #666;
      margin: 0 0 14px;
    }
    .dist b {
      color: #e53935;
      font-size: 20px;
    }

    /* 按钮公共样式 */
    button {
      font-size: 22px;
      border: none;
      border-radius: 14px;
      color: #fff;
      cursor: pointer;
      -webkit-user-select: none;
      user-select: none;
      -webkit-tap-highlight-color: transparent;
      touch-action: none;       /* 防止按住时页面滑动 */
    }

    /* 模式按钮：两列 */
    .modes {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 8px;
      margin-bottom: 14px;
    }
    .modes button {
      height: 46px;
      font-size: 16px;
      background: #3f51b5;
    }

    /* 选中模式 */
    .on {
      background: #e53935 !important;
    }

    /* 方向键盘：3×3 */
    .grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
    }
    .grid button {
      height: 66px;
      font-size: 20px;
    }

    /* 按键配色 */
    .f { background: #4caf50; }   /* 前进：绿 */
    .b { background: #f44336; }   /* 后退：红 */
    .l { background: #ff9800; }   /* 左转：橙 */
    .r { background: #2196f3; }   /* 右转：蓝 */
    .s { background: #607d8b; }   /* 停车：灰 */
    .p { background: #9c27b0; }   /* 云台：紫 */
    .c { background: #009688; }   /* 夹爪：青 */

    p.tip {
      font-size: 14px;
      color: #999;
      margin: 14px 0 0;
    }
  </style>
</head>

<body>
  <div class="card">

    <h2>当前模式：<span id="m">手动遥控</span></h2>
    <p class="dist">前方距离：<b id="d">--</b> cm</p>

    <!-- 模式切换 -->
    <div class="modes">
      <button id="b-manual" class="on" onclick="mode('manual')">手动遥控</button>
      <button id="b-line"   onclick="mode('line')">循迹行驶</button>
      <button id="b-avoid"  onclick="mode('avoid')">超声波避障</button>
      <button id="b-follow" onclick="mode('follow')">超声波跟随</button>
    </div>

    <!-- 3×3 键盘：上排云台，中间方向，下排夹爪 -->
    <div class="grid">
      <button class="p" onclick="go('servo_plus')">云台◀</button>
      <button class="f" onpointerdown="go('forward')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">↑</button>
      <button class="p" onclick="go('servo_minus')">云台▶</button>

      <button class="l" onpointerdown="go('left')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">←</button>
      <button class="s" onclick="go('stop')">■</button>
      <button class="r" onpointerdown="go('right')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">→</button>

      <button class="c" onclick="go('claw_close')">爪合</button>
      <button class="b" onpointerdown="go('backward')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">↓</button>
      <button class="c" onclick="go('claw_open')">爪开</button>
    </div>

    <p class="tip">按住方向键开车，松手即停 ｜ 云台和夹爪点一下走一步</p>
  </div>

  <script>
    // 模式名映射
    var names = {
      manual: '手动遥控',
      line:   '循迹行驶',
      avoid:  '超声波避障',
      follow: '超声波跟随'
    };

    // 切换模式：更新标题与高亮
    function mode(m) {
      document.getElementById('m').innerText = names[m];

      for (var k in names) {
        document.getElementById('b-' + k).className = (k === m ? 'on' : '');
      }

      fetch('/cmd?mode=' + m).catch(function () {});
    }

    // 发送动作指令
    function go(cmd) {
      fetch('/cmd?move=' + cmd).catch(function () {});
    }

    // 每秒刷新距离
    setInterval(function () {
      fetch('/distance')
        .then(function (r) { return r.text(); })
        .then(function (t) { document.getElementById('d').innerText = t; })
        .catch(function () {});
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

#endif
