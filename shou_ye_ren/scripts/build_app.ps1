# 命令行构建 APP（不打开 DevEco Studio 也能出 HAP）
#
# 用法：powershell -ExecutionPolicy Bypass -File .\build_app.ps1
# 产物：<工程>\entry\build\default\outputs\default\entry-default-unsigned.hap
#
# 说明：
#   1. DEVECO_SDK_HOME 必须指向 DevEco Studio 的集成 SDK 根目录（里面是 default/sdk-pkg.json），
#      不是外挂的 "DevEco Studio sdk"（那个目录结构不同，会报 00303312）。
#   2. 打包阶段需要 Java，用 Studio 自带的 jbr，不用另外装 JDK。
#   3. 产出是 unsigned HAP；装到模拟器/真机需要在 Studio 里登录华为账号后自动签名。

param(
    [string]$Project = $(if ($env:SHADOWLAMP_PROJECT) { $env:SHADOWLAMP_PROJECT } else { "D:\DevEcoProjects\shadow-lamp-app" }),
    [string]$Studio  = $(if ($env:SHADOWLAMP_STUDIO) { $env:SHADOWLAMP_STUDIO } else { "D:\HarmonySetup\DevEco Studio" })
)

$ErrorActionPreference = 'Stop'

# 构建前自动从工作区同步源码到 DevEco 工程目录。
# 不同步的话，构建用的是旧副本，改完代码也会"看起来没生效"（已踩过坑）。
$sync = Join-Path $PSScriptRoot "sync_app.ps1"
if (Test-Path $sync) {
    & $sync | Out-Null
}

if (-not (Test-Path $Project)) { throw "工程不存在：$Project" }
if (-not (Test-Path $Studio))  { throw "DevEco Studio 路径不对：$Studio" }

$env:DEVECO_SDK_HOME = Join-Path $Studio "sdk"
$env:PATH = (Join-Path $Studio "jbr\bin") + ";" + $env:PATH

$node   = Join-Path $Studio "tools\node\node.exe"
$hvigor = Join-Path $Studio "tools\hvigor\bin\hvigorw.js"

Write-Host "SDK   : $env:DEVECO_SDK_HOME"
Write-Host "工程  : $Project"
Write-Host "构建中…" -ForegroundColor Cyan

Push-Location $Project
try {
    & $node $hvigor --mode module -p product=default assembleHap --no-daemon
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($code -ne 0) {
    Write-Host "构建失败（exit=$code），看 .hvigor\outputs\build-logs" -ForegroundColor Red
    exit $code
}

$hap = Get-ChildItem (Join-Path $Project "entry\build\default\outputs\default") -Filter *.hap |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
Write-Host ""
Write-Host "构建成功" -ForegroundColor Green
if ($hap) { Write-Host ("产物: {0}  ({1:N0} KB)" -f $hap.FullName, ($hap.Length / 1KB)) }
