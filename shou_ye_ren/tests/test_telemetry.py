# -*- coding: utf-8 -*-
"""板端串口遥测适配层的单元测试。

固件（elderly_care_system）每秒从串口打印的帧格式与网关契约不同，
gateway/telemetry.py 负责翻译。这里覆盖：
    * 字段映射正确（motion/temp/humi/door/no_motion_sec）
    * 缺 ts/seq 时自动补齐，且 seq 单调可用
    * sos=1 时额外产出 SOS 事件（L3）
    * 标准快照不会被误判成遥测帧
"""
from __future__ import annotations

import os
import sys
import unittest
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from gateway import telemetry  # noqa: E402
from gateway.api import Api  # noqa: E402

# 与固件 elderly_care_main.c 里 printf 的字段完全一致
BOARD_LINE = {
    "type": "telemetry", "uptime": 12, "motion": 1, "no_motion_sec": 0,
    "door": 0, "door_sec": 0, "temp": 24.5, "humi": 53.0,
    "sos": 0, "alert": 0, "level": 0, "count": 3,
}


class TelemetryShapeTest(unittest.TestCase):
    def test_01_识别遥测帧(self):
        self.assertTrue(telemetry.looks_like_telemetry(BOARD_LINE))

    def test_02_不误判标准快照(self):
        snap = {"deviceId": "bearpi-001", "ts": "2026-10-29T10:00:00+08:00", "seq": 1}
        self.assertFalse(telemetry.looks_like_telemetry(snap))

    def test_03_字段映射(self):
        now = datetime(2026, 10, 29, 10, 0, 0)
        snap = telemetry.to_snapshot(BOARD_LINE, now=now)
        self.assertEqual(snap["deviceId"], "bearpi-001")
        self.assertTrue(snap["pirDetected"])
        self.assertEqual(snap["temperature"], 24.5)
        self.assertEqual(snap["humidity"], 53.0)
        self.assertFalse(snap["doorOpen"])
        self.assertEqual(snap["source"], "hardware")   # 真板子来的，不能标 test
        self.assertTrue(snap["seq"] > 0)
        self.assertTrue(snap["ts"].endswith("+08:00"))

    def test_04_静默时长换算成最后活动时间(self):
        now = datetime(2026, 10, 29, 10, 0, 0)
        frame = dict(BOARD_LINE, no_motion_sec=180)
        snap = telemetry.to_snapshot(frame, now=now)
        self.assertEqual(snap["lastMotionTs"], "2026-10-29T09:57:00+08:00")

    def test_05_SOS位映射成事件(self):
        now = datetime(2026, 10, 29, 10, 0, 0)
        ev = telemetry.to_sos_event(dict(BOARD_LINE, sos=1), now=now)
        self.assertIsNotNone(ev)
        self.assertEqual(ev["type"], "SOS")
        self.assertEqual(ev["source"], "hardware")

    def test_06_没有SOS就不产事件(self):
        self.assertIsNone(telemetry.to_sos_event(BOARD_LINE))


class TelemetryIngestTest(unittest.TestCase):
    """走真实 Api.ingest_snapshot，确认端到端能入库并触发告警。"""

    def setUp(self):
        import tempfile
        from gateway.store import Store
        self.tmp = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.tmp.close()
        self.store = Store(self.tmp.name)
        self.api = Api(self.store)
        self.api.log = lambda *a, **k: None
        self.api.alert = lambda alarms: None

    def tearDown(self):
        self.store.close()
        os.unlink(self.tmp.name)

    def test_11_遥测帧能入库(self):
        res = self.api.ingest_snapshot(dict(BOARD_LINE), channel="serial")
        self.assertTrue(res["ok"])
        self.assertEqual(res["channel"], "serial")
        self.assertEqual(res["source"], "hardware")
        latest, _raw = self.store.latest_snapshot("bearpi-001")
        self.assertIsNotNone(latest)
        self.assertAlmostEqual(latest["temperature"], 24.5, places=3)

    def test_12_高温遥测触发L1(self):
        frame = dict(BOARD_LINE, temp=36.0, motion=0, no_motion_sec=10)
        res = self.api.ingest_snapshot(frame, channel="serial")
        self.assertTrue(res["alarms"], "36℃ 应触发出环境提醒")

    def test_13_SOS位触发L3告警(self):
        frame = dict(BOARD_LINE, sos=1)
        res = self.api.ingest_snapshot(frame, channel="serial")
        self.assertTrue(res["alarms"])
        alarms = self.store.list_alarms(status="pending", limit=5)
        self.assertTrue(any(a["type"] == "SOS" and a["level"] == "L3" for a in alarms))

    def test_14_毫秒时间戳当seq不冲突(self):
        for _ in range(3):
            self.api.ingest_snapshot(dict(BOARD_LINE), channel="serial")
        rows = self.store.list_snapshots(limit=10, device_id="bearpi-001")
        seqs = [r["seq"] for r in rows if "seq" in r]
        self.assertTrue(seqs, "快照列表里应有 seq 字段")
        self.assertEqual(len(seqs), len(set(seqs)))


if __name__ == "__main__":
    unittest.main()
