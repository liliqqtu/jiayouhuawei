# -*- coding: utf-8 -*-
"""烟雾 / 燃气两类安全传感器的状态视图。

背景（2026-11-05）：
    比赛赛题四的"创新拓展"第 1 条是多维度监测：在红外/门磁/温湿度之外，
    再加烟雾传感器（看火灾）和燃气传感器（看燃气泄漏）。规则引擎里
    GAS / SMOKE 已经是直接事件类型（建 L3 告警），缺的是"现在到底报没报警"
    这个持续状态：APP 首页要能像门磁一样显示"正常 / 报警"。

设计：
    - 不新增表：状态从告警表反推。"该类型最近一条告警还在 pending" = 报警中；
      否则 = 正常，并带上最近一次的告警时间，方便看"上次报警是什么时候"。
    - 好处是关了告警（APP 处置完 / 板端自动恢复）状态立即回正常，
      不会出现"告警关了但卡片还红着"的两套真相。
"""
from __future__ import annotations

# (kind, 中文名, 事件类型, 说明)
SENSOR_SPECS = (
    ("gas", "燃气", "GAS", "燃气泄漏监测"),
    ("smoke", "烟雾", "SMOKE", "火灾烟雾监测"),
)


def _sensor_item(store, device_id, kind, name, etype, note):
    alarm = store.latest_alarm_by_type(etype, device_id)
    alarming = bool(alarm) and alarm.get("status") == "pending"
    item = {
        "kind": kind,
        "name": name,
        "type": etype,
        "note": note,
        "status": "alarm" if alarming else "normal",
        "statusText": "报警" if alarming else "正常",
        "level": alarm.get("level") if alarming else None,
        "message": alarm.get("message") if alarming else None,
        "ts": alarm.get("ts") if alarm else None,
        "lastAlarmTs": alarm.get("ts") if alarm else None,
        "lastStatus": alarm.get("status") if alarm else None,
        "eventId": alarm.get("eventId") if alarming else None,
        "zone": alarm.get("zone") if alarm else None,
        "deviceId": (alarm.get("deviceId") if alarm else None) or device_id,
    }
    return item


def sensor_states(store, device_id=None):
    """返回烟雾/燃气当前状态：{"deviceId":..., "items":[...], "alarming": n}。

    device_id 为空时先按"最近上报过数据的设备"兜底，避免接口返回 deviceId=null。
    """
    if not device_id:
        # latest_snapshot 返回 (json, row)；取不到数据时按全设备看
        latest, _row = store.latest_snapshot()
        device_id = latest.get("deviceId") if latest else None
    items = [_sensor_item(store, device_id, kind, name, etype, note)
             for kind, name, etype, note in SENSOR_SPECS]
    alarming = len([item for item in items if item["status"] == "alarm"])
    return {
        "deviceId": device_id,
        "items": items,
        "alarming": alarming,
        "supportedTypes": [etype for _, _, etype, _ in SENSOR_SPECS],
    }
