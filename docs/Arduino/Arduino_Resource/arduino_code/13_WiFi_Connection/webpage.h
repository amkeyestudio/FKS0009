#ifndef WEBPAGE_H
#define WEBPAGE_H

// Web page content is stored separately

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mini Tracked Tank V4.0 - Wi-Fi Connection</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;text-align:center;background:#f2f3f5;margin:0;padding:16px}
.card{background:#fff;border-radius:14px;padding:16px;max-width:420px;margin:0 auto}
h2{margin:4px 0 16px;font-size:20px}
button{height:56px;font-size:17px;border:none;border-radius:12px;color:#fff;background:#3f51b5;margin:4px;cursor:pointer}
.row button{width:30%}
.wide{width:100%;background:#009688;margin-top:16px}
p{font-size:17px;margin:12px 0}
b{color:#e53935;font-size:22px}
</style></head><body>
<div class="card">
<h2>Mini Tracked Tank V4.0</h2>
<p>Pan angle: <b id="a">90</b> °</p>
<div class="row">
<button onclick="pan(180)">Left 180°</button>
<button onclick="pan(90)">Center 90°</button>
<button onclick="pan(0)">Right 0°</button>
</div>
<button class="wide" onclick="readD()">Measure Distance</button>
<p>Front distance: <b id="d">--</b> cm</p>
</div>
<script>
function pan(v){
  fetch('/pan?angle='+v).then(r=>r.text()).then(t=>document.getElementById('a').innerText=t);
}
function readD(){
  document.getElementById('d').innerText='...';
  fetch('/distance').then(r=>r.text()).then(t=>document.getElementById('d').innerText=t);
}
</script>
</body></html>
)rawliteral";

#endif
