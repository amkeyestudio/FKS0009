#ifndef WEBPAGE_H
#define WEBPAGE_H

// Web page content is stored separately
// This HTML is sent unchanged by the board and runs in the browser

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
  <title>Mini Tracked Tank V4.0 - Wi-Fi Multi-Function Car</title>

  <style>
    /* Overall page */
    body {
      font-family: -apple-system, "Microsoft YaHei", sans-serif;
      text-align: center;
      background: #f2f3f5;
      margin: 0;
      padding: 16px;
    }

    /* Card */
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

    /* Distance display */
    .dist {
      font-size: 16px;
      color: #666;
      margin: 0 0 14px;
    }
    .dist b {
      color: #e53935;
      font-size: 20px;
    }

    /* Common button style */
    button {
      font-size: 22px;
      border: none;
      border-radius: 14px;
      color: #fff;
      cursor: pointer;
      -webkit-user-select: none;
      user-select: none;
      -webkit-tap-highlight-color: transparent;
      touch-action: none;       /* Prevent page scrolling while a button is held */
    }

    /* Mode buttons: two columns */
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

    /* Selected mode */
    .on {
      background: #e53935 !important;
    }

    /* Direction keypad: 3x3 */
    .grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
    }
    .grid button {
      height: 66px;
      font-size: 20px;
    }

    /* Button colors */
    .f { background: #4caf50; }   /* Forward: green */
    .b { background: #f44336; }   /* Backward: red */
    .l { background: #ff9800; }   /* Turn left: orange */
    .r { background: #2196f3; }   /* Turn right: blue */
    .s { background: #607d8b; }   /* Stop: gray */
    .p { background: #9c27b0; }   /* Pan-tilt: purple */
    .c { background: #009688; }   /* Gripper: cyan */

    p.tip {
      font-size: 14px;
      color: #999;
      margin: 14px 0 0;
    }
  </style>
</head>

<body>
  <div class="card">

    <h2>Current mode: <span id="m">Manual Control</span></h2>
    <p class="dist">Front distance: <b id="d">--</b> cm</p>

    <!-- Mode switching -->
    <div class="modes">
      <button id="b-manual" class="on" onclick="mode('manual')">Manual Control</button>
      <button id="b-line"   onclick="mode('line')">Line Tracking</button>
      <button id="b-avoid"  onclick="mode('avoid')">Ultrasonic Obstacle Avoidance</button>
      <button id="b-follow" onclick="mode('follow')">Ultrasonic Following</button>
    </div>

    <!-- 3x3 keypad: pan-tilt on top, direction in the middle, gripper at the bottom -->
    <div class="grid">
      <button class="p" onclick="go('servo_plus')">Pan ◀</button>
      <button class="f" onpointerdown="go('forward')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">↑</button>
      <button class="p" onclick="go('servo_minus')">Pan ▶</button>

      <button class="l" onpointerdown="go('left')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">←</button>
      <button class="s" onclick="go('stop')">■</button>
      <button class="r" onpointerdown="go('right')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">→</button>

      <button class="c" onclick="go('claw_close')">Close Gripper</button>
      <button class="b" onpointerdown="go('backward')" onpointerup="go('stop')"
              onpointerleave="go('stop')" onpointercancel="go('stop')">↓</button>
      <button class="c" onclick="go('claw_open')">Open Gripper</button>
    </div>

    <p class="tip">Hold a direction button to drive; release to stop | Tap the pan-tilt and gripper buttons for one step</p>
  </div>

  <script>
    // Mode name mapping
    var names = {
      manual: 'Manual Control',
      line:   'Line Tracking',
      avoid:  'Ultrasonic Obstacle Avoidance',
      follow: 'Ultrasonic Following'
    };

    // Switch mode: update the title and highlight
    function mode(m) {
      document.getElementById('m').innerText = names[m];

      for (var k in names) {
        document.getElementById('b-' + k).className = (k === m ? 'on' : '');
      }

      fetch('/cmd?mode=' + m).catch(function () {});
    }

    // Send a motion command
    function go(cmd) {
      fetch('/cmd?move=' + cmd).catch(function () {});
    }

    // Refresh the distance every second
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
