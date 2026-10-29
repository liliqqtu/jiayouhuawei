# 一键回到“干净演示状态”：预演很多遍时用它，省得每次手动收拾
#
# 它做四件事：停掉上一轮的网关/模拟器 → 清掉测试设备与残留待确认告警
#              → 重新拉起网关（带声光播报）+ 模拟器 → 打印接下来该做什么
#
# 用法（在 software 目录下）：
#   powershell -ExecutionPolicy Bypass -File scripts\rehearse.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\rehearse.ps1 -Siren -Voice   # 带声音（正式预演用）
#   powershell -ExecutionPolicy Bypass -File scripts\rehearse.ps1 -KeepData       # 保留上一轮数据不清设备
#
# 和 demo.ps1 的区别：demo.ps1 只管“起服务”，rehearse.ps1 额外保证“一开始是干净的”，
# 并按演示顺序把下一步该敲的命令打出来，照着念就行。

param(
    [switch]$Siren,
    [switch]$Voice,
    [switch]$KeepData,
    [int]$Port = 8780,
    [int]$UdpPort = 9998
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host ""
Write-Host "① 停掉上一轮的网关 / 模拟器" -ForegroundColor Cyan
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "demo.ps1") -Stop | Out-Null
# demo.ps1 只认它记录的 pid，这里再按命令行兜底杀一次，避免手动起过、状态文件里没记录
Get-CimInstance Win32_Process -Filter "Name like 'python%'" |
    Where-Object { $_.CommandLine -match 'run_gateway\.py|device_sim\.py' } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 600

if (-not $KeepData) {
    Write-Host "② 清掉历史测试设备（bench-01 / bench-02）" -ForegroundColor Cyan
    & python (Join-Path $PSScriptRoot "clean_test_devices.py") 2>&1 | Select-Object -Last 3
} else {
    Write-Host "② 跳过清库（-KeepData）" -ForegroundColor Cyan
}

Write-Host "③ 拉起网关与模拟器" -ForegroundColor Cyan
$demoArgs = @{ Port = $Port; UdpPort = $UdpPort }
if ($Siren) { $demoArgs.Siren = $true }
if ($Voice) { $demoArgs.Voice = $true }
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "demo.ps1") @demoArgs
Start-Sleep -Seconds 3

Write-Host "④ 清掉开机瞬间可能重新入队的待确认告警" -ForegroundColor Cyan
& python (Join-Path $PSScriptRoot "..\sim\incident.py") --type reset --url ("http://127.0.0.1:" + $Port) 2>&1 | Select-Object -Last 2

Write-Host ""
Write-Host "================ 现在开始按顺序演示 ================" -ForegroundColor Green
Write-Host " 浏览器： http://127.0.0.1:$Port          （网关看板，可以一直开着）" -ForegroundColor Gray
Write-Host " APP   ： 我的 → 网关地址 http://127.0.0.1:$Port → 保存并测试" -ForegroundColor Gray
Write-Host ""
$steps = @(
    @{ n = "1"; t = "温度偏高 L1"; c = "python sim\incident.py --type temp" },
    @{ n = "2"; t = "湿度偏高 L2"; c = "python sim\incident.py --type hum" },
    @{ n = "3"; t = "夜间光照 L1"; c = "python sim\incident.py --type lux" },
    @{ n = "4"; t = "门磁未关 L1"; c = "python sim\incident.py --type door" },
    @{ n = "5"; t = "夜间无活动 L3"; c = "python sim\incident.py --type night" },
    @{ n = "6"; t = "随身求助 L3"; c = "python sim\incident.py --type sos" },
    @{ n = "7"; t = "疑似火情 L3"; c = "python sim\incident.py --type fire" }
)
foreach ($s in $steps) {
    Write-Host ("  {0}. {1}  ->  {2}" -f $s.n, $s.t, $s.c) -ForegroundColor White
}
Write-Host ""
Write-Host "  每步做完：APP「告警」页点「确认告警」→ 声音立刻停；再点「提交处置并关闭」。" -ForegroundColor Gray
Write-Host "  想整段回到干净状态：重新跑一次本脚本。" -ForegroundColor Gray
Write-Host "===================================================" -ForegroundColor Green
