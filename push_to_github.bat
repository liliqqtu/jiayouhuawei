@echo off
chcp 936 >nul
title GitHub 同步推送工具 - liliqqtu/jiayouhuawei

echo ==========================================================
echo    正在检查并推送到 GitHub 仓库...
echo    https://github.com/liliqqtu/jiayouhuawei
echo ==========================================================

cd /d D:\Dream
"D:\git\Git\cmd\git.exe" status

echo.
echo 正在添加所有改动...
"D:\git\Git\cmd\git.exe" add .

echo 正在提交改动...
"D:\git\Git\cmd\git.exe" commit -m "feat: 支持小熊派独立供电无线Wi-Fi组网(UDP 9999)与边缘断网环形对账"

echo 正在推送到 GitHub (origin main)...
"D:\git\Git\cmd\git.exe" push origin main

echo.
echo ==========================================================
echo    推送完成！
echo ==========================================================
pause
