#ifndef WEBPAGE_H
#define WEBPAGE_H

// Web page content is stored separately

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mini Tracked Tank V4.0 - Remote Control Car</title>
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
<h2>Hold a direction button to drive</h2>
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
<p>Release to stop; it also stops if no command arrives for 1.5 s</p>
</div>
<script>
function go(d){
  fetch('/cmd?d='+d).catch(e=>{});
}
</script>
</body></html>
)rawliteral";

#endif
