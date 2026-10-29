# 华为手机 / 虚拟手机一键安装脚本
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$scriptDir\install_to_phone.py"
if ($LASTEXITCODE -ne 0) {
    Pause
}
