# -*- coding: utf-8 -*-
"""回归：告警编号 EVT-YYYYMMDD-NNNN 在删过记录后也不能撞号。

历史缺陷：`_next_event_id` 早先用 `COUNT(*)+1` 取号，
一旦删过告警（清理测试设备、手工清库）或并发插入，就会算出已存在的编号，
插入时报 `UNIQUE constraint failed: alarm.event_id`，
现象是“演示场景注入了，但 APP 里始终没有新告警”。
"""
import os
import shutil
import tempfile
import unittest

from gateway.store import Store
from gateway import util


class EventIdTestCase(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="shadowlamp_evtid_")
        self.db = os.path.join(self.dir, "t.db")
        self.store = Store(self.db)

    def tearDown(self):
        try:
            self.store.close()
        except Exception:
            pass
        shutil.rmtree(self.dir, ignore_errors=True)

    def _make(self, alarm_type="TEMP_WARM"):
        ts = util.iso_now()
        return self.store.create_alarm(alarm_type, "L1", "测试告警", ts,
                                       device_id="dev-a", zone="living")

    def test_1_ids_increase(self):
        a = self._make()
        b = self._make()
        self.assertTrue(a["eventId"] < b["eventId"])

    def test_2_delete_tail_then_create_no_conflict(self):
        a = self._make()
        b = self._make()
        with self.store.lock:
            self.store.conn.execute("DELETE FROM alarm WHERE event_id=?", (b["eventId"],))
            self.store.conn.commit()
        c = self._make()   # 旧实现会在这里撞号
        self.assertNotIn(c["eventId"], (a["eventId"], b["eventId"]))

    def test_3_delete_all_then_create(self):
        a = self._make()
        with self.store.lock:
            self.store.conn.execute("DELETE FROM alarm")
            self.store.conn.commit()
        b = self._make()   # 全删后重新起号，不能再撞历史编号
        self.assertNotEqual(a["eventId"], b["eventId"])


if __name__ == "__main__":
    unittest.main()
