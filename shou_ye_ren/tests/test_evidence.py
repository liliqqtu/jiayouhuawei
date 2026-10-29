# -*- coding: utf-8 -*-
"""告警证据链自测：每条告警都要留下“触发那一刻的读数”。

为什么重要：答辩现场一定会被问“你凭什么说这是火情”。
只有一句 message 是不够的，必须能回答“当时 58℃、湿度 50%、门是关的”。
所以规则建单时把关键读数一起写进 evidence，APP 详情页再还原成证据链。

    python run_tests.py
"""
import json
import os
import shutil
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)

from gateway.rules import Engine      # noqa: E402
from gateway.store import Store       # noqa: E402

TZ = timezone(timedelta(hours=8))


def iso(dt):
    return dt.replace(microsecond=0).isoformat()


class EvidenceTestCase(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="shadow_ev_")
        self.store = Store(os.path.join(self.dir, "ev.db"))
        self.engine = Engine(self.store)

    def tearDown(self):
        self.store.close()
        shutil.rmtree(self.dir, ignore_errors=True)

    def frame(self, ts, **kw):
        base = {
            "deviceId": "dev-ev",
            "ts": iso(ts),
            "seq": kw.pop("seq", 1),
            "pirDetected": False,
            "lastMotionTs": iso(ts - timedelta(minutes=1)),
            "lastMotionZone": "bathroom",
            "temperature": 24.0,
            "humidity": 50.0,
            "lux": 100.0,
            "doorOpen": False,
            "battery": 90,
            "powerState": "mains",
            "rssi": -50,
            "online": True,
            "source": "hardware",
            "fwVersion": "1.0.0",
        }
        base.update(kw)
        return base

    def test_61_evidence_contains_trigger_reading(self):
        """火情告警的 evidence 里必须能读到 58℃ 这个事实。"""
        t0 = datetime.now(TZ)
        self.engine.on_snapshot(self.frame(t0, seq=1, temperature=26.0))
        events = self.engine.on_snapshot(self.frame(t0 + timedelta(seconds=5), seq=2, temperature=58.0))
        self.assertTrue(events, "陡升应当触发 TEMP_FIRE")
        ev = self.store.list_alarms(status=None, limit=1)[0]
        self.assertEqual("TEMP_FIRE", ev["type"])
        self.assertTrue(ev["evidence"], "evidence 不能为空")
        data = json.loads(ev["evidence"])
        self.assertEqual(58.0, data["temperature"])
        self.assertFalse(data["doorOpen"])
        self.assertTrue(data["online"])

    def test_62_evidence_reports_null_instead_of_zero(self):
        """没接传感器的项必须是 null，不能拿 0 冒充读数。"""
        t0 = datetime.now(TZ)
        # 只让温度超阈值，湿度/光照故意不报（模拟“没接这个传感器”）
        self.engine.on_snapshot(self.frame(t0, seq=1, temperature=24.0, humidity=None, lux=None))
        events = self.engine.on_snapshot(
            self.frame(t0 + timedelta(seconds=5), seq=2, temperature=30.0, humidity=None, lux=None))
        self.assertTrue(events, "温度偏高应当触发 TEMP_WARM")
        ev = self.store.list_alarms(status=None, limit=1)[0]
        data = json.loads(ev["evidence"])
        self.assertEqual(30.0, data["temperature"])
        self.assertIsNone(data["humidity"])
        self.assertIsNone(data["lux"])


if __name__ == "__main__":
    unittest.main()
