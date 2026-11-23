#ifndef WEBPAGE_H
#define WEBPAGE_H

// 网页内容单独存放

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>FKS0009 遥控车</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;background:#f2f3f5;margin:0;padding:16px}
.card{background:#fff;border-radius:14px;padding:16px;max-width:420px;margin:0 auto}
h2{margin:4px 0 16px;font-size:20px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{height:72px;font-size:24px;border:none;border-radius:14px;color:#fff;cursor:pointer;
       -webkit-user-select:none;user-select:none;-webkit-tap-highlight-color:transparent;touch-action:none}
.f{background:#4caf50}.b{background:#f44336}.l{background:#ff9800}
.r{background:#2196f3}.s{background:#607d8b}
.blank{background:transparent;pointer-events:none}
p{font-size:15px;color:#888;margin-top:14px}
</style></head><body>
<div class="card">
<h2>按住方向键开车</h2>
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
<p>松手自动停，1.5 秒收不到指令也会停</p>
</div>
<script>
function go(d){
  fetch('/cmd?d='+d).catch(e=>{});
}
</script>
</body></html>
)rawliteral";

#endif
