# -*- coding: utf-8 -*-
"""
独居老人居家安全守护系统 - 一键启动器
同时启动：
1. 局域网网关服务 (run_gateway.py --lan --udp-port 9999，HTTP: 8780, UDP: 9999)
2. 硬件数据同步守护桥接器 (hardware_bridge.py)
"""

import os
import sys
import time
import socket
import subprocess
import webbrowser

DIR = os.path.dirname(os.path.abspath(__file__))

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

def main():
    local_ip = get_local_ip()

    print("=" * 64)
    print("      独居老人居家安全守护系统 · 一键启动中枢")
    print(f"      本机局域网 IP: {local_ip}")
    print("=" * 64)
    print("  [1/2] 正在启动局域网网关服务 (HTTP: 8780, UDP: 9999)...")
    
    # 在独立窗口启动网关
    subprocess.Popen(
        ["cmd", "/k", "python", "run_gateway.py", "--lan", "--udp-port", "9999"],
        cwd=DIR,
        creationflags=subprocess.CREATE_NEW_CONSOLE
    )
    
    time.sleep(2)
    print("  [2/2] 正在启动小熊派硬件同步桥接器...")
    
    # 在独立窗口启动硬件桥接器
    subprocess.Popen(
        ["cmd", "/k", "python", "hardware_bridge.py"],
        cwd=DIR,
        creationflags=subprocess.CREATE_NEW_CONSOLE
    )

    print()
    print("=" * 64)
    print("  >>> 全部后台服务启动成功！<<<")
    print()
    print("  【硬件连接双通道说明】")
    print("    * 独立供电模式: 小熊派接充电宝/电源头，Wi-Fi 自动连网 UDP:9999 发送")
    print("    * USB 调试模式: 小熊派插电脑 USB，通过串口实时读取并同步")
    print()
    print("  【手机/虚拟机查看方式 A：华为手机原生 APP】")
    print("    1. 确保手机/虚拟机与电脑处于同一局域网")
    print(f"    2. 在 APP「我的」页面中填入网关地址：")
    print(f"       http://{local_ip}:8780")
    print()
    print("  【手机/电脑端查看方式 B：自带浏览器】")
    print("    电脑或手机浏览器直接访问：")
    print(f"    http://{local_ip}:8780/")
    print("=" * 64)
    print()
    
    try:
        choice = input("是否直接在电脑浏览器中打开网页端大屏？(y/n，默认 y): ").strip().lower()
        if choice != "n":
            webbrowser.open(f"http://{local_ip}:8780/")
    except Exception:
        pass
        
    print("\n服务在后台运行中，可随时按 Ctrl+C 退出本窗口（后台服务仍会保持运行）。")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
