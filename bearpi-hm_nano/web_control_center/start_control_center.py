#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
河南省信息技术应用创新大赛（开源鸿蒙）- 赛题四
独居老人居家安全监测与应急辅助系统
北向控制中心一键启动助手
"""

import os
import sys
import webbrowser
import http.server
import socketserver

PORT = 8088
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

def main():
    print("=" * 60)
    print("  独居老人居家安全监测与应急辅助系统 - 控制中心启动器")
    print("  正在为您打开浏览器界面: http://localhost:8088/index.html")
    print("=" * 60)

    url = f"http://localhost:{PORT}/index.html"
    webbrowser.open(url)

    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print(f"Web 控制中心已在端口 {PORT} 持续运行，按 Ctrl+C 退出...")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n控制中心服务已安全停止。")

if __name__ == "__main__":
    main()
