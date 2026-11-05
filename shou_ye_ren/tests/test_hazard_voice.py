# -*- coding: utf-8 -*-
"""烟雾 / 燃气监测 + 语音模块触发链路的自测（标准库 unittest）。

覆盖：
    1. 语音模块的预设短语（SOS -> “我需要帮助”）与 BY8301 指令帧；
    2. SOS 事件 -> 规则引擎建 L3 -> 语音模块播报这条链路；
    3. 语音开关关闭时不播报；
    4. 烟雾/燃气状态卡片：正常 -> 报警 -> 处置后回正常（并保留上次报警时间）；
    5. 遥测帧里的 smoke/gas 位也能转成 L3 事件。
"""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)

from gateway import telemetry                      # noqa: E402
from gateway.api import Api, ApiError               # noqa: E402
from gateway.store import Store                     # noqa: E402
from gateway.voice_module import (                  # noqa: E402
    VoiceModule, build_by8301_frame, DEFAULT_PHRASES)


class VoiceModuleTestCase(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-voice-")
        self.log_path = os.path.join(self.tmpdir, "voice.log")
        self.module = VoiceModule(backend="log", log_path=self.log_path)

    def tearDown(self):
        self.module.close()

    def test_sos_phrase_is_help_me(self):
        """求助按钮的预设语音必须就是“我需要帮助”。"""
        self.assertEqual(DEFAULT_PHRASES["SOS"], "我需要帮助")
        record = self.module.play("SOS")
        self.assertTrue(record["ok"])
        self.assertEqual(record["text"], "我需要帮助")
        self.assertEqual(record["track"], 1)
        self.assertEqual(self.module.plays, 1)

    def test_log_backend_writes_file(self):
        self.module.play("GAS")
        self.assertTrue(os.path.exists(self.log_path))
        with open(self.log_path, encoding="utf-8") as handle:
            content = handle.read()
        self.assertIn("GAS", content)
        self.assertIn("燃气", content)

    def test_by8301_frame(self):
        frame = build_by8301_frame(1)
        self.assertEqual(len(frame), 9)
        self.assertEqual(frame[0], 0x7E)
        self.assertEqual(frame[-1], 0xEF)
        self.assertEqual(frame[6], 1)          # 曲目号
        total = sum(frame[1:7]) & 0xFF
        self.assertEqual((total + frame[7]) & 0xFF, 0x00)   # 校验和补齐为 0

    def test_unknown_key_falls_back_to_message(self):
        record = self.module.play("UNKNOWN", "自定义提示")
        self.assertEqual(record["text"], "自定义提示")
        self.assertTrue(record["ok"])


class HazardApiTestCase(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-hazard-")
        self.store = Store(os.path.join(self.tmpdir, "gateway.db"))
        self.api = Api(self.store)

    def tearDown(self):
        self.store.close()

    def _post(self, body):
        return self.api.post_event({}, dict(body))

    def _sensors(self, device_id="dev1"):
        return self.api.get_sensors({"deviceId": device_id}, {})

    def _item(self, payload, kind):
        for item in payload["items"]:
            if item["kind"] == kind:
                return item
        raise AssertionError("缺少传感器状态：%s" % kind)

    def test_sos_triggers_voice_help_me(self):
        log_path = os.path.join(self.tmpdir, "voice.log")
        module = VoiceModule(backend="log", log_path=log_path)
        self.api.voice = True
        self.api.voice_module = module
        result = self._post({"type": "SOS", "deviceId": "dev1",
                             "source": "key_sim", "payload": {"zone": "living"}})
        self.assertIsNotNone(result["created"])
        self.assertEqual(result["created"]["level"], "L3")
        self.assertEqual(module.plays, 1)
        self.assertEqual(module.last["text"], "我需要帮助")

    def test_voice_off_does_not_play(self):
        module = VoiceModule(backend="log", log_path=os.path.join(self.tmpdir, "v.log"))
        self.api.voice = False
        self.api.voice_module = module
        self._post({"type": "SOS", "deviceId": "dev1", "source": "key_sim"})
        self.assertEqual(module.plays, 0)

    def test_sensor_status_lifecycle(self):
        normal = self._sensors()
        self.assertEqual(normal["alarming"], 0)
        self.assertEqual(self._item(normal, "gas")["status"], "normal")
        self.assertEqual(self._item(normal, "smoke")["status"], "normal")

        created = self._post({"type": "GAS", "deviceId": "dev1",
                              "source": "key_sim",
                              "payload": {"zone": "kitchen", "sensor": "MQ-5"}})["created"]
        self.assertEqual(created["level"], "L3")

        alarming = self._sensors()
        self.assertEqual(alarming["alarming"], 1)
        gas = self._item(alarming, "gas")
        self.assertEqual(gas["status"], "alarm")
        self.assertEqual(gas["eventId"], created["eventId"])
        self.assertEqual(self._item(alarming, "smoke")["status"], "normal")

        self.api.close_alarm({"event_id": created["eventId"]},
                             {"handleType": "other", "handleResult": "已开窗通风"})
        after = self._sensors()
        gas_after = self._item(after, "gas")
        self.assertEqual(gas_after["status"], "normal")
        self.assertEqual(gas_after["lastStatus"], "closed")
        self.assertEqual(gas_after["lastAlarmTs"], created["ts"])

    def test_smoke_event_is_l3(self):
        created = self._post({"type": "SMOKE", "deviceId": "dev1",
                              "source": "key_sim",
                              "payload": {"zone": "living", "sensor": "MQ-2"}})["created"]
        self.assertEqual(created["level"], "L3")
        self.assertIn("烟雾", created["message"])

    def test_voice_say_endpoint(self):
        self.api.voice_module = VoiceModule(
            backend="log", log_path=os.path.join(self.tmpdir, "say.log"))
        played = self.api.voice_say({}, {"key": "SOS"})
        self.assertTrue(played["ok"])
        self.assertEqual(played["played"]["text"], "我需要帮助")
        with self.assertRaises(ApiError):
            self.api.voice_say({}, {})

    def test_voice_say_requires_module(self):
        with self.assertRaises(ApiError) as ctx:
            self.api.voice_say({}, {"key": "SOS"})
        self.assertEqual(ctx.exception.code, "VOICE_DISABLED")

    def test_telemetry_hazard_events(self):
        events = telemetry.to_hazard_events({"smoke": 1, "gas": 0})
        self.assertEqual([item["type"] for item in events], ["SMOKE"])
        events = telemetry.to_hazard_events({"mq5": 1})
        self.assertEqual([item["type"] for item in events], ["GAS"])
        self.assertEqual(telemetry.to_hazard_events({"smoke": 0, "gas": 0}), [])


if __name__ == "__main__":
    unittest.main()
