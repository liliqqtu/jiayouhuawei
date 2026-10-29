# emulator_ram.ps1 -- lower a HarmonyOS emulator AVD's RAM so it can start on a
# memory-tight machine (ErrorCode 00801005 "可申请内存不足 4.00GB").
#
# ASCII-only output on purpose (Windows PowerShell 5.1 reads non-BOM UTF-8 as ANSI).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\emulator_ram.ps1                 # show current RAM + free memory
#   powershell -ExecutionPolicy Bypass -File scripts\emulator_ram.ps1 -Ram 3072 -Apply # patch the AVD
#   powershell -ExecutionPolicy Bypass -File scripts\emulator_ram.ps1 -Ram 4096 -Apply # revert
#
# What it touches (all reversible; originals backed up as *.bak-<timestamp>):
#   <AVD dir>\config.ini          -> hw.ramSize
#   <AVD dir>\hardware-qemu.ini   -> hw.ramSize
#   ...\Emulator\deployed\lists.json -> memoryRamSize for that AVD entry

param(
    [string]$AvdName = "Mate 90 Pro",
    [int]$Ram = 0,
    [switch]$Apply
)

$ErrorActionPreference = "Stop"
$avdRoot = "C:\Users\ZhuanZ1\AppData\Local\Huawei\Emulator\deployed"
$lists   = Join-Path $avdRoot "lists.json"
$avdDir  = Join-Path $avdRoot $AvdName
$stamp   = Get-Date -Format "yyyyMMdd-HHmmss"

function Show-Mem {
    $os = Get-CimInstance Win32_OperatingSystem
    $cs = Get-CimInstance Win32_ComputerSystem
    Write-Host ("RAM total {0:N1} GB | free {1:N1} GB | commit free {2:N1} GB" -f `
        ($cs.TotalPhysicalMemory / 1GB), ($os.FreePhysicalMemory / 1MB), ($os.FreeVirtualMemory / 1MB))
}

if (-not (Test-Path $avdDir)) {
    Write-Host "AVD not found: $avdDir"
    Write-Host "available:"
    Get-ChildItem $avdRoot -Directory | Select-Object -ExpandProperty Name
    exit 1
}

Show-Mem
$cfg = Join-Path $avdDir "config.ini"
$cur = (Select-String -Path $cfg -Pattern '^hw\.ramSize=(.+)$').Matches.Groups[1].Value
Write-Host ("AVD '{0}' hw.ramSize = {1}" -f $AvdName, $cur)

if (-not $Apply) {
    Write-Host ""
    Write-Host "dry run. to patch:  -Ram 3072 -Apply   (or 4096 to revert)"
    exit 0
}
if ($Ram -lt 2048 -or $Ram -gt 8192) {
    Write-Host "Ram must be between 2048 and 8192"
    exit 1
}

# 1) config.ini + hardware-qemu.ini
foreach ($f in @($cfg, (Join-Path $avdDir "hardware-qemu.ini"))) {
    if (-not (Test-Path $f)) { Write-Host "skip (missing): $f"; continue }
    Copy-Item $f "$f.bak-$stamp" -Force
    $text = Get-Content $f -Raw
    $new = [regex]::Replace($text, 'hw\.ramSize=\d+', "hw.ramSize=$Ram")
    if ($new -ne $text) {
        [System.IO.File]::WriteAllText($f, $new, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host ("patched {0} -> hw.ramSize={1}" -f (Split-Path -Leaf $f), $Ram)
    } else {
        Write-Host ("no hw.ramSize in {0}" -f (Split-Path -Leaf $f))
    }
}

# 2) lists.json (the launcher reads memoryRamSize from here)
Copy-Item $lists "$lists.bak-$stamp" -Force
$json = Get-Content $lists -Raw
$pattern = '("name"\s*:\s*"' + [regex]::Escape($AvdName) + '"(?:(?!\}).)*?"memoryRamSize"\s*:\s*")\d+(")'
$updated = [regex]::Replace($json, $pattern, { param($m) $m.Groups[1].Value + $Ram + $m.Groups[2].Value }, 'Singleline')
if ($updated -ne $json) {
    [System.IO.File]::WriteAllText($lists, $updated, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ("patched lists.json entry '{0}' -> memoryRamSize={1}" -f $AvdName, $Ram)
} else {
    Write-Host "lists.json: entry not matched (check AVD name)"
}

Write-Host ""
Show-Mem
Write-Host "done. now RESTART DevEco Studio (it caches lists.json), then Start the emulator."
Write-Host ("backups: *.bak-{0}" -f $stamp)
