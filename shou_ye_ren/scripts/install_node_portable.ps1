# 便携版 Node.js LTS 安装脚本（免管理员、可回滚）
#
# 说明：本项目的两条编译链都不依赖“系统 Node”——
#   · 南向 Device Tool 4.0.400 自带 deveco-node-helpers，跑在 VS Code 的 Electron 内；
#   · 北向 DevEco Studio 自带 node（tools\node），hvigor/ohpm 用它。
# 因此本脚本是“按官方七步补齐”的可选项，不是编译必需。
#
# 用法：powershell -ExecutionPolicy Bypass -File .\install_node_portable.ps1
# 回滚：删除 D:\HarmonySetup\node，并从“用户变量 PATH”里去掉该路径。

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$root = 'D:\HarmonySetup\node'
$tmp  = Join-Path $env:TEMP ('node-lts-' + [guid]::NewGuid().ToString('N').Substring(0, 8) + '.zip')

Write-Host '== 1/4 查询最新 LTS 版本 ==' -ForegroundColor Cyan
try {
    $index = Invoke-RestMethod -Uri 'https://nodejs.org/dist/index.json' -TimeoutSec 30
    $lts = $index | Where-Object { $_.lts -ne $false } | Select-Object -First 1
    $ver = $lts.version
    Write-Host "   选定 $ver ($($lts.lts))"
} catch {
    Write-Host "   查询失败：$($_.Exception.Message)" -ForegroundColor Red
    Write-Host '   可改用：winget install OpenJS.NodeJS.LTS' -ForegroundColor Yellow
    exit 1
}

$zipName = "node-$ver-win-x64.zip"
$url = "https://nodejs.org/dist/$ver/$zipName"

Write-Host "== 2/4 下载 $zipName ==" -ForegroundColor Cyan
Invoke-WebRequest -Uri $url -OutFile $tmp -TimeoutSec 600
Write-Host ("   完成，{0:N1} MB" -f ((Get-Item $tmp).Length / 1MB))

Write-Host '== 3/4 解压到 D:\HarmonySetup\node ==' -ForegroundColor Cyan
if (Test-Path $root) { Remove-Item $root -Recurse -Force }
New-Item -ItemType Directory -Path $root | Out-Null
Expand-Archive -Path $tmp -DestinationPath $root -Force
$inner = Get-ChildItem $root -Directory | Select-Object -First 1
if ($inner) {
    Get-ChildItem $inner.FullName | Move-Item -Destination $root -Force
    Remove-Item $inner.FullName -Recurse -Force
}
Remove-Item $tmp -Force

Write-Host '== 4/4 写入用户 PATH（不需要管理员） ==' -ForegroundColor Cyan
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if ($userPath -notlike "*$root*") {
    [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ';' + $root), 'User')
    Write-Host "   已追加 $root"
} else {
    Write-Host '   已存在，跳过'
}

Write-Host ''
& "$root\node.exe" -v
& "$root\npm.cmd" -v
Write-Host ''
Write-Host '完成。新开一个终端再执行 node -v、npm -v 验证（当前窗口的 PATH 是旧的）。' -ForegroundColor Green
