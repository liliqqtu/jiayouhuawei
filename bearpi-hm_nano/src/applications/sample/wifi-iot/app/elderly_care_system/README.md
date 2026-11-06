# 独居老人居家安全监测与应急辅助系统 (Southbound Embedded Core)

## 一、 快速导览与代码目录结构

本项目基于 **OpenHarmony LiteOS-M** 操作系统与海思 **Hi3861V100 RISC-V SoC** 开发，是独居老人智慧养老解决方案的核心南向感知控制枢纽。

```
elderly_care_system/
├── BUILD.gn                      # GN 编译构建规则脚本
├── elderly_care_config.h         # 系统全局硬件引脚映射、告警阈值与系统配置
├── elderly_care_main.c           # 系统主入口、多任务调度初始化与状态机轮询
├── emergency_sos.c / .h          # 双通道紧急求助检测 (板载F1 + 433MHz无线遥控开关)
├── sensor_infrared.c / .h        # E53_IS1 人体活动红外热释电传感器与活动计数 (GPIO_7)
├── alarm_beeper.c / .h           # E53_IS1 无源蜂鸣器 PWM1 占空比声音驱动 (GPIO_8)
├── sensor_environment.c / .h     # E53_IA1 温湿度 SHT30 I2C1 采集 + SF1 烟雾 ADC 采集
├── sensor_door.c / .h            # 门窗磁干簧管状态监测与开门超时计时 (GPIO_10)
├── voice_syn6288.c / .h          # SYN6288 适老化中文语音合成播报驱动 (UART1)
├── network_service.c / .h        # Wi-Fi 连接管理、UDP 遥测上报与环形离线缓存队列
└── docs/                         # 开发者工程与架构指南
    ├── 01_公网物联网平台接入与架构设计指南.md
    └── 02_设备免烧录配网方案与多热点自动回退.md
```

---

## 二、 核心硬件引脚分配清单（零冲突设计）

| 外设模块 | 功能名称 | 物理 GPIO | J1 座子丝印 | 信号类型与电气属性 |
| :--- | :--- | :--- | :--- | :--- |
| **SYN6288 语音模块** | TXD 发送文本 | `GPIO_6` | `UART_TX` | 串口发送，波特率 9600 8N1，必须接模块 5V 功放供电 |
| | BUSY / 反馈 | `GPIO_5` | `UART_RX` | 串口接收（可选接线） |
| **E53_SF1 烟雾/燃气**| AO 模拟电压 | `GPIO_13` | `ADC` | `ADC Channel 6`，加热线圈必须接 5V |
| **E53_IS1 人体红外** | OUT 中断脉冲 | `GPIO_7` | `GPIO2` | 内部弱上拉，人体走动时输出上升沿硬件中断 |
| **E53_IS1 无源蜂鸣** | BEEP 驱动 | `GPIO_8` | `GPIO3` | `PWM1_OUT`，4kHz 占空比 50% 驱动 |
| **E53_IA1 微气候温湿**| SDA / SCL | `GPIO_0` / `GPIO_1` | `IIC_SDA`/`SCL` | I2C1 硬件总线，读取 SHT30 温湿度 |
| **干簧管门窗磁** | 开关量信号 | `GPIO_10` | `SPI_SCK` | 内部弱上拉，门关接地=0，门开断开=1 |
| **板载 F1 紧急 SOS** | 物理求助按键 | `GPIO_11` | 板载 F1 | 内部弱上拉，按压接地（低电平有效） |
| **433MHz 无线遥控** | 远程无线求助 | `GPIO_14` 或并联 J16 | J1 Pin 4 / Pin 16 | 白线接检测脚，蓝线接 GND，按下继电器闭合接地 |
| **板载 F2 复位键** | 现场消警复位 | `GPIO_12` | 板载 F2 | 内部弱上拉，按压接地，用于现场消警 |
| **板载状态指示灯** | 系统运行 LED | `GPIO_2` | 板载 LED | 正常心跳慢闪（1Hz），告警急促爆闪（5Hz） |
| **USB 调试串口** | 日志与遥测 | `GPIO_3` / `GPIO_4` | `UART0` | 115200 8N1，持续输出标准 JSON 遥测帧 |

---

## 三、 固件编译与烧录指令

本工程使用 DevEco Device Tool 或 OpenHarmony 独立编译器构建：

```bash
# 1. 编译命令 (在工程根目录执行)
D:\DevEco_Studio\DevEco-Device-Tool\core\deveco-venv\Scripts\hos.exe run --project-dir D:\Dream\bearpi-hm_nano --environment hi3861

# 2. 烧录文件输出路径
D:\Dream\bearpi-hm_nano\out\bearpi-hm_nano\wifiiot_hispark_pegasus\Hi3861_wifiiot_app_allinone.bin
```

---

## 四、 常见开发问题排查与速查

1. **HiBurn 烧录提示 `Wait connect success flag overtime`**：
   - 原因：面包板外设（如 MQ-2 传感器加热丝 160mA 或杜邦线短接 UART0）拉低了 USB 供电。
   - 解决：烧录前先拔掉面包板 5V 杜邦线，点击烧录出现 `Connecting...` 后果断按一下小熊派黄色 RST 复位键，烧录成功后再插回 5V。
2. **SYN6288 开机不发声**：
   - 检查模块是否接 5V（接 3.3V 功放供电不足无声）；
   - 文本帧长必须严格为 `文本长度 + 3`（含命令字与校验和）；
   - 中文字符串需为 GBK 编码（UTF-8 汉字会被芯片丢弃）。
3. **433MHz 无线遥控接收按键无反应**：
   - 必须接满 4 根线：红线接 5V，黑线接 GND，蓝线（COM）必须接 GND，白线（NO）接 J16（或 J4）；
   - 按下接收板黑色配对键，红灯常亮后按遥控器按键，对码成功后方可吸合继电器。