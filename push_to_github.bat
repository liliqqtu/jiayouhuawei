@echo off
chcp 936 >nul
title 推送项目到 GitHub - liliqqtu/jiayouhuawei

set "PATH=D:\git\Git\cmd;%PATH%"

echo ==========================================================
echo   正在准备推送项目到 GitHub...
echo   目标仓库: https://github.com/liliqqtu/jiayouhuawei
echo ==========================================================
echo.

cd /d D:\Dream

echo [1/3] 检查远程仓库配置...
git remote -v

echo.
echo [2/3] 检查最新提交信息...
git log -1 --oneline

echo.
echo [3/3] 正在推送到 GitHub main 分支...
echo （若屏幕弹出 GitHub 登录授权窗口，请点击 "Sign in with your browser" 授权即可）
echo.

git push -u origin main

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ==========================================================
    echo   ? 推送成功！
    echo   请访问 GitHub 查看：https://github.com/liliqqtu/jiayouhuawei
    echo ==========================================================
) else (
    echo.
    echo ==========================================================
    echo   ?? 推送未完成。
    echo   常见原因：
    echo   1. 首次推送需要授权：若弹出了浏览器登录界面，请完成授权后重新运行。
    echo   2. 网络原因：请确保代理软件 (如 Clash) 处于开启状态。
    echo ==========================================================
)

echo.
pause
