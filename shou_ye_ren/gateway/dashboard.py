# -*- coding: utf-8 -*-
"""网关 Web 看板：一个页面看完全局状态。

为什么要有：演示现场未必能装 APP（签名、机型、网络都可能拦），
但评委的手机、电脑浏览器一定能开。看板只依赖三个已有 JSON 接口，
纯静态 HTML + 原生 fetch，不引外部 CDN，断网也能正常渲染。

页面每 3 秒自动刷新；离线超过阈值时顶部变黄，提示"数据已过期"。
"""
from __future__ import annotations

# 用 __TOKEN__ 占位再替换，避免 HTML 里的花括号和 str.format 打架。
_TPL = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>守夜灯 · 网关看板</title>
<style>
  :root{
    --bg:#f2f4f7; --card:#ffffff; --ink:#0f172a; --muted:#64748b;
    --line:#e2e8f0; --ok:#10b981; --warn:#f59e0b; --bad:#ef4444;
    --brand:#0b1c3d; --accent:#e2570f;
  }
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  body{margin:0;background:var(--bg);color:var(--ink);
       font-family:-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;
       padding:0 0 28px}
  header{background:var(--brand);color:#fff;padding:16px 18px 14px}
  header h1{margin:0;font-size:22px;letter-spacing:1px}
  header p{margin:4px 0 0;font-size:13px;color:#9fb0cc}
  .dot{display:inline-block;width:9px;height:9px;border-radius:50%;
       background:var(--ok);margin-right:7px;vertical-align:middle}
  .wrap{max-width:760px;margin:0 auto;padding:0 14px}
  .card{background:var(--card);border-radius:16px;padding:16px;
        margin-top:14px;box-shadow:0 1px 2px rgba(15,23,42,.06)}
  .hero{display:flex;align-items:center;gap:14px}
  .hero .big{font-size:30px;font-weight:700;line-height:1.2}
  .hero .sub{font-size:14px;color:var(--muted);margin-top:6px}
  .pill{font-size:12px;padding:3px 10px;border-radius:999px;
        background:#eef2f7;color:var(--muted)}
  .grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:4px}
  .metric{padding:10px 4px;text-align:center}
  .metric .v{font-size:24px;font-weight:700}
  .metric .k{font-size:12px;color:var(--muted);margin-top:2px}
  .note{margin-top:12px;padding:10px 12px;border-radius:12px;
        background:#fff8ec;color:#8a5b00;font-size:13px;display:none}
  .alarm{display:flex;gap:10px;padding:10px 0;border-bottom:1px solid var(--line)}
  .alarm:last-child{border-bottom:none}
  .lv{flex:none;width:44px;height:26px;border-radius:8px;color:#fff;
      font-size:12px;font-weight:700;display:flex;align-items:center;
      justify-content:center}
  .lv.L3{background:var(--bad)} .lv.L2{background:var(--warn)} .lv.L1{background:#3b82f6}
  .alarm .t{font-size:15px;font-weight:600}
  .alarm .d{font-size:12px;color:var(--muted);margin-top:3px}
  .empty{color:var(--muted);font-size:14px;padding:8px 0}
  table{width:100%;border-collapse:collapse;font-size:13px}
  th,td{text-align:left;padding:7px 4px;border-bottom:1px solid var(--line)}
  th{color:var(--muted);font-weight:600;font-size:12px}
  code{background:#eef2f7;border-radius:6px;padding:1px 6px;font-size:12px}
  .tools{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
  .tools a{font-size:13px;text-decoration:none;color:#1d4ed8;
           background:#eef2ff;padding:7px 12px;border-radius:10px}
  footer{text-align:center;color:var(--muted);font-size:12px;margin-top:18px}
</style>
</head>
<body>
<header>
  <h1>守夜灯 · 网关看板</h1>
  <p><span class="dot" id="hd"></span><span id="hstate">连接中…</span>
     <span id="hup" style="margin-left:8px"></span></p>
</header>

<div class="wrap">
  <div class="card" id="devCard" style="display:none">
    <div class="pill">这台网关带的设备</div>
    <select id="devSel" style="margin-top:8px;width:100%;padding:10px 12px;
            border:1px solid var(--line);border-radius:10px;font-size:15px;
            background:#f8fafc;color:var(--ink)"></select>
  </div>

  <div class="card">
    <div class="hero">
      <div>
        <div class="big" id="headline">读取中…</div>
        <div class="sub" id="headlineSub"></div>
      </div>
    </div>
    <div class="note" id="staleNote"></div>
  </div>

  <div class="card">
    <div class="grid">
      <div class="metric"><div class="v" id="mMotion">—</div><div class="k">人体活动</div></div>
      <div class="metric"><div class="v" id="mTemp">—</div><div class="k">温度</div></div>
      <div class="metric"><div class="v" id="mHum">—</div><div class="k">湿度</div></div>
      <div class="metric"><div class="v" id="mLux">—</div><div class="k">光照</div></div>
      <div class="metric"><div class="v" id="mDoor">—</div><div class="k">门磁</div></div>
      <div class="metric"><div class="v" id="mDev">—</div><div class="k">设备</div></div>
    </div>
  </div>

  <div class="card">
    <div class="pill">待处理告警</div>
    <div id="pending" style="margin-top:8px"></div>
  </div>

  <div class="card">
    <div class="pill">最近告警</div>
    <div style="margin-top:10px;overflow:auto">
      <table>
        <thead><tr><th>时间</th><th>等级</th><th>类型</th><th>状态</th></tr></thead>
        <tbody id="recent"></tbody>
      </table>
    </div>
  </div>

  <div class="card">
    <div class="pill">接口速查</div>
    <div class="tools">
      <a href="/health">/health</a>
      <a href="/api/v1/device">/api/v1/device</a>
      <a href="/api/v1/snapshot">/api/v1/snapshot</a>
      <a href="/api/v1/alarms?limit=20">/api/v1/alarms</a>
      <a href="/api/v1/reports/daily">/daily</a>
      <a href="/api/v1/reports/monthly">/monthly</a>
    </div>
    <div class="sub" style="margin-top:10px;font-size:12px">
      数据来源：<code>__BASE__</code> · 本页 3 秒自动刷新
    </div>
  </div>

  <footer>守夜灯 · 独居老人居家安全监测 · 本页仅作演示与自查，不做医疗诊断</footer>
</div>

<script>
var LV_NAME = {L1:"提醒", L2:"关注", L3:"紧急"};
var ST_NAME = {pending:"待处理", acknowledged:"已确认", closed:"已关闭"};
var lastOk = 0;

function el(id){ return document.getElementById(id); }
function setState(ok, text){
  el("hd").style.background = ok ? "var(--ok)" : "var(--bad)";
  el("hstate").textContent = text;
}
function fmt(n, unit){
  if(n === null || n === undefined) return "—";
  return (Math.round(n*10)/10) + (unit || "");
}
function shortTime(ts){
  if(!ts) return "—";
  var s = String(ts);
  return s.length >= 16 ? s.substring(5, 16).replace("T", " ") : s;
}

function renderDevice(d){
  el("mDev").textContent = d.online ? "在线" : "离线";
  el("mDev").style.color = d.online ? "var(--ok)" : "var(--bad)";
  var stale = (d.staleSeconds === null || d.staleSeconds === undefined) ? 999 : d.staleSeconds;
  if(!d.online || stale > 10){
    el("staleNote").style.display = "block";
    el("staleNote").textContent = "数据已过期：设备离线，以下为最后一次上报（" +
      Math.round(stale/60) + " 分钟前）";
  } else {
    el("staleNote").style.display = "none";
  }
}

function render(s, d, pending){
  if(s){
    el("mMotion").textContent = s.pirDetected ? "有活动" : "无活动";
    el("mTemp").textContent = fmt(s.temperature, "℃");
    el("mHum").textContent  = fmt(s.humidity, "%");
    el("mLux").textContent  = s.lux === null || s.lux === undefined ? "—" : (Math.round(s.lux) + "lx");
    el("mDoor").textContent = s.doorOpen === null || s.doorOpen === undefined
      ? "—" : (s.doorOpen ? "已开" : "已关");
  }
  if(d) renderDevice(d);

  var n = pending.length;
  if(n > 0){
    var top = pending[0];
    for(var i=1;i<pending.length;i++){
      if(LV_NAME[pending[i].level] && pending[i].level === "L3"){ top = pending[i]; }
    }
    el("headline").textContent = "有 " + n + " 条待确认告警";
    el("headline").style.color = "var(--bad)";
    el("headlineSub").textContent = "最高等级：" + (LV_NAME[top.level] || top.level) +
      " · " + (top.message || top.type);
  } else if(d && d.online){
    el("headline").textContent = "一切正常";
    el("headline").style.color = "var(--ok)";
    el("headlineSub").textContent = "最后活动 " + (s && s.lastMotionTs ? shortTime(s.lastMotionTs) : "未知") +
      " · " + (s && s.lastMotionZone ? s.lastMotionZone : "未知区域") +
      " · " + (d ? "在线" : "离线");
  } else {
    el("headline").textContent = "设备离线";
    el("headline").style.color = "var(--warn)";
    el("headlineSub").textContent = d ? ("最后一次上报 " + shortTime(d.lastSeen)) : "尚未收到任何数据";
  }

  var box = el("pending");
  if(n === 0){
    box.innerHTML = '<div class="empty">没有待处理的告警</div>';
  } else {
    box.innerHTML = pending.map(function(a){
      return '<div class="alarm"><div class="lv ' + a.level + '">' + a.level + '</div>' +
        '<div><div class="t">' + (a.message || a.type) + '</div>' +
        '<div class="d">' + shortTime(a.ts) + ' · ' + (a.type || '') +
        ' · 编号 ' + (a.eventId || '') + '</div></div></div>';
    }).join("");
  }
}

function renderRecent(list){
  var tbody = el("recent");
  if(!list.length){
    tbody.innerHTML = '<tr><td colspan="4" class="empty">暂无告警记录</td></tr>';
    return;
  }
  tbody.innerHTML = list.map(function(a){
    return "<tr><td>" + shortTime(a.ts) + "</td><td>" + (a.level || "") +
      "</td><td>" + (a.type || "") + "</td><td>" +
      (ST_NAME[a.status] || a.status || "") + "</td></tr>";
  }).join("");
}

function getJSON(url){
  return fetch(url, {cache:"no-store"}).then(function(r){
    if(!r.ok) throw new Error(url + " " + r.status);
    return r.json();
  });
}

// ---- 多设备（多老人）：一台网关带两台时出现下拉 ----
var devId = "";

function esc(s){
  return String(s).replace(/[&<>"]/g, function(c){
    return {"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;"}[c];
  });
}

function withDev(url){          // 调用时 url 以 ? 结尾，便于无过滤时保持合法
  return devId ? url + "deviceId=" + encodeURIComponent(devId) : url;
}

function loadDevices(){
  return getJSON("/api/v1/devices").then(function(d){
    var items = (d && d.items) || [];
    var card = el("devCard"), sel = el("devSel");
    if(items.length < 2){           // 只有一台：不打扰
      card.style.display = "none";
      devId = "";
      return;
    }
    card.style.display = "";
    sel.innerHTML = items.map(function(x){
      return '<option value="' + esc(x.deviceId) + '">' + esc(x.deviceId) +
             (x.online ? " · 在线" : " · 离线") + "</option>";
    }).join("");
    var exists = items.some(function(x){ return x.deviceId === devId; });
    if(!exists){
      devId = items[0].deviceId;
    }
    sel.value = devId;
  }).catch(function(){});
}

el("devSel").addEventListener("change", function(){
  devId = this.value;
  tick();
});

function tick(){
  loadDevices().then(function(){
  Promise.all([
    getJSON(withDev("/api/v1/snapshot?")).catch(function(){ return null; }),
    getJSON(withDev("/api/v1/device?")).catch(function(){ return null; }),
    getJSON(withDev("/api/v1/alarms?status=pending&limit=20&")).catch(function(){ return {items:[]}; }),
    getJSON(withDev("/api/v1/alarms?limit=8&")).catch(function(){ return {items:[]}; }),
    getJSON("/health").catch(function(){ return null; })
  ]).then(function(r){
    render(r[0], r[1], (r[2] && r[2].items) || []);
    renderRecent((r[3] && r[3].items) || []);
    if(r[4]){
      lastOk = Date.now();
      setState(true, "网关在线 · 已运行 " + Math.round(r[4].uptime/60) + " 分钟");
      el("hup").textContent = "";
    } else {
      setState(false, "网关无响应");
    }
  }).catch(function(){
    setState(false, "网关无响应");
  });
  });
}

tick();
setInterval(tick, 3000);
</script>
</body>
</html>
"""


def render(base_url=""):
    """返回看板 HTML；base_url 只用于页脚展示当前实际访问地址。"""
    return _TPL.replace("__BASE__", base_url or "当前地址")
