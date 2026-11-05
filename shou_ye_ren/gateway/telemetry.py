# -*- coding: utf-8 -*-
"""板端串口遥测适配层：把「简化遥测帧」翻译成标准 SensorSnapshot。

背景（2026-10-29）：
    比赛用的 Hi3861 固件（applications/sample/wifi-iot/app/elderly_care_system）
    每秒从串口 printf 一行：

        {"type":"telemetry","uptime":12,"motion":1,"no_motion_sec":0,
         "door":0,"door_sec":0,"temp":24.5,"humi":53.0,"sos":0,
         "alert":0,"level":0,"count":3}

    字段是「好读」的简写，也不带 deviceId / ts / seq。
    而网关内部只认 docs/hardware-contract.md 里的 16 字段快照（camelCase + ISO 时间 + 正整数 seq）。
    这个模块就是两者之间的唯一转换点：

        motion        -> pirDetected
        no_motion_sec -> lastMotionTs = now - no_motion_sec
        temp / humi   -> temperature / humidity
        door          -> doorOpen
        sos           -> 额外补一条 SOS 事件（L3）
        uptime        -> fwVersion 后缀带上，便于现场确认板子活着

设计约束：
    1. 只在「看起来是遥测帧」时才转换，标准快照不受影响（不会破坏既有 61 项测试）；
    2. 缺少 ts/seq 时用本地时间补，保证入库与去重规则照常工作；
    3. 转换结果 source 固定为 hardware —— 这是真板子来的数据，不能标成 test；
    4. 纯函数、不碰 IO，方便单测。
"""
from __future__ import annotations

from datetime import datetime, timedelta

# 兼容固件里可能出现的几种 type 写法
TELEMETRY_TYPES = ("telemetry", "elderly_care", "care", "ecs")

DEFAULT_DEVICE_ID = "bearpi-001"


def looks_like_telemetry(body):
    """判断一条报文是不是简化遥测帧。"""
    if not isinstance(body, dict):
        return False
    if "deviceId" in body:
        return False          # 已经是标准快照，交给原流程
    kind = str(body.get("type") or "").lower()
    if kind in TELEMETRY_TYPES:
        return True
    # 兜底：即使 type 丢了，只要带这几个独有字段也认
    return ("no_motion_sec" in body) and ("motion" in body)


def _iso(moment):
    return moment.strftime("%Y-%m-%dT%H:%M:%S+08:00")


def _num(value, default=None):
    try:
        if value is None or value == "":
            return default
        return float(value)
    except (TypeError, ValueError):
        return default


def _flag(value):
    """固件用 0/1 表示，也兼容 true/false。"""
    if isinstance(value, bool):
        return value
    try:
        return int(value) != 0
    except (TypeError, ValueError):
        return False


def to_snapshot(body, device_id=None, now=None):
    """遥测帧 -> 标准快照（16 字段）。now 可注入，方便测试。"""
    now = now or datetime.now()
    dev = body.get("deviceId") or device_id or DEFAULT_DEVICE_ID

    no_motion = _num(body.get("no_motion_sec"), 0.0) or 0.0
    last_motion = now - timedelta(seconds=no_motion)

    # seq：固件不带，用「毫秒时间戳」当单调递增序号，天然不重复
    seq = body.get("seq")
    if not isinstance(seq, int) or seq <= 0:
        seq = int(now.timestamp() * 1000)

    uptime = _num(body.get("uptime"), None)
    fw = "bearpi-telemetry"
    if uptime is not None:
        fw = "%s/up%ds" % (fw, int(uptime))

    temp = _num(body.get("temp"))
    humi = _num(body.get("humi"))

    return {
        "deviceId": str(dev),
        "ts": body.get("ts") or _iso(now),
        "seq": int(seq),
        "pirDetected": _flag(body.get("motion")),
        "lastMotionTs": _iso(last_motion),
        "lastMotionZone": body.get("zone") or "unknown",
        "temperature": temp,
        "humidity": humi,
        "lux": _num(body.get("lux")),
        "doorOpen": _flag(body.get("door")),
        "battery": _num(body.get("battery")),
        "powerState": body.get("power") or "mains",
        "rssi": _num(body.get("rssi")),
        "online": True,
        "source": "hardware",
        "fwVersion": fw,
        "timeAdjusted": False,
    }


def to_sos_event(body, device_id=None, now=None):
    """遥测帧里 sos=1 时，额外产出一条 SOS 事件（规则引擎据此开 L3 告警）。"""
    if not _flag(body.get("sos")):
        return None
    now = now or datetime.now()
    dev = body.get("deviceId") or device_id or DEFAULT_DEVICE_ID
    return {
        "type": "SOS",
        "deviceId": str(dev),
        "ts": body.get("ts") or _iso(now),
        "source": "hardware",
        "payload": {"trigger": "board_key", "zone": body.get("zone") or "unknown"},
    }


# 烟雾/燃气：固件把传感器的数字量（1=超限）放进遥测帧，
# 字段名沿用硬件组约定（smoke / gas），也兼容 mq2 / mq5 两种模块叫法。
HAZARD_FIELDS = (
    ("smoke", "SMOKE", "烟雾传感器"),
    ("gas", "GAS", "燃气传感器"),
)


def to_hazard_events(body, device_id=None, now=None):
    """遥测帧里 smoke=1 / gas=1 时，各补一条 GAS/SMOKE 事件（L3 告警）。

    返回列表：可能同时有烟雾和燃气（例如火灾把燃气管烤漏了）。
    与 to_sos_event 一样是纯函数，方便单测。
    """
    if not isinstance(body, dict):
        return []
    now = now or datetime.now()
    dev = body.get("deviceId") or device_id or DEFAULT_DEVICE_ID
    events = []
    for field, etype, label in HAZARD_FIELDS:
        raw = body.get(field)
        if raw is None and field == "smoke":
            raw = body.get("mq2")
        if raw is None and field == "gas":
            raw = body.get("mq5")
        if not _flag(raw):
            continue
        events.append({
            "type": etype,
            "deviceId": str(dev),
            "ts": body.get("ts") or _iso(now),
            "source": "hardware",
            "payload": {
                "trigger": "sensor",
                "sensor": label,
                "zone": body.get("zone") or "unknown",
            },
            "message": "%s检测到超限" % label,
        })
    return events
