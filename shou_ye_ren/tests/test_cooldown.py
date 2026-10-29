# -*- coding: utf-8 -*-
"""测试环境类与门窗类告警时间阀门冷却去重机制，验证跌倒/SOS等零冷却即时告警。"""
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from gateway import util
from gateway.api import Api
from gateway.rules import Engine
from gateway.store import Store


class TestCooldownAndSuppression(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.db_path = self.tmpdir + "/test.db"
        self.store = Store(self.db_path)
        self.api = Api(self.store)
        self.engine = self.api.engine

    def tearDown(self):
        self.store.close()
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_environment_alarm_cooldown_after_close(self):
        """温湿度等环境类告警：处置关闭后处于冷却窗口内，不立刻重复弹窗报警。"""
        device = "dev-env-01"
        ts1 = util.iso_now()

        # 1. 湿度过低 (15%) -> 触发 HUM_LOW 告警
        snap1 = {
            "deviceId": device,
            "ts": ts1,
            "humidity": 15.0,
            "temperature": 22.0,
            "source": "hardware"
        }
        alarms1 = self.engine.on_snapshot(snap1)
        self.assertEqual(len(alarms1), 1)
        event_id = alarms1[0]["eventId"]
        self.assertEqual(alarms1[0]["type"], "HUM_LOW")

        # 2. 家属在手机端点击“提交处置并关闭”
        self.api.close_alarm({"event_id": event_id}, {
            "handleType": "visited",
            "handleResult": "已开启加湿器"
        })
        closed_alarm = self.store.get_alarm(event_id)
        self.assertEqual(closed_alarm["status"], "closed")

        # 3. 2 秒后硬件再次上报快照，湿度依然低 (15.2%)
        snap2 = {
            "deviceId": device,
            "ts": util.iso_now(),
            "humidity": 15.2,
            "temperature": 22.0,
            "source": "hardware"
        }
        alarms2 = self.engine.on_snapshot(snap2)
        # 冷却时间内应该被抑制！不再产生相同报警
        self.assertEqual(len(alarms2), 0, "处于冷却期内的环境告警不应重复产生新告警")

    def test_emergency_alarm_zero_cooldown(self):
        """跌倒/SOS 等严重安全告警：零冷却间隔，每次触发都必须立即报警建单！"""
        device = "dev-sos-01"
        ts1 = util.iso_now()

        # 1. 发生紧急求助 SOS
        ev1 = self.engine.on_event({
            "deviceId": device,
            "ts": ts1,
            "type": "SOS",
            "source": "hardware"
        })
        self.assertIsNotNone(ev1)
        event_id = ev1["eventId"]

        # 2. 处置关闭
        self.api.close_alarm({"event_id": event_id}, {
            "handleType": "visited",
            "handleResult": "已确认老人安全"
        })

        # 3. 2 秒后老人再次按下求助键
        ev2 = self.engine.on_event({
            "deviceId": device,
            "ts": util.iso_now(),
            "type": "SOS",
            "source": "hardware"
        })
        self.assertIsNotNone(ev2, "SOS 属于生命安全危机告警，零冷却，必须再次立即建单！")
        self.assertNotEqual(ev1["eventId"], ev2["eventId"])


if __name__ == "__main__":
    unittest.main()
