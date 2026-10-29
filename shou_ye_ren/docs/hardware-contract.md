# 硬件接入约定（给固件同学的一页纸）

> 目标：开发板只做三件事——**采集、判定、上报**；存储、闭环、报表都在网关。
> 只要按本文的报文与重试规则实现，硬件到位后不需要改网关代码。

---

## 1. 连接方式

| 项 | 约定 |
|---|---|
| 协议 | HTTP/1.1，`POST http://<电脑IP>:8780/api/v1/snapshot` |
| 编码 | UTF-8 JSON |
| 地址 | 网关启动时会打印 `局域网: http://192.168.x.x:8780` |
| 地址配置 | **不要写死在固件里**，放配置区或串口可改；演示现场 IP 会变 |

## 2. 上行报文

```json
{
  "deviceId": "bearpi-001",
  "ts": "2026-11-02T14:03:00+08:00",
  "seq": 1024,
  "pirDetected": true,
  "lastMotionTs": "2026-11-02T14:02:58+08:00",
  "lastMotionZone": "bathroom",
  "temperature": 23.4,
  "humidity": 51.0,
  "lux": 120.5,
  "doorOpen": false,
  "battery": null,
  "powerState": "mains",
  "rssi": -52,
  "online": true,
  "source": "real",
  "fwVersion": "v0.1.0"
}
```

必填：`deviceId`、`ts`、`seq`。
`lastMotionZone` 取值：`living / bedroom / bathroom / kitchen / entry / unknown`。
`source`：真实采集填 `real`，按键模拟填 `key_sim`，**不要**拿 `real` 冒充模拟。
未知值用 `null`，不要填 0。

### 求助按钮（事件，单独发，不要等心跳）

```json
POST /api/v1/events
{ "deviceId": "bearpi-001", "ts": "...", "type": "SOS", "source": "real",
  "payload": { "zone": "living" } }
```

`type` 支持 `SOS / GAS / SMOKE`，均直接建 L3。

## 3. 频率

- 心跳：**5 秒一帧**足够（1 秒一帧 = 86400 行/天，SQLite 约 10MB/天，没必要）。
- 事件（SOS、门磁变化）：**立即发**，不等下一个心跳。
- 开发板内存紧张时，可以只发“状态变化”+ 每 30 秒一次的全量心跳。

## 4. 重试与断网缓存（这条决定演示稳不稳）

1. 发送失败 → 重试 3 次：间隔 200ms / 1s / 3s。
2. 仍失败 → 写入**环形队列**（建议 200 条），不丢当前数据。
3. 网络恢复 → 按 `seq` 升序补传，**允许乱序**，网关按 `(deviceId, seq)` 去重。
4. 补传期间不要阻塞采集与本地告警——本地声光必须照常工作。

## 5. seq 规则

- 上电从 `1` 开始，每次发送 +1。
- **不需要**把 seq 存到 Flash：网关会识别“同一 seq 隔了 2 分钟以上又出现”为设备重启，自动重编号，不会覆盖历史。
- 也就是说：重启后放心从 1 开始。

## 6. 时间

- Hi3861 没有 RTC，**上电后必须先做一次 SNTP 对时**。
- 对时前可以先上报，但 `ts` 若是“未来时间”，网关会强制改用服务器时间并在响应里回 `"timeAdjusted": true`。
- 绝对时间错太多不会影响“无活动时长”判定（相对时间自洽），但会让**日报落到错误的日期**，所以对时还是必须做。

## 7. 响应与错误

| 状态码 | 含义 | 固件该做什么 |
|---|---|---|
| 200 | 成功 | 正常；返回里 `alarms` 非空说明触发了告警 |
| 400 | 报文不合法 | 检查 `message` 字段，**不要盲目重试** |
| 404 | 路径错 | 检查 URL |
| 5xx | 网关异常 | 退避重试 + 进缓存 |

成功响应示例：

```json
{"ok": true, "seq": 1024, "timeAdjusted": false, "alarms": ["EVT-20261102-0003"]}
```

## 8. 联调自检（三条命令）

```powershell
curl http://127.0.0.1:8780/health          # 网关活着
curl http://127.0.0.1:8780/api/v1/snapshot # 收到最新一帧
curl http://127.0.0.1:8780/api/v1/device   # 设备在线状态
```

## 9. 后续可选：MQTT

若之后接 MQTT（长连接更省电），主题约定：

- 上行 `dev/{deviceId}/up`，QoS 1，payload 同上
- 下行 `dev/{deviceId}/cmd`，QoS 1，`{cmd:"setThreshold"|"reboot", args:{}}`

网关侧只需把 MQTT 消息转成一次 `POST /api/v1/snapshot`，业务层不用改。

## 10. 板端本地判定（断网也不失守）

网关挂了、Wi-Fi 断了，板子仍要能判断最要紧的那几件事。这层逻辑在 `local_rule.c`，纯逻辑、
不碰 GPIO 也不碰 socket（`check_firmware.py` 会检查这一点），阈值全部来自 `config.h`，并与网关
`config.py::Thresholds` 同值（脚本逐项比对，改一处漏改另一处直接 FAIL）。

| 板端类型 | 触发条件 | 本地等级 | 确认窗 |
|---|---|---|---|
| `TEMP_FIRE` | 温度 ≥ 55℃ | L3 | 立即 |
| `TEMP_RISE` | 180 秒内升温 ≥ 8℃ | L3 | 立即 |
| `TEMP_HIGH` / `TEMP_LOW` | ≥ 35℃ / ≤ 16℃ | L2 | 30 秒 |
| `HUM_HIGH` / `HUM_LOW` | ≥ 80% / ≤ 25% | L2 | 30 秒 |
| `DOOR_OPEN` | 门磁开 ≥ 10 分钟 | L2 | 30 秒 |
| `TEMP_WARM` | ≥ 28℃ | L1 | 30 秒 |
| `HUM_WARN_HIGH` / `HUM_WARN_LOW` | ≥ 70% / ≤ 35% | L1 | 30 秒 |
| `SOS` / `NO_MOTION` | 按键 / 无活动 | L3 / L2 | 由 `biz.c` 状态机产出 |

四条纪律：

1. **确认窗 + 回差**：判据要持续 `CFG_ENV_CONFIRM_MS`（30 秒）才升级；恢复也要持续
   `CFG_ENV_CLEAR_MS`（60 秒）才降级。
2. **L3 不免确认窗的不确认**：求助与疑似火情是立即项。
3. **板端只做“够不够急”**：外出抑制、夜间加权、冷却去重都在网关。
4. **本地等级 ≠ 台账等级**：事件仍全量上报，最终等级以网关为准；本地等级只驱动本机声光。

LED（单色灯，不假装能亮绿）：

| 状态 | LED |
|---|---|
| 无告警 | 灭 |
| 待确认 L1 / L2 / L3 | 800 / 400 / 150 毫秒闪烁（越急越快） |
| 已被确认 | 常亮（“有人接手了”） |

## 11. 下行确认（网关 → 板端）

上行只能说明“我报了”，板子并不知道家属看没看。固件每 `CFG_ACK_POLL_MS`（10 秒）问一次：

```
GET /api/v1/alarms/active?deviceId=shouye-01
```

```json
{"deviceId":"shouye-01","pendingCount":1,"acknowledgedCount":0,
 "pending":[{"eventId":"EVT-20261027-0003","type":"TEMP_HIGH","level":"L2"}],
 "serverTime":"2026-10-27T09:14:02+08:00"}
```

板端只取两个整数（不解析数组，省 RAM）：

| 字段 | 板端行为 |
|---|---|
| `pendingCount > 0` | 保持声光，节奏按本地等级 |
| `pendingCount == 0` 且本地有 L2/L3 | 停响 + LED 常亮 = 已有人处理 |
| 请求失败 / 超时 | 维持现状，下个周期再问，**不改台账** |

这条链路是**只读**的：`acknowledged` 在固件里表示“已上报成功”，与“家属已确认”不是一回事，
界面文案不要混用。

## 12. 变更记录

- 2026-10-27：新增板端本地判定（`local_rule.c`）与下行确认（`/alarms/active`）。
- 快照仍 16 字段；板端可产生的告警类型固定 10 种（网关 `config.py::BOARD_EVENT_TYPES`）。
- 端口统一 8780（UDP 9998）。
