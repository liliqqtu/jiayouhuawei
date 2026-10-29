# sim_preflight.ps1 -- one-shot environment check before demoing on the emulator.
# ASCII-only on purpose: Windows PowerShell 5.1 reads non-BOM UTF-8 as ANSI and
# would garble Chinese output (or even fail to parse).
param(
    [string]$Hdc = $(if ($env:SHADOWLAMP_STUDIO) { Join-Path $env:SHADOWLAMP_STUDIO "sdk\default\openharmony\toolchains\hdc.exe" } else { "D:\HarmonySetup\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe" }),
    [string]$Hap = "",
    [int]$Port = 8780
)

# HAP 位置：优先用工程构建产物，其次用软件包 dist 里预置的（队友机器没装 DevEco 也能装 APP）
if (-not $Hap) {
    $proj = if ($env:SHADOWLAMP_PROJECT) { $env:SHADOWLAMP_PROJECT } else { Join-Path $env:USERPROFILE 'DevEcoProjects\shadow-lamp-app' }
    $built = Join-Path $proj 'entryuild\default\outputs\default\entry-default-unsigned.hap'
    $dist = Join-Path (Split-Path $PSScriptRoot -Parent) 'dist\entry-default-unsigned.hap'
    if (Test-Path $built) { $Hap = $built }
    elseif (Test-Path $dist) { $Hap = $dist }
    else { $Hap = $built }
}

$ok = 0
$fail = 0

function Say([string]$tag, [string]$msg) {
    Write-Host ("  [{0}] {1}" -f $tag, $msg)
}

Write-Host "== shadowlamp preflight =="

# 1. hdc + emulator/device
if (Test-Path $Hdc) {
    Say "ok" "hdc found: $Hdc"
    $targets = & $Hdc list targets 2>&1
    $text = ($targets | Out-String).Trim()
    if ($text -match "Empty" -or $text -eq "") {
        Say "fail" "no device. Start the emulator in DevEco Studio -> Device Manager -> Mate 90 Pro -> Start"
        $fail++
    } elseif ($text -match "5555") {
        Say "ok" "target: $text"
        $ok++
    } else {
        Say "ok" "target: $text"
        $ok++
    }
} else {
    Say "fail" "hdc not found at $Hdc"
    $fail++
}

# 2. build artifact
if (Test-Path $Hap) {
    $mb = [math]::Round((Get-Item $Hap).Length / 1MB, 2)
    Say "ok" "hap: $Hap ($mb MB)"
    $ok++
} else {
    Say "fail" "hap missing. Run: powershell -ExecutionPolicy Bypass -File scripts\build_app.ps1"
    $fail++
}

# 3. port state
$conn = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if ($conn) {
    Say "warn" ("port {0} already in use (pid {1}) -- probably a running gateway; that is fine" -f $Port, ($conn | Select-Object -First 1).OwningProcess)
} else {
    Say "ok" ("port {0} free -- demo.ps1 will start the gateway" -f $Port)
}
$ok++

# 4. python
$py = (Get-Command python -ErrorAction SilentlyContinue)
if ($py) {
    $ver = & python -c "import sys; print(sys.version.split()[0])" 2>&1
    Say "ok" "python $ver ($($py.Source))"
    $ok++
} else {
    Say "fail" "python not on PATH"
    $fail++
}

# 5. app installed on the emulator (best effort)
if (Test-Path $Hdc) {
    $pkg = & $Hdc shell "bm dump -n com.shadowlamp.care" 2>&1 | Out-String
    if ($pkg -match "com.shadowlamp.care") {
        Say "ok" "app installed on device"
    } else {
        Say "warn" "app not installed yet: cd <hap dir>; hdc install -r entry-default-unsigned.hap"
    }
}

Write-Host ""
Write-Host ("summary: ok={0} fail={1}" -f $ok, $fail)
if ($fail -gt 0) {
    Write-Host "fix the [fail] lines first, then re-run."
    exit 1
}
Write-Host "ready. next: powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -Siren -Voice"
exit 0
