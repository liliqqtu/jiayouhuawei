@echo off
chcp 936 >nul
title 独居老人居家安全守护系统 - 局域网服务中枢

echo ==========================================================
echo    正在启动局域网网关服务 (HTTP: 8780, UDP: 9999)...
echo ==========================================================

REM 启动后台网关（同时开启 HTTP 与 UDP 9999 端口）
start "守夜人-网关服务(端口8780/UDP9999)" cmd /k "cd /d D:\Dream\shou_ye_ren && python run_gateway.py --lan --udp-port 9999"

timeout /t 2 >nul

REM 启动硬件守护与同步桥接器
start "守夜人-小熊派硬件同步桥接器" cmd /k "cd /d D:\Dream\shou_ye_ren && python hardware_bridge.py"

echo.
echo ==========================================================
echo   服务已全部就绪！
echo.
echo   【硬件供电与组网模式】
echo     - 独立供电模式: 小熊派插充电宝/电源头，Wi-Fi 自动上报 UDP:9999
echo     - USB 有线模式: 小熊派插电脑 USB，桥接器自动识别 CH340 串口
echo.
echo   【手机/虚拟机 APP 配置】
echo     请在 APP「我的」页面中填入本机局域网网关地址：
echo     http://10.202.41.10:8780
echo.
echo   【浏览器大屏直接访问】
echo     http://10.202.41.10:8780/
echo ==========================================================
echo.
pause
