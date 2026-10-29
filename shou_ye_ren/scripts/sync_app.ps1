# 把工作区里的 APP 源码同步到 DevEco 工程，再可选地构建
#
# 为什么需要这个脚本：
#   DevEco Studio 打开的是 D:\DevEcoProjects\shadow-lamp-app（必须避开中文/OneDrive 路径），
#   而源码存在工作区 software\app\ShadowLamp（在 OneDrive 里）。两边是镜像关系，
#   改了工作区不会自动进工程——曾经因为漏同步，构建出来的 HAP 还是旧端口。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File .\sync_app.ps1            # 只同步
#   powershell -ExecutionPolicy Bypass -File .\sync_app.ps1 -Build     # 同步并构建
#
# 只同步我们自己的代码（ets / resources / AppScope / module.json5），
# 不动工程里的 build-profile signingConfigs、build、oh_modules、.hvigor，
# 否则会在 Studio 里重新弹“签名未配置”。

param(
    [string]$Source  = "",
    [string]$Project = $(if ($env:SHADOWLAMP_PROJECT) { $env:SHADOWLAMP_PROJECT } else { "D:\DevEcoProjects\shadow-lamp-app" }),
    [switch]$Build
)

$ErrorActionPreference = 'Stop'

if (-not $Source) {
    # 脚本在 software\scripts 下，源码在 software\app\ShadowLamp
    $Source = Join-Path (Split-Path $PSScriptRoot -Parent) "app\ShadowLamp"
}

if (-not (Test-Path $Source))  { throw "源码目录不存在：$Source" }
if (-not (Test-Path $Project)) { throw "工程目录不存在：$Project（先用 DevEco Studio 打开一次生成）" }

$pairs = @(
    @{ From = "entry\src\main\ets";           To = "entry\src\main\ets" },
    @{ From = "entry\src\main\resources";     To = "entry\src\main\resources" },
    @{ From = "AppScope";                     To = "AppScope" }
)

foreach ($p in $pairs) {
    $from = Join-Path $Source $p.From
    $to   = Join-Path $Project $p.To
    if (-not (Test-Path $from)) { continue }
    Write-Host ("同步 {0} -> {1}" -f $p.From, $Project) -ForegroundColor Cyan
    robocopy $from $to /E /NFL /NDL /NJH /NJS /R:1 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { throw ("robocopy 失败，exit={0}" -f $LASTEXITCODE) }
}

# 模块清单单独拷（不在上面的目录里）
$module = Join-Path $Source "entry\src\main\module.json5"
if (Test-Path $module) {
    Copy-Item $module (Join-Path $Project "entry\src\main\module.json5") -Force
}

Write-Host "同步完成。" -ForegroundColor Green

if ($Build) {
    Write-Host "开始构建…" -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "build_app.ps1") -Project $Project
    exit $LASTEXITCODE
}
