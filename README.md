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

## 🚀 硬件供电与双模组网架构

系统支持**「独立电源无线组网」**与**「USB 有线串口调试」**双模热备，硬件固件自适应切换，无需重新烧录：

### 模式一：独立供电与无线 Wi-Fi 组网（推荐演示与实际部署）
- **供电方式**：小熊派通过 Type-C 接口连接 5V 充电宝或手机充电头，彻底脱离电脑摆放于老人房间任意位置。
- **传输通道**：硬件内置海思 Hi3861 2.4GHz Wi-Fi 基带，开机自动连接家庭 Wi-Fi 或手机热点，通过非阻塞 UDP 数据报（端口 `9999`）秒级将数据推送到电脑网关。
- **Wi-Fi 配置**：在 `bearpi-hm_nano/src/applications/sample/wifi-iot/app/elderly_care_system/elderly_care_config.h` 中可自定义 Wi-Fi SSID、密码与网关 IP（如电脑局域网 IP 或 `255.255.255.255` 广播）。
- **边缘安全与断网补传**：
  - **离线安全兜底**：若路由器断电或 Wi-Fi 信号中断，板载状态机毫秒级巡检不受任何影响，跌倒/SOS/火警本地 PWM 蜂鸣器与 LED 报警 **100% 独立运作**，生命安全不依赖云端。
  - **环形缓存对账**：断网期间遥测数据自动存入 RAM 环形缓存（深度 24 条），网络恢复后自动触发 `NetworkServiceFlushCache()` 错峰回填网关，无数据断层。

### 模式二：USB 有线调试模式（开发联调）
- **供电方式**：通过 USB 数据线直接连接电脑 USB 口。
- **传输通道**：通过板载 CH340 虚拟串口（115200 波特率）输出标准 JSON 遥测流，`hardware_bridge.py` 自动识别并同步至网关。

---

## 💻 快速启动指南

### 1. 启动电脑端中枢服务
在 `shou_ye_ren` 目录下双击运行 `start_all.bat` 或执行：
```bash
python D:\Dream\shou_ye_ren\start_all.py
```
- 后台同时启动 HTTP 服务（端口 `8780`）与 UDP 无线遥测接收服务（端口 `9999`）。
- 启动器将自动打印当前电脑的局域网 IP（例如 `http://10.202.41.10:8780`）。
- 电脑浏览器访问 `http://localhost:8780/` 可直接打开 Web 全景大屏。

### 2. 手机端 / 虚拟机运行
1. 打开 **DevEco Studio**，导入 `D:\Dream\shou_ye_ren\app\ShadowLamp` 工程。
2. 启动模拟器或连接真机，点击右上角绿色 **Run (Shift + F10)** 安装运行。
3. 在 APP「我的」页面中输入控制台打印的局域网网关地址（如 `http://10.202.41.10:8780`）即可实时联动。
