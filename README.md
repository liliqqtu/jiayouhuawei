# 独居老人居家安全监测与应急辅助系统 (守夜灯)

> 河南省第三届高校信息技术应用创新大赛 · 开源鸿蒙赛道 (赛题四)  
> 软硬件全栈集成解决方案

---

## 📌 项目简介

本项目针对当前独居老人居家跌倒、夜间突发急病、门窗长开遗忘等生活安全痛点，设计并实现了一套**端-边-云-移动端**联动的居家安全监测与应急辅助系统。

- **南向硬件感知**：基于 **BearPi-HM Nano（小熊派 Hi3861）** 搭载 OpenHarmony 轻量系统，采集 PIR 人体红外、门磁状态、室内温湿度与板载 SOS 实体按键，毫秒级响应声光本地告警与串口遥测推流。
- **边缘网关中枢**：基于 Python 高性能轻量网关，具备局域网自发现、设备生命周期探测、多级告警裁决与本地数据库持久化。
- **北向移动应用**：基于 **OpenHarmony ArkTS** 研发的「守夜灯」原生应用，提供关怀模式大字界面、微气候环境大屏、实时活动对账与一键求助闭环。

---

## 📂 仓库目录结构

```text
D:\Dream
├── bearpi-hm_nano/                                    # 【南向硬件】小熊派 OpenHarmony 源码与方案
│   ├── src/applications/sample/wifi-iot/app/
│   │   └── elderly_care_system/                       # 核心业务驱动与仲裁状态机
│   │       ├── elderly_care_main.c                    # 主任务调度与遥测输出
│   │       ├── sensor_infrared.c                      # E53_IS1 PIR 人体红外传感器
│   │       ├── sensor_door.c                          # 门磁状态与消抖检测
│   │       ├── emergency_sos.c                        # F1/F2 实体求助按键
│   │       └── alarm_beeper.c                         # PWM 蜂鸣器与双色 LED 驱动
│   ├── firmware/                                      # 已编译烧录固件二进制包 (All-in-one .bin)
│   ├── 独居老人居家安全监测与应急辅助系统_设计方案.docx # 匿名标准设计方案
│   ├── 10分钟答辩路演PPT结构与逐页讲稿.md              # 答辩路演逐页演讲稿
│   └── 5分钟演示视频分镜头脚本与录制指南.md            # 演示视频拍摄脚本
│
├── shou_ye_ren/                                       # 【北向软件 & 网关】
│   ├── app/ShadowLamp/                                # 华为鸿蒙 ArkTS 原生手机应用源码
│   │   └── entry/src/main/ets/
│   │       ├── pages/Index.ets                        # 应用主入口与 3 秒全局心跳
│   │       ├── view/HomeTab.ets                       # 居家安全概览大屏
│   │       ├── view/AlarmTab.ets                      # 告警流水与证据链溯源
│   │       └── view/MineTab.ets                       # 局域网网关配置与关怀模式
│   ├── gateway/                                       # Python 局域网边缘网关服务
│   ├── run_gateway.py                                 # 网关启动主入口 (端口 8780)
│   ├── hardware_bridge.py                             # 小熊派硬件数据 -> 网关实时同步桥接器
│   ├── start_all.bat                                  # 一键启动批处理
│   └── start_all.py                                   # 一键启动 Python 调度器
│
└── .gitignore                                         # 排除大文件压缩包与构建产物
```

---

## 🚀 快速启动指南

### 1. 启动电脑端后台服务
在 `shou_ye_ren` 目录下双击运行 `start_all.bat` 或执行：
```bash
python D:\Dream\shou_ye_ren\start_all.py
```
- 后台将自动连接小熊派硬件开发板串口（COM7）并启动端口 8780 的网关服务。
- 电脑浏览器访问 `http://localhost:8780/` 可直接打开 Web 监控大屏。

### 2. 手机端 / 虚拟机运行
1. 打开 **DevEco Studio**，导入 `D:\Dream\shou_ye_ren\app\ShadowLamp` 工程。
2. 启动模拟器或连接真机，点击右上角绿色 **Run (Shift + F10)** 安装运行。
3. 在 APP「我的」页面中输入电脑局域网 IP 网关（如 `http://<电脑IP>:8780`）即可实时联动。
