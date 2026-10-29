# -*- coding: utf-8 -*-
"""连续性判定自测：网关重启 / 数据断档后不得用旧时间戳直接报“无活动”。

背景（真实故障）：gateway.db 里 device 表保留上一次运行的 last_motion_ts，
网关重启后第一张快照一来，delta 已经是 7 分钟，规则立刻建一条
L2「卫生间连续 7 分钟未检测到活动」，声光播报再把它当新告警响起来。
断档期间的“静默”不是观察到的静默，所以：
    1. 静默计时起点取 max(设备上报起点, 本次连续观察起点)；
    2. 观察起点之后 startup_warmup_sec 内不判时间累积型告警；
    3. 回放 / 补传帧（ts 明显早于服务器时间）照原样判定，
       保证 sim/incident.py --type night 这类历史注入仍然立刻出告警。

    python run_tests.py
"""
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


def now():
    return datetime.now(TZ)


class ContinuityTestCase(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-cont-")
        self.store = Store(os.path.join(self.tmpdir, "cont.db"))
        self.engine = Engine(self.store)
        # 固定成“白天阈值口径”：夜间加权会把 5 分钟阈值降到 2 分钟，
        # 这些用例断言的是白天规则，夜里跑 CI 不能因为系统时间而飘。
        self.store.save_thresholds({"night_start_hour": 99})
        self.seq = 0

    def tearDown(self):
        self.store.close()
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    # ------------------------------------------------------------------
    def seed_device(self, **fields):
        """模拟“上一次运行留在库里”的设备状态。"""
        self.store.upsert_device("dev-cont", **fields)

    def frame(self, ts, last_motion=None, pir=False, zone="bathroom",
              temp=None, hum=None, door=None, device="dev-cont"):
        """构造一张（已归一化的）快照。"""
        self.seq += 1
        return {
            "deviceId": device,
            "ts": iso(ts),
            "seq": self.seq,
            "pirDetected": pir,
            "lastMotionTs": iso(last_motion) if last_motion is not None else None,
            "lastMotionZone": zone,
            "temperature": temp,
            "humidity": hum,
            "lux": None,
            "doorOpen": door,
            "gasLeak": None,
            "smoke": None,
            "batteryLevel": None,
            "powerStatus": None,
            "source": "test",
        }

    def test_51_restart_with_stale_motion_ts_is_quiet(self):
        """重启后第一张实时帧：设备记录里的旧 last_motion 不能直接触发 L2。"""
        ts = now()
        self.seed_device(last_motion_ts=iso(ts - timedelta(minutes=30)),
                         last_zone="bathroom")
        alarms = self.engine.on_snapshot(self.frame(ts))
        self.assertEqual([], alarms)

    def test_52_replay_frame_still_fires(self):
        """回放 / 补传帧（ts 明显早于现在）照旧判定：夜间卫生间 L3。"""
        base = now().replace(hour=2, minute=10, second=0, microsecond=0)
        if base > now():                     # 凌晨跑测试时 base 仍在今天
            base = base - timedelta(days=1)
        self.engine.on_snapshot(
            self.frame(base, last_motion=base, pir=True, zone="bathroom"))
        alarms = self.engine.on_snapshot(
            self.frame(base + timedelta(minutes=3), zone="bathroom"))
        self.assertEqual(1, len(alarms))
        self.assertEqual("NO_MOTION", alarms[0]["type"])
        self.assertEqual("L3", alarms[0]["level"])

    def test_53_warmup_expires_and_alarm_returns(self):
        """预热窗口过后，连续观察到的“长时间无活动”仍然要报。"""
        ts = now()
        self.seed_device(last_motion_ts=iso(ts - timedelta(minutes=30)),
                         last_zone="bathroom")
        # 模拟“已经连续观察了 400 秒”
        self.engine._watch["dev-cont"] = ts - timedelta(seconds=400)
        alarms = self.engine.on_snapshot(self.frame(ts))
        self.assertEqual(1, len(alarms))
        self.assertEqual("NO_MOTION", alarms[0]["type"])
        self.assertEqual("L2", alarms[0]["level"])
        # 消息里的分钟数应按“观察到的静默”算，而不是设备的 30 分钟
        self.assertIn("6 分钟", alarms[0]["message"])

    def test_54_silence_start_is_rebased(self):
        """观察起点晚于设备上报起点时，静默时长从观察起点算。"""
        ts = now()
        self.seed_device(last_motion_ts=iso(ts - timedelta(minutes=30)),
                         last_zone="bathroom")
        self.engine._watch["dev-cont"] = ts - timedelta(seconds=200)
        # 观察只有 200 秒（3.3 分钟）< 白天无活动阈值 5 分钟，不应报
        self.assertEqual([], self.engine.on_snapshot(self.frame(ts)))

    def test_55_door_open_rebased(self):
        """门磁：断档期间累积的“已开启 X 分钟”同样从观察起点算。"""
        ts = now()
        self.seed_device(door_open=1,
                         door_open_since=iso(ts - timedelta(minutes=30)),
                         last_motion_ts=iso(ts - timedelta(minutes=40)))
        self.engine._watch["dev-cont"] = ts - timedelta(seconds=60)
        self.assertEqual([], self.engine.on_snapshot(self.frame(ts, door=True)))

    def test_56_door_open_normal(self):
        """正常连续观察下，门窗长时间开启仍要提醒。"""
        ts = now()
        self.seed_device(door_open=1,
                         door_open_since=iso(ts - timedelta(minutes=30)),
                         last_motion_ts=iso(ts - timedelta(minutes=40)))
        self.engine._watch["dev-cont"] = ts - timedelta(minutes=20)
        alarms = self.engine.on_snapshot(self.frame(ts, door=True))
        types = [a["type"] for a in alarms]
        self.assertIn("DOOR_OPEN", types)


    def test_57_door_open_continuous(self):
        """门磁连续开启超过阈值要提醒（顺带回归 int/bool 状态读取）。"""
        base = now().replace(hour=2, minute=10, second=0, microsecond=0)
        if base > now():
            base = base - timedelta(days=1)
        self.engine.on_snapshot(self.frame(base, last_motion=base, door=True))
        alarms = self.engine.on_snapshot(
            self.frame(base + timedelta(minutes=12), door=True))
        types = [a["type"] for a in alarms]
        self.assertIn("DOOR_OPEN", types)

    def test_58_door_close_sets_away_and_suppresses_no_motion(self):
        """开→关之后按“疑似外出”处理：不再报无活动（外出抑制）。"""
        base = now().replace(hour=2, minute=10, second=0, microsecond=0)
        if base > now():
            base = base - timedelta(days=1)
        self.engine.on_snapshot(self.frame(base, last_motion=base, door=True))
        self.engine.on_snapshot(self.frame(base + timedelta(minutes=2), door=False))
        alarms = self.engine.on_snapshot(
            self.frame(base + timedelta(minutes=15), door=False))
        types = [a["type"] for a in alarms]
        self.assertNotIn("NO_MOTION", types)
        self.assertIsNotNone(self.store.get_device("dev-cont")["away_since"])


if __name__ == "__main__":
    unittest.main()
