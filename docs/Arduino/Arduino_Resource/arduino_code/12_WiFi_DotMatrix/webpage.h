#ifndef WEBPAGE_H
#define WEBPAGE_H

// Web page content stored separately

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dot Matrix Message</title>
<style>
body{font-family:-apple-system,"Microsoft YaHei",sans-serif;background:#1a1a1a;color:#ddd;
     text-align:center;margin:0;padding:20px}
.card{background:#242424;border-radius:14px;padding:18px;max-width:420px;margin:0 auto}
h2{margin:0 0 14px;font-size:18px;color:#4dd0e1;font-weight:500}
input{width:100%;box-sizing:border-box;height:52px;font-size:20px;text-align:center;
      border:none;border-radius:10px;background:#111;color:#eee;outline:none;
      letter-spacing:2px;text-transform:uppercase}
button{margin-top:12px;width:100%;height:52px;font-size:19px;border:none;border-radius:10px;
       background:#00897b;color:#fff;cursor:pointer}
p{font-size:13px;color:#777;margin:14px 0 0;line-height:1.6}
</style></head><body>
<div class="card">
<h2>Dot Matrix Message</h2>
<form action="/msg" method="get">
  <input name="t" maxlength="24" placeholder="Enter letters or numbers" autocomplete="off">
  <button type="submit">Send to Dot Matrix</button>
</form>
<p>Only uppercase letters, numbers, and spaces are supported, up to 24 characters<br>
Short text (fits on one screen) is displayed statically in the center; long text (such as LOVE) scrolls horizontally</p>
</div>
</body></html>
)rawliteral";

#endif
