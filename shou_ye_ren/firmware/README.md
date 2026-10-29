# 南向固件（BearPi-HM Nano / Hi3861）

> 说明：本目录代码按 BearPi 官方示例的 API 编写，**未在本机编译**（本机没有 Hi3861 源码树）。
> 第一次编译请预留 1~2 小时，按第 4 节逐条排查。

## 1. 文件说明

| 文件 | 层次 | 职责 |
|---|---|---|
| `config.h` | 配置 | Wi-Fi、网关 IP、引脚、阈值，**只改这一个文件** |
| `drv.c/h` | 驱动层 | PIR / 按键 / LED 的 GPIO 轮询与去抖 |
| `biz.c/h` | 业务状态 | 最后活动时间、告警判定、去重、SOS 优先级 |
| `local_rule.c/h` | 本地判定 | 温湿度/门磁阈值兜底（断网也判），**纯逻辑不碰硬件** |
| `sntp.c/h` | 时间 | 极简 SNTP 校时（无 RTC，必须校时） |
| `net.c/h` | 通信 | Wi-Fi STA + HTTP POST + 断网环形缓存补传 |
| `app_main.c` | 入口 | 两个任务：采集判定 / 上报 |
| `BUILD.gn` | 构建 | gn 配置。**7 个 .c 必须全在 sources 里**（曾漏 `env.c`，结果温湿度光照全是 null），`python scripts/check_firmware.py` 会校验 |

## 2. 移植步骤

1. 把整个 `shadow_lamp/` 拷到：
   `applications/BearPi/BearPi-HM_Nano/sample/shadow_lamp/`
2. 编辑同级 `sample/BUILD.gn`，在 `deps` 里加一行：
   `"//applications/BearPi/BearPi-HM_Nano/sample/shadow_lamp:shadow_lamp",`
3. 改 `config.h` 三处：`CFG_WIFI_SSID`、`CFG_WIFI_PASSWORD`、`CFG_GATEWAY_IP`。
4. 编译 → 烧录 → 串口看日志。

## 3. 串口应该看到什么

```
[drv] init ok (pir=11 key=12 led=2)
[biz] init, no_motion_threshold=180000 ms
===== ShadowLamp firmware v0.1.0 =====
[app] device=bearpi-001 gateway=192.168.1.100:8780
[net] wifi state -> 1
[net] connected, ip=192.168.1.23
[sntp] synced, unix=1793000000
[app] time synced: 2026-11-02T14:03:00+08:00
[app] seq=1 pir=0 alarm=0
```

- 在 PIR 前走动 → `pir=1`，`lastMotionTs` 跟着变
- 连续 `CFG_NO_MOTION_MS` 不动 → 串口打 `[biz] NO_MOTION`，LED 开始闪
- 按板载键 → `[biz] SOS triggered`，LED 快闪

## 4. 编译常见错误对照

| 报错 | 原因 | 处理 |
|---|---|---|
| `HI_IO_FUNC_GPIO_11_GPIO` 未定义 | SDK 版本不同 | 打开 SDK 的 `hi_io.h`，照枚举名改 `drv.c` 顶部三个 `*_FUNC` |
| `WifiEvent` 回调返回值不匹配 | 有的版本要求返回 `WifiErrorCode` | 把回调返回类型改掉即可 |
| `netifapi_dhcp_start` 未定义 | 头文件路径不同 | 确认 `lwip/netifapi.h` 已包含；或直接用官方 `wifi_connect.c` 替换 `Net_ConnectWifi()` |
| 连不上 Wi-Fi | 用了 5G 频段 | Hi3861 只支持 **2.4G** |
| 上报 404 | 网关没起 / IP 写错 | 电脑上 `curl http://IP:8780/health` 先确认 |

## 5. 与网关的约定

报文格式、`seq` 规则、重试策略见 `../docs/hardware-contract.md`。要点：

- `seq` 上电从 1 开始，**不用存 Flash**（网关会识别重启）
- 断网时进缓存（16 条），恢复后自动补传
- 时间靠 SNTP，失败时日志会明确提示

## 5.1 环境传感器与门磁（本次补强）

| 模块 | 文件 | 说明 |
|---|---|---|
| SHT30 温湿度 | env.c | I2C0，地址 0x44；读失败输出 null |
| BH1750 光照 | env.c | I2C0，地址 0x23；读失败输出 null |
| 门磁 | drv.c | CFG_DOOR_MODE：0=未接(输出 null) / 1=真实 GPIO / 2=长按按键模拟(source=test) |

Env_Init() 上电探测一次：模块不存在就永久输出 null，不刷错误日志。

## 5.2 告警状态机与串口 JSON

- 状态机：Active（触发）→ Acknowledged（上报成功）→ closed（条件恢复）
- 每条 JSON 在串口以 #JSON# 前缀再输出一份，供网关**串口桥接降级**使用
- 日志格式：[uptime][版本][模块][级别] msg code=N

## 5.3 板端本地判定（断网也不失守）

`local_rule.c` 是纯逻辑模块：不碰 GPIO、不碰 socket、不打日志，只吃 (时间, 温湿度, 门磁开合)。

- 判据与阈值全部来自 `config.h`，与网关 `config.py::Thresholds` 同值；两边改一处漏改另一处，
  `python scripts/check_firmware.py` 立刻 FAIL（它按名字逐项比对）。
- 确认窗 `CFG_ENV_CONFIRM_MS`（30 秒）防“手一捂就报警”；回差 `CFG_ENV_CLEAR_MS`（60 秒）
  防“铃声抖个不停”；`TEMP_FIRE` / `TEMP_RISE` 属立即项，不走确认窗。
- 只在**等级发生变化**时补报一次事件（`/api/v1/events`），不是每帧都报。
- 板端不做外出抑制 / 夜间加权 / 冷却去重——那些是网关的活；板端只回答“现在够不够急”。

LED 三态（单色灯，不假装能亮绿）：无告警灭；待确认按等级闪 L1 800ms / L2 400ms / L3 150ms；
已被确认则常亮（“有人接手了”）。蜂鸣器默认关，`CFG_BUZZER_EN=1` 且接好
`CFG_BUZZER_GPIO` 才在 L3 响。

## 5.4 下行确认（网关 → 板端）

板子上报成功不等于家属看到了。固件每 `CFG_ACK_POLL_MS`（10 秒）拉一次
`GET /api/v1/alarms/active?deviceId=...`，只解析两个整数：

- `pendingCount > 0`：继续按本地等级声光；
- `pendingCount == 0` 且本地还有 L2/L3：停响、LED 常亮 = 有人处理了；
- 请求失败：维持现状，下个周期再问（**这条链路只读，绝不改台账**）。

## 6. 已知限制

- 无 RTC：SNTP 失败时时间会是 1970 年起的开机计时，日报会错位。
- 只有 PIR + 按键：温湿度、门磁需另接模块后扩展 `drv.c`。
- `lastMotionZone` 固定为 `unknown`（单 PIR 无法定位），分区轨迹在网关侧由多设备实现。
- HTTP 短连接：5 秒一帧够用；若改 1 秒一帧建议上 MQTT。
- 没接温湿度模块时，本地判定只剩门磁与 PIR 两条线（温度类判据会因数据缺失自动跳过）。
- 蜂鸣器要外接（默认 `CFG_BUZZER_EN=0`），没接就是纯 LED 告警，不影响判定。
