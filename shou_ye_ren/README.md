# 守夜灯 · 软件工程（网关 + 模拟器 + 自测）

赛题四「独居老人居家安全监测与应急辅助系统」的软件侧实现。
**零第三方依赖**，只用 Python 标准库 —— 队友拷走就能跑，演示当天不会因为装包失败翻车。

---

## 1. 现在能跑什么

| 能力 | 状态 | 验证方式 |
|---|---|---|
| 本地网关（14 个接口） | ✅ 可运行 | `python run_tests.py` 20 项全绿 |
| 告警判定（三档 + 三条融合规则） | ✅ 可运行 | 模拟器 5 个场景 |
| 设备模拟器（替代未到货的硬件） | ✅ 可运行 | 见第 4 节 |
| SQLite 持久化 + 断网补传去重 | ✅ 可运行 | `seq` 幂等、补传覆盖 |
| 北向 APP（ArkTS） | 🚧 下一步 | 工程骨架已在 `app/ShadowLamp` |

## 2. 目录

```
software/
├─ run_gateway.py          # 启动网关
├─ run_sim.py              # 启动设备模拟器
├─ run_tests.py            # 跑自测
├─ gateway/
│  ├─ util.py              # 时间/JSON/类型工具
│  ├─ config.py            # 阈值与常量（可远程下发）
│  ├─ store.py             # SQLite 表结构与 DAO
│  ├─ rules.py             # 告警规则引擎
│  ├─ api.py               # 路由与业务组装
│  ├─ dashboard.py         # Web 看板（浏览器直接看，不依赖 APP）
│  └─ server.py            # HTTP 服务
├─ sim/device_sim.py       # 设备模拟器
├─ tests/                  # 57 项自测（test_api 44 + continuity 8 + evidence 2 + event_id 3）
├─ scripts/api.py          # 命令行客户端（PowerShell 里替代 curl）
├─ scripts/verify_devices.py  # 多设备隔离验证（需网关在跑）
├─ scripts/clean_test_devices.py  # 清理自检留下的 bench-01/02 假设备
├─ scripts/verify_multi.py    # 多设备+看板下拉验证（自带临时网关，跑完自清）
├─ scripts/
│  ├─ demo.ps1             # 一键演示：网关 + 看板 + 模拟器
│  ├─ rehearse.ps1         # 预演收尾：先清干净再起，并打印 7 个演示命令
│  ├─ bench.py             # 压测/容量评估（打 p50/p95 与吞吐）
│  ├─ check_firmware.py    # 固件静态检查（无板子也能查）
│  ├─ check_words.py       # 用词合规扫描
│  └─ sync_app.ps1         # APP 源码同步 + 构建
└─ app/ShadowLamp/         # 北向 APP 工程（DevEco Studio 打开）
```

## 3. 三步启动

> 现场最省事的开法：`powershell -ExecutionPolicy Bypass -File scripts\demo.ps1`
> 一条命令起「网关 + Web 看板 + 模拟器」，停止用 `-Stop`。

```powershell
cd software
python run_gateway.py --lan          # 1. 起网关（--lan 才能被手机访问）
python run_sim.py --scenario night --base 02:10   # 2. 另开一个窗口，触发场景
python run_tests.py                  # 3. 任何时候都能跑自测
```

启动后网关会打印两个地址：

```
本机访问: http://127.0.0.1:8780/health
网 页 看 板: http://192.168.x.x:8780/   （浏览器直接打开，手机也行）
局域网  : http://192.168.x.x:8780  （APP 里填这个）
```

## 4. 演示脚本（5 个场景，均可复现）

| 命令 | 触发 | 预期结果 |
|---|---|---|
| `python run_sim.py --scenario daily` | 一天正常活动 | 无告警，报告里出现活动分布 |
| `python run_sim.py --scenario night --base 02:10` | 夜间卫生间 3 分钟无活动 | **L3** 夜间卫生间连续无活动 |
| `python run_sim.py --scenario away` | 开门→关门→家中无人 | **不产生**无活动告警（外出抑制生效） |
| `python run_sim.py --scenario sos` | 老人按随身按钮 | **L3** 紧急求助 |
| `python run_sim.py --scenario env` | 温度 33.5℃ | **L1** 温度超限 |
| `python run_sim.py --loop` | 持续心跳 | 供 APP 联调用 |

> 模拟数据一律标 `source=key_sim`，APP 界面会显式提示“模拟输入”，不会冒充真实采集。

## 5. 接口速查

| 方法 | 路径 | 用途 |
|---|---|---|
| GET | `/` | **Web 看板**（浏览器打开即看，零依赖、断网可渲染） |
| GET | `/health` | 健康检查 |
| POST | `/api/v1/snapshot` | 设备上报快照（核心入口） |
| GET | `/api/v1/snapshot` | 最新快照 + 在线/数据新鲜度 |
| GET | `/api/v1/snapshots?from=&to=&limit=` | 历史快照 |
| GET | `/api/v1/alarms?status=` | 告警列表 |
| GET | `/api/v1/alarms/{id}` | 告警详情 |
| POST | `/api/v1/alarms/{id}/ack` | 确认 |
| POST | `/api/v1/alarms/{id}/close` | 关闭并写处置 |
| GET | `/api/v1/reports/daily?date=` | 日报（含 24 小时分布、环比、基线） |
| GET | `/api/v1/reports/weekly?week=` | 周报 |
| GET | `/api/v1/reports/monthly?month=` | 月报 |
| GET | `/api/v1/alarms/active?deviceId=` | 板端轮询：`pendingCount` / `acknowledgedCount`（下行确认） |
| GET | `/api/v1/device` | 在线/供电/信号 |
| GET | `/api/v1/devices` | 全部设备清单（多设备切换） |
| POST | `/api/v1/devices` | 登记设备（还没上报时先占位，body `{"deviceId":"bench-01"}`） |
| GET/POST | `/api/v1/config/threshold` | 阈值查询 / 下发 |
| POST | `/api/v1/events` | 设备直报事件（SOS/燃气/烟雾） |

错误统一 `{code, message}`；状态非法迁移返回 **409**。

## 6. 判定规则

三档：`L1 提醒`（环境越界）→ `L2 关注`（白天长时间无活动）→ `L3 紧急`（夜间卫生间无活动 / 求助 / 门磁开后未返回）。

板端兜底（实现于固件 `local_rule.c`，断网也判）：温度/湿度/门磁按 `config.h` 阈值给 L1/L2/L3，
带 30 秒确认窗与 60 秒回差；疑似火情与求助直接 L3；本地等级只驱动声光，**最终等级以网关为准**。

三条融合规则（实现于 `rules.py`）：

1. **外出抑制**：门磁 `开→关` 后无活动 ⇒ 进入疑似外出，不报 L2；超过 `away_max_min` 才提醒。
2. **时段加权**：22:00–06:00 阈值收紧，卫生间直接判 L3。
3. **冷却去重**：同设备 + 同类型在活动期内不重复建单。

阈值默认值（`config.py`，均为**演示参数**，非医疗标准）：
白天 5 分钟、夜间 3 分钟、夜间卫生间 2 分钟、门窗开启 10 分钟、升级 10 分钟。

## 7. 与交付材料的对应

| 交付物 | 对应位置 |
|---|---|
| `赛题四_需求符合性矩阵.xlsx` 04 表 | 本工程全部接口 |
| 05 数据字段 | `store.py` 建表 + `snapshot_json/alarm_json` |
| 06 测试矩阵 T01–T13 | `tests/test_api.py` |
| 设计方案 3.3 / 3.5 / 3.6 | `rules.py`、状态机、`api.py` 报表 |

## 8. 环境与兼容

- **Python 3.8.10**（与 DevEco Device Tool 要求一致），本工程不使用 3.9+ 语法。
- 无第三方依赖：不需要 pip install。
- 数据库默认 `data/gateway.db`，可用 `--db` 指定；删除即重置。

## 9. 下一步

1. 写完 `app/ShadowLamp` 的四个页面（首页 / 告警 / 报告 / 我的），对接本网关。
2. 把工程整体拷到 `D:\DevEcoProjects\shadow-lamp-app`（**不要**放在中文或 OneDrive 路径下）。
3. 用 `--loop` 模拟器持续喂数据，边开发边看效果。

## 10. 已知限制

- 网关无鉴权，仅限局域网演示；答辩时说明“内网 + 演示用”。
- 无 MQTT Broker：设备侧用 HTTP 上报；若后续接 MQTT，把 `dev/{deviceId}/up` 的消息转成 `POST /api/v1/snapshot` 即可，业务层不用改。
- 报表基线用“有数据的天”求平均，冷启动阶段提示为“数据不足”。

---

## 11. 北向 APP（ArkTS，已可编译）

工程位置：

```
软件/app/ShadowLamp          # 源码（工作区内）
D:\DevEcoProjects\shadow-lamp-app   # 实际构建位置（全英文路径）
```

### 11.1 页面

| 页签 | 文件 | 做什么 |
|---|---|---|
| 首页 | `view/HomeTab.ets` | 状态灯、最后活动、温湿度门磁、长按 1 秒求助 |
| 告警 | `view/AlarmTab.ets` | 待确认/处理中/已关闭分栏，卡片内确认 + 处置（枚举） |
| 报告 | `view/ReportTab.ets` | 今日次数、环比、2 小时时段柱状图、7 日基线提示、本周概览 |
| 我的 | `view/MineTab.ets` | 网关地址、连接自检、数据来源与判定口径说明 |

配套：`model/Models.ets`（类型与文案映射）、`service/ApiClient.ets`（HTTP + preferences）、`common/Theme.ets`（与 PPT 同一套色板）。

### 11.2 构建

命令行（不打开 IDE）：

```powershell
powershell -ExecutionPolicy Bypass -File software\scripts\build_app.ps1
```

已验证结果：**BUILD SUCCESSFUL**，产物
`D:\DevEcoProjects\shadow-lamp-app\entry\build\default\outputs\default\entry-default-unsigned.hap`（约 310 KB）。

两个坑（已踩过并写进脚本）：

1. `DEVECO_SDK_HOME` 要指向 `DevEco Studio\sdk`（集成式，里面是 `default/sdk-pkg.json`），
   指到外挂的 `DevEco Studio sdk` 会报 `00303312 Cannot find the corresponding SDK version`。
2. 打包阶段需要 Java，用 Studio 自带的 `jbr\bin` 加进 PATH，否则报 `spawn java ENOENT`。

### 11.3 安装到设备

命令行只产出 **unsigned HAP**。装模拟器/真机有两种做法：

- 在 DevEco Studio 里打开工程 → 登录华为账号 → 自动签名后 Run（推荐，最省事）；
- 或自行配置 `build-profile.json5` 的 `signingConfigs` 后重新构建。

### 11.4 联调

1. 电脑起网关：`python run_gateway.py --lan`
2. 记下打印的局域网 IP，例如 `http://192.168.1.100:8780`
3. APP →「我的」→ 填入地址 → 保存并测试连接
4. 电脑另开窗口：`python run_sim.py --loop` 持续喂数据
5. 首页应看到数据变化；跑 `--scenario night` 可看到 L3 告警进入告警页
D:\python3.8\python.exe D:\Dream\shou_ye_ren\hardware_bridge.py