<#
  incident.ps1 —— 场景注入（不用先 cd，在任何目录下都能跑）

  为什么要有这个脚本：演示时最常见的一步错是“忘了先 cd 到项目目录”，
  于是在 C:\Users\... 下执行 `python sim\incident.py --type temp`，
  Python 直接报 `can't open file 'sim\incident.py': [Errno 2] No such file or directory`。
  这里把路径和解释器都固定好，参数只剩场景名。

  用法：
      powershell -ExecutionPolicy Bypass -File scripts\incident.ps1 -Type temp
      powershell -ExecutionPolicy Bypass -File scripts\incident.ps1 -Type sos
      powershell -ExecutionPolicy Bypass -File scripts\incident.ps1 -Type reset

  等价于：
      cd D:\shadowlamp
      python sim\incident.py --type temp

  场景：temp 高温 / fire 火情 / hum 高湿 / lux 夜间强光 / night 夜间无活动 /
        door 门磁未关 / sos 随身求助 / reset 复位
#>
param(
    [Parameter(Position = 0)]
    [ValidateSet("temp", "fire", "hum", "lux", "night", "door", "sos", "reset")]
    [string]$Type = "sos",

    [string]$Url = "http://127.0.0.1:8780"
)

$ErrorActionPreference = "Stop"

# scripts\ 的上一级就是项目根目录（software）
$root = Split-Path -Parent $PSScriptRoot

# 优先用装了 3.8 的解释器；找不到再退回 PATH 里的 python
$py = if ($env:SHADOWLAMP_PY) { $env:SHADOWLAMP_PY } else { "D:\HarmonySetup\Python38\python.exe" }
if (-not (Test-Path $py)) { $py = "python" }

$incident = Join-Path $root "sim\incident.py"
if (-not (Test-Path $incident)) {
    Write-Host "找不到注入脚本：$incident" -ForegroundColor Red
    exit 1
}

$env:PYTHONIOENCODING = "utf-8"
& $py $incident --type $Type --url $Url
exit $LASTEXITCODE
