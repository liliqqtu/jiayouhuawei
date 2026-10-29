# 环境自检脚本（Windows PowerShell）
# 用法：右键“使用 PowerShell 运行”，或在 PowerShell 里执行
#       powershell -ExecutionPolicy Bypass -File .\check_env.ps1
#
# 目的：把“七步安装”逐条核实成可读结论，避免凭印象判断环境。

$ErrorActionPreference = 'SilentlyContinue'

function Line($name, $value, $ok) {
    $mark = if ($ok -eq $true) { '[OK]  ' } elseif ($ok -eq $false) { '[MISS]' } else { '[??]  ' }
    Write-Host ("{0} {1,-18} {2}" -f $mark, $name, $value)
}

Write-Host "==== 赛题四 软件开发环境自检 ====" -ForegroundColor Cyan

# 1 Git
$git = (git --version) 2>$null
Line 'Git' $git ([bool]$git)

# 2 Python 3.8
$pyExe = (Get-Command python -ErrorAction SilentlyContinue).Source
$pyVer = (python --version 2>&1) -replace 'Python ', ''
$pyOk = $pyVer -like '3.8.*'
Line 'Python' "$pyVer  ($pyExe)" $pyOk

# 3 Node（本项目非必需，仅作记录）
$node = (Get-Command node -ErrorAction SilentlyContinue).Source
Line 'Node(可选)' ($(if ($node) { (node -v) + "  ($node)" } else { '未安装（本项目两条编译链都不依赖系统 Node）' })) $null

# 4 VS Code
$code = (Get-Command code -ErrorAction SilentlyContinue).Source
Line 'VS Code' ($(if ($code) { $code } else { '未找到' })) ([bool]$code)

# 5 DevEco Device Tool（VS Code 插件）
$ext = Get-ChildItem "$env:USERPROFILE\.vscode\extensions" -Directory |
    Where-Object { $_.Name -like 'huawei.deveco-device-tool-*' } |
    Select-Object -First 1
Line 'Device Tool' ($(if ($ext) { $ext.Name } else { '未安装' })) ([bool]$ext)

# 6 DevEco Studio + SDK
$studio = @(
    "D:\HarmonySetup\DevEco Studio",
    "C:\Program Files\Huawei\DevEco Studio"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
Line 'DevEco Studio' ($(if ($studio) { $studio } else { '未找到' })) ([bool]$studio)

$apiLevel = $null
if ($studio) {
    foreach ($sdk in (Get-ChildItem "D:\HarmonySetup" -Directory -Filter '*sdk*')) {
        $pkg = Get-ChildItem $sdk.FullName -Recurse -Depth 3 -Filter 'oh-uni-package.json' | Select-Object -First 1
        if ($pkg) {
            $apiLevel = (Get-Content $pkg.FullName -Raw | ConvertFrom-Json).apiVersion
            break
        }
    }
}
Line 'SDK API' ($(if ($apiLevel) { "API $apiLevel（要求 ≥12）" } else { '未读到' })) ($apiLevel -and [int]$apiLevel -ge 12)

# 7 路径合规性（中文/空格/OneDrive 是南向与北向编译的常见杀手）
$cwd = (Get-Location).Path
$bad = $cwd -match '[^\x00-\x7F]' -or $cwd -match '\s' -or $cwd -match 'OneDrive'
Line '当前路径' $cwd (-not $bad)
if ($bad) {
    Write-Host '        ↑ 路径含中文/空格/OneDrive，工程请放到 D:\DevEcoProjects' -ForegroundColor Yellow
}

$proj = 'D:\DevEcoProjects'
Line '工程目录' ($(if (Test-Path $proj) { $proj + ' (已就绪)' } else { '不存在，请创建' })) (Test-Path $proj)

Write-Host ""
Write-Host "结论口径：" -ForegroundColor Cyan
Write-Host "  1. 南向（BearPi-HM Nano）必须用 Python 3.8 + Device Tool；Studio 版本与此无关。"
Write-Host "  2. 北向（APP）只认 DevEco Studio 的 SDK API 版本，要求 ≥12。"
Write-Host "  3. 工程路径必须全英文、不含空格、不在 OneDrive 同步目录内。"
