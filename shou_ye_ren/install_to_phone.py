# -*- coding: utf-8 -*-
"""
华为手机 / OpenHarmony 虚拟手机 一键安装 HAP 脚本
自动识别真实手机/模拟器，智能过滤小熊派 COM 串口干扰
"""

import os
import sys
import subprocess
import time

HDC_PATH = r"D:\DevEco_Studio\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe"
HAP_PATH = r"D:\Dream\shou_ye_ren\dist\entry-default-unsigned.hap"

def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="ignore")
        return res.stdout.strip()
    except Exception as e:
        return f"Error: {e}"

def main():
    print("=" * 65)
    print("       华为手机 / 鸿蒙虚拟手机 · HAP 一键安装助手")
    print("=" * 65)

    if not os.path.exists(HDC_PATH):
        print(f"[错误] 未找到 hdc 工具: {HDC_PATH}")
        return

    if not os.path.exists(HAP_PATH):
        print(f"[错误] 未找到 HAP 安装包: {HAP_PATH}")
        return

    print("正在扫描当前已连接的 OpenHarmony 设备 / 模拟器...")
    
    # 尝试连接常见模拟器端口
    run_cmd([HDC_PATH, "tconn", "127.0.0.1:5555"])

    raw_output = run_cmd([HDC_PATH, "list", "targets", "-v"])
    lines = [line.strip() for line in raw_output.splitlines() if line.strip()]

    valid_devices = []
    uart_devices = []

    for line in lines:
        parts = line.split()
        if not parts or parts[0] == "[Empty]":
            continue
        dev_id = parts[0]
        # 判断类型
        if "UART" in line or dev_id.upper().startswith("COM"):
            uart_devices.append(dev_id)
        else:
            status = parts[2] if len(parts) >= 3 else "Unknown"
            valid_devices.append((dev_id, status, line))

    if uart_devices:
        print(f"  [发现串口设备] {', '.join(uart_devices)} (小熊派开发板，已自动忽略)")

    if not valid_devices:
        print("\n" + "=" * 65)
        print("【提示】暂未检测到运行中的虚拟手机或已连接的真机！")
        print("=" * 65)
        print("请检查：")
        print("  1. DevEco Studio 中的虚拟手机是否已经启动？")
        print("     - 如果使用的是本地模拟器（Local Emulator）：")
        print("       请在 DevEco Studio 顶部菜单点击【Tools】->【Device Manager】并启动它。")
        print("     - 如果使用的是远程真机（Remote Device）：")
        print("       由于远程设备在云端，最推荐的安装方式是直接在 DevEco Studio 内一键运行。")
        print()
        print("  ★ 最省心推荐方式（1秒重新运行）：")
        print("    直接在 DevEco Studio 顶部工具栏，点击绿色的 ▶ 运行按钮 (Shift + F10)！")
        print("    DevEco Studio 会自动重新打包、自动签名并直接将 APP 推送到虚拟手机中打开！")
        print("=" * 65)
        input("\n按回车键退出...")
        return

    # 如果有多个有效手机设备，选择第一个 Ready 的，或者第一个
    target_device = None
    for dev_id, status, _ in valid_devices:
        if status.lower() == "ready":
            target_device = dev_id
            break
    if not target_device:
        target_device = valid_devices[0][0]

    print(f"\n  [选中目标设备] {target_device}")
    print(f"  [正在安装应用] {os.path.basename(HAP_PATH)} ...")

    install_res = run_cmd([HDC_PATH, "-t", target_device, "install", "-r", HAP_PATH])
    print(f"  [安装执行结果] {install_res}")

    if "msg:install success" in install_res.lower() or "success" in install_res.lower():
        print("\n" + "=" * 65)
        print("  🎉 安装成功！")
        print("  现在请在虚拟手机桌面上查看，【守夜灯 / ShadowLamp】已重新出现！")
        print("=" * 65)
    else:
        print("\n" + "=" * 65)
        print("  [提示] 命令行安装遇到设备鉴权或签名限制。")
        print("  ★ 终极解决方案：")
        print("    回到 DevEco Studio 窗口，直接点击顶部的绿色 ▶ 运行按钮（Shift + F10）！")
        print("    IDE 会自动附带调试签名，100% 成功安装并直接在虚拟手机中拉起 APP！")
        print("=" * 65)

    input("\n按回车键退出...")

if __name__ == "__main__":
    main()
