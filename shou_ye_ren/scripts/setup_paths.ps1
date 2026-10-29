# setup_paths.ps1 —— 队友机器上一次性配置本机路径
#
# 为什么需要：软件包里的脚本默认按开发机的路径写（D:\HarmonySetup\DevEco Studio 等），
# 换一台电脑就找不到。跑一次这个脚本，把本机真实路径写进“用户环境变量”，
# 之后 demo.ps1 / build_app.ps1 / sync_app.ps1 / sim_preflight.ps1 都自动用上。
#
# 用法（在解压后的 software 目录下）：
#   powershell -ExecutionPolicy Bypass -File scripts\setup_paths.ps1              # 自动探测并写入
#   powershell -ExecutionPolicy Bypass -File scripts\setup_paths.ps1 -Show        # 只看探测结果，不写
#   powershell -ExecutionPolicy Bypass -File scripts\setup_paths.ps1 -Studio "C:\Program Files\Huawei\DevEco Studio"
#
# 写入的是“用户变量”（不需要管理员）；重开 PowerShell 窗口生效。

param(
    [string]$Studio = "",
    [string]$Project = "",
    [string]$Python = "",
    [switch]$Show
)

$ErrorActionPreference = 'Stop'

function Write-Item($name, $value, $ok) {
    $flag = if ($ok) { '[OK]  ' } else { '[缺失]' }
    $color = if ($ok) { 'Green' } else { 'Yellow' }
    Write-Host ("{0} {1} = {2}" -f $flag, $name, $value) -ForegroundColor $color
}

# ---------- 1. 找 DevEco Studio（需要它自带的 hvigor / node / sdk） ----------
function Find-Studio {
    $candidates = @()
    if ($env:DEVECO_SDK_HOME) {
        # DEVECO_SDK_HOME 一般指向 <Studio>\sdk
        $candidates += (Split-Path $env:DEVECO_SDK_HOME -Parent)
    }
    $candidates += @(
        'C:\Program Files\Huawei\DevEco Studio',
        'D:\HarmonySetup\DevEco Studio',
        "$env:LOCALAPPDATA\Huawei\DevEco Studio",
        'C:\Program Files\Huawei\DevEcoStudio',
        'D:\DevEco Studio'
    )
    # 再扫一遍常见盘根的 *\DevEco Studio
    foreach ($drive in @('C:', 'D:', 'E:')) {
        if (Test-Path $drive) {
            Get-ChildItem $drive -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like '*DevEco*' -or $_.Name -like '*Harmony*' } |
                ForEach-Object {
                    Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue |
                        Where-Object { $_.Name -like '*DevEco Studio*' } |
                        ForEach-Object { $candidates += $_.FullName }
                }
        }
    }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c 'tools\hvigor\bin\hvigorw.js'))) { return $c }
    }
    return $null
}

# ---------- 2. 找 Python（3.8+ 即可，网关只用标准库） ----------
function Find-Python {
    foreach ($name in @('python', 'python3')) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    if (Get-Command py -ErrorAction SilentlyContinue) {
        try {
            $p = & py -3 -c "import sys;print(sys.executable)" 2>$null
            if ($p) { return $p.Trim() }
        } catch { }
    }
    return $null
}

# ---------- 3. DevEco 工程目录（APP 构建副本） ----------
function Find-Project {
    param([string]$StudioPath)
    $default = Join-Path $env:USERPROFILE 'DevEcoProjects\shadow-lamp-app'
    if (Test-Path $default) { return $default }
    # 常见做法是把工程放在非系统盘（避开中文/OneDrive 路径），逐盘找一遍
    foreach ($drive in @('D:', 'E:', 'C:')) {
        $candidate = Join-Path $drive 'DevEcoProjects\shadow-lamp-app'
        if (Test-Path $candidate) { return $candidate }
    }
    # 再扫一层 <盘>:\*\shadow-lamp-app
    foreach ($drive in @('C:', 'D:', 'E:')) {
        if (-not (Test-Path $drive)) { continue }
        $hit = Get-ChildItem $drive -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path (Join-Path $_.FullName 'shadow-lamp-app') } |
            Select-Object -First 1
        if ($hit) { return (Join-Path $hit.FullName 'shadow-lamp-app') }
    }
    return $default   # 还没建也先返回默认值，sync_app.ps1 会提示先建
}

if (-not $Studio) { $Studio = Find-Studio }
if (-not $Python) { $Python = Find-Python }
if (-not $Project) { $Project = Find-Project -StudioPath $Studio }

Write-Host '== 本机路径探测 ==' -ForegroundColor Cyan
Write-Item 'DevEco Studio' $Studio ([bool]$Studio)
Write-Item 'Python' $Python ([bool]$Python)
Write-Item 'APP 工程目录' $Project ([bool](Test-Path $Project))

if ($Studio) {
    Write-Item '  sdk 目录' (Join-Path $Studio 'sdk') (Test-Path (Join-Path $Studio 'sdk'))
    Write-Item '  hdc' (Join-Path $Studio 'sdk\default\openharmony\toolchains\hdc.exe') `
        (Test-Path (Join-Path $Studio 'sdk\default\openharmony\toolchains\hdc.exe'))
}

if ($Show) {
    Write-Host '（-Show 模式：未写入环境变量）' -ForegroundColor DarkGray
    exit 0
}

Write-Host '== 写入用户环境变量 ==' -ForegroundColor Cyan
if ($Studio) { [Environment]::SetEnvironmentVariable('SHADOWLAMP_STUDIO', $Studio, 'User') }
if ($Python) { [Environment]::SetEnvironmentVariable('SHADOWLAMP_PY', $Python, 'User') }
if ($Project) { [Environment]::SetEnvironmentVariable('SHADOWLAMP_PROJECT', $Project, 'User') }

# 当前会话也生效，方便接着往下跑
if ($Studio) { $env:SHADOWLAMP_STUDIO = $Studio }
if ($Python) { $env:SHADOWLAMP_PY = $Python }
if ($Project) { $env:SHADOWLAMP_PROJECT = $Project }

Write-Host ''
Write-Host '完成。新开一个 PowerShell 窗口后再跑 scripts\demo.ps1 或 scripts\check_env.ps1。' -ForegroundColor Green
if (-not $Studio) {
    Write-Host '没找到 DevEco Studio：如果你只做硬件联调（不构建 APP），可以直接跳过，' -ForegroundColor Yellow
    Write-Host '用软件包 dist 目录里预置的 HAP 装到模拟器即可。' -ForegroundColor Yellow
}
