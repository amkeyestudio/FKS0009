#ifndef WEBPAGE_H
#define WEBPAGE_H

// Web page content is stored separately

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mini Tracked Tank V4.0 - Soccer Robot</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;background:#f2f3f5;margin:0;padding:14px}
.card{background:#fff;border-radius:14px;padding:14px;max-width:420px;margin:0 auto 12px}
h2{margin:2px 0 12px;font-size:19px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:9px}
button{height:66px;font-size:22px;border:none;border-radius:14px;color:#fff;cursor:pointer;
       -webkit-user-select:none;user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none}
.f{background:#4caf50}.b{background:#f44336}.l{background:#ff9800}
.r{background:#2196f3}.s{background:#607d8b}
.g{background:#009688}.h{background:#9c27b0}.w{background:#ff5722}.k{background:#795548}
.blank{background:transparent;pointer-events:none}
.score{font-size:40px;font-weight:700;color:#e53935;margin:6px 0}
p{font-size:13px;color:#888;margin:8px 0 0}
</style></head><body>

<div class="card">
<h2>Score</h2>
<div class="score" id="sc">0</div>
<p id="st">Hold a direction button to drive</p>
</div>

<div class="card">
<h2>Drive</h2>
<div class="grid">
<div class="blank"></div>
<button class="f" onpointerdown="go('f')" onpointerup="go('s')" onpointerleave="go('s')" onpointercancel="go('s')">↑</button>
<div class="blank"></div>
<button class="l" onpointerdown="go('l')" onpointerup="go('s')" onpointerleave="go('s')" onpointercancel="go('s')">←</button>
<button class="s" onclick="go('s')">■</button>
<button class="r" onpointerdown="go('r')" onpointerup="go('s')" onpointerleave="go('s')" onpointercancel="go('s')">→</button>
<div class="blank"></div>
<button class="b" onpointerdown="go('b')" onpointerup="go('s')" onpointerleave="go('s')" onpointercancel="go('s')">↓</button>
<div class="blank"></div>
</div>
</div>

<div class="card">
<h2>Gripper</h2>
<div class="grid" style="grid-template-columns:repeat(2,1fr)">
<button class="g" onclick="go('carry')">Dribble</button>
<button class="w" onclick="go('shoot')">Shoot</button>
</div>
<div class="grid" style="grid-template-columns:repeat(2,1fr);margin-top:9px">
<button class="k" onclick="go('goal')">Add a Goal</button>
<button class="k" onclick="go('reset')">Reset Score</button>
</div>
<p>Tap "Dribble" to open the gripper and hold the ball, then use the direction buttons to dribble; tap "Shoot" to close the gripper and push the ball out</p>
</div>

<script>
function go(d){
  fetch('/cmd?d='+d).then(r=>r.text()).then(t=>{
    if(t.indexOf(',')>0){
      var a=t.split(',');
      document.getElementById('sc').innerText=a[0];
      document.getElementById('st').innerText=a[1];
    }
  }).catch(e=>{});
}
function tick(){
  fetch('/score').then(r=>r.text()).then(t=>{
    var a=t.split(',');
    document.getElementById('sc').innerText=a[0];
  }).catch(e=>{});
}
setInterval(tick,1000);
</script>
</body></html>
)rawliteral";

#endif
