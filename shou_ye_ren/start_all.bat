@echo off
title 独居老人居家安全守护系统 - 局域网服务中枢

echo ==========================================================
echo    正在启动局域网网关服务 (端口: 8780)...
echo ==========================================================

REM 启动后台网关
start "守夜人-网关服务(端口8780)" cmd /k "cd /d D:\Dream\shou_ye_ren && python run_gateway.py --lan"

timeout /t 2 >nul

REM 启动硬件数据同步桥接器
start "守夜人-小熊派硬件同步桥接器" cmd /k "cd /d D:\Dream\shou_ye_ren && python hardware_bridge.py"

echo.
echo ==========================================================
echo   服务已全部就绪！
echo.
echo   【手机/虚拟机查看方式 A：华为手机原生 APP】
echo     1. 确保手机/虚拟机与电脑网络相通
echo     2. 在 APP「我的」页面中填入网关地址：
echo        http://10.202.43.40:8780
echo.
echo   【手机/电脑端查看方式 B：自带浏览器】
echo     电脑或手机浏览器直接访问：
echo     http://10.202.43.40:8780/
echo ==========================================================
echo.
pause
