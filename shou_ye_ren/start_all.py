# -*- coding: utf-8 -*-
"""
独居老人居家安全守护系统 - 一键启动器
同时启动：
1. 局域网网关服务 (run_gateway.py --lan，端口 8780)
2. 硬件数据/模拟同步桥接器 (hardware_bridge.py)
"""

import os
import sys
import time
import subprocess
import webbrowser

DIR = os.path.dirname(os.path.abspath(__file__))

def main():
    print("=" * 60)
    print("      独居老人居家安全守护系统 · 一键启动中枢")
    print("=" * 60)
    print("  [1/2] 正在启动局域网网关服务 (端口: 8780)...")
    
    # 在独立窗口启动网关
    subprocess.Popen(
        ["cmd", "/k", "python", "run_gateway.py", "--lan"],
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
    print("=" * 60)
    print("  >>> 全部后台服务启动成功！<<<")
    print()
    print("  【手机/虚拟机查看方式 A：华为手机原生 APP】")
    print("    1. 确保手机/虚拟机与电脑网络通畅")
    print("    2. 在 APP「我的」页面中填入网关地址：")
    print("       http://10.202.43.40:8780")
    print()
    print("  【手机/电脑端查看方式 B：自带浏览器】")
    print("    电脑或手机浏览器直接访问：")
    print("    http://10.202.43.40:8780/")
    print("=" * 60)
    print()
    
    choice = input("是否直接在电脑浏览器中打开网页端大屏？(y/n，默认 y): ").strip().lower()
    if choice != "n":
        webbrowser.open("http://10.202.43.40:8780/")
        
    print("\n服务在后台运行中，可随时按 Ctrl+C 退出本窗口（后台服务仍会保持运行）。")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
