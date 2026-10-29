# 一键演示：把网关、Web 看板、模拟器一次拉起
#
# 用法（在 software 目录下）：
#   powershell -ExecutionPolicy Bypass -File scripts\demo.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -NoSim     # 只起网关
#   powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -Stop      # 全部停掉
#   powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -Port 8790 # 换端口
#   powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -Siren -Voice   # 带声音（演示用这个）
#
# 现场为什么用它：演示最怕"忘了先起哪个"。这条命令按顺序做三件事——
# 校验端口 → 起网关（带看板与 UDP）→ 起模拟器循环，最后把要用的地址一次性打出来。

param(
    [switch]$Stop,
    [switch]$NoSim,
    [switch]$NoOpen,
    [switch]$Siren,      # 网关本机分级播报（演示建议打开）
    [switch]$Voice,      # 语音播报（SAPI，Windows 自带）
    [int]$Port = 8780,
    [int]$UdpPort = 9998
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot          # software 目录
$stateFile = Join-Path $root ".demo.json"
$dataDir = Join-Path $root "data"
if (-not (Test-Path $dataDir)) { New-Item -ItemType Directory -Path $dataDir | Out-Null }

function Stop-Demo {
    if (Test-Path $stateFile) {
        $st = Get-Content $stateFile -Raw | ConvertFrom-Json
        foreach ($id in @($st.gateway, $st.sim)) {
            if ($id) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
        }
        Remove-Item $stateFile -Force
        Write-Host "已停止演示进程（网关 / 模拟器）" -ForegroundColor Green
    } else {
        Write-Host "没有正在运行的演示进程（未找到 .demo.json）" -ForegroundColor Yellow
    }
}

if ($Stop) { Stop-Demo; exit 0 }

# 1. 端口占用检查：端口被占时报错，比起来后"连不上"好排查
$busy = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if ($busy) {
    # 先看占用者是不是"上一轮没关干净的网关"——是就自动收掉，让用户一条命令就能重跑。
    $ownPids = ($busy | Select-Object -ExpandProperty OwningProcess -Unique)
    $allOurs = $true
    foreach ($op in $ownPids) {
        $cmd = (Get-CimInstance Win32_Process -Filter "ProcessId=$op" -ErrorAction SilentlyContinue).CommandLine
        if (-not $cmd -or $cmd -notmatch 'run_gateway\.py') { $allOurs = $false }
    }
    if ($allOurs) {
        foreach ($op in $ownPids) { Stop-Process -Id $op -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Milliseconds 800
        Write-Host "上一轮网关没关干净（pid=$($ownPids -join ',')），已自动停掉，继续启动。" -ForegroundColor Yellow
    } else {
        Write-Host "端口 $Port 已被别的程序占用（pid=$($ownPids -join ',')）。" -ForegroundColor Red
        foreach ($op in $ownPids) {
            $cmd = (Get-CimInstance Win32_Process -Filter "ProcessId=$op" -ErrorAction SilentlyContinue).CommandLine
            Write-Host "  pid=$op  $cmd" -ForegroundColor DarkGray
        }
        Write-Host "处理办法：关掉该程序，或换端口：scripts\demo.ps1 -Port 8790" -ForegroundColor Red
        exit 1
    }
}

$python = if ($env:SHADOWLAMP_PY -and (Test-Path $env:SHADOWLAMP_PY)) { $env:SHADOWLAMP_PY } else { (Get-Command python -ErrorAction SilentlyContinue).Source }
if (-not $python) {
    Write-Host "找不到 python，请先把 Python 3.8 加进 PATH（可跑 scripts\check_env.ps1）" -ForegroundColor Red
    exit 1
}

# 2. 起网关（--lan 让手机/开发板能访问；--udp-port 兼容 Wi-Fi 直连模式）
$gOut = Join-Path $dataDir "demo_gateway.log"
$gErr = Join-Path $dataDir "demo_gateway.err.log"
$gwArgs = @("-u", "run_gateway.py", "--port", "$Port", "--lan", "--udp-port", "$UdpPort")
if ($Siren) { $gwArgs += "--siren" }
if ($Voice) { $gwArgs += "--voice" }

$gateway = Start-Process -FilePath $python `
    -ArgumentList $gwArgs `
    -WorkingDirectory $root -RedirectStandardOutput $gOut -RedirectStandardError $gErr `
    -PassThru -WindowStyle Hidden
Write-Host "网关启动中（pid=$($gateway.Id)）..." -ForegroundColor Cyan

$healthy = $false
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 400
    try {
        $resp = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$Port/health" -TimeoutSec 1
        if ($resp.StatusCode -eq 200) { $healthy = $true; break }
    } catch { }
}
if (-not $healthy) {
    Write-Host "网关没起来，看日志：$gOut / $gErr" -ForegroundColor Red
    if (Test-Path $gErr) { Get-Content $gErr -Tail 15 }
    Stop-Process -Id $gateway.Id -Force -ErrorAction SilentlyContinue
    exit 1
}

# 3. 起模拟器（循环模式，数值缓慢变化，不会瞬间刷屏）
$sim = $null
if (-not $NoSim) {
    $sOut = Join-Path $dataDir "demo_sim.log"
    $sErr = Join-Path $dataDir "demo_sim.err.log"
    $sim = Start-Process -FilePath $python `
        -ArgumentList @("-u", "sim\device_sim.py", "--url", "http://127.0.0.1:$Port", "--loop", "--interval", "2", "--duration", "7200") `
        -WorkingDirectory $root -RedirectStandardOutput $sOut -RedirectStandardError $sErr `
        -PassThru -WindowStyle Hidden
}

@{ gateway = $gateway.Id; sim = $(if ($sim) { $sim.Id } else { $null }) } |
    ConvertTo-Json | Set-Content -Path $stateFile -Encoding UTF8

# 4. 把要用的地址一次性打出来
$lanIp = "127.0.0.1"
try {
    $lanIp = (Get-NetIPAddress -AddressFamily IPv4 |
              Where-Object { $_.IPAddress -notlike "127.*" -and $_.PrefixOrigin -ne "WellKnown" } |
              Select-Object -First 1).IPAddress
} catch { }

Write-Host ""
if ($Siren) {
    Write-Host "分级播报：已就绪（L3 循环警笛 / L2 三声 / L1 两声）" -ForegroundColor Yellow
    Write-Host "  注意：启动时不会自动响——只响"启动之后"新产生的告警。" -ForegroundColor DarkGray
    Write-Host "  要立刻播报现有待确认：python scripts\siren.py on ｜ 静音：python scripts\siren.py off" -ForegroundColor DarkGray
} else {
    Write-Host "分级播报：未开启（要听声音请加 -Siren -Voice）" -ForegroundColor DarkGray
}
Write-Host "演示环境已就绪" -ForegroundColor Green
Write-Host ("  本机看板      http://127.0.0.1:{0}/" -f $Port)
Write-Host ("  手机看板      http://{0}:{1}/" -f $lanIp, $Port) -ForegroundColor Yellow
Write-Host ("  APP 网关地址  http://{0}:{1}" -f $lanIp, $Port) -ForegroundColor Yellow
Write-Host ("  UDP 接收端口  {0}（Wi-Fi 直连模式）" -f $UdpPort)
Write-Host ""
Write-Host "接下来可以（另开一个窗口）：" -ForegroundColor Cyan
Write-Host "  python sim\incident.py --type temp     # 温度过高 -> 提醒"
Write-Host "  python sim\incident.py --type night    # 夜间无活动 -> 紧急"
Write-Host "  python sim\incident.py --type sos      # 按求助键 -> 紧急"
Write-Host "  python sim\incident.py --type reset    # 清空待处理 + 恢复心跳"
Write-Host "  python scripts\siren.py on             # 立刻播报（含历史待确认）"
Write-Host "  python scripts\siren.py off            # 立刻静音"
Write-Host ""
Write-Host "停止：powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -Stop" -ForegroundColor Cyan

if (-not $NoOpen) {
    Start-Process ("http://127.0.0.1:{0}/" -f $Port)
}
