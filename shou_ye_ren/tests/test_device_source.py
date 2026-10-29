# -*- coding: utf-8 -*-
"""设备来源标签自测：/api/v1/devices 必须能区分“真实设备”和“模拟演示”。

为什么这条要单独测：赛题和内部规范都要求 source 只能取 hardware/test，
界面上的“测试/模拟”标签必须来自网关真实字段，不能由 APP 猜。
设备列表是家属切设备的第一入口，标签错了就等于把模拟数据当真实采集。

    python run_tests.py
"""
import json
import os
import shutil
import sys
import tempfile
import threading
import unittest
from datetime import datetime, timedelta, timezone
from http.server import ThreadingHTTPServer
from urllib import request

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)

from gateway.api import Api        # noqa: E402
from gateway.server import Handler  # noqa: E402
from gateway.store import Store     # noqa: E402

TZ = timezone(timedelta(hours=8))


class DeviceSourceTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-devsrc-")
        cls.store = Store(os.path.join(cls.tmpdir, "dev.db"))
        cls.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.httpd.api = Api(cls.store)
        cls.httpd.daemon_threads = True
        cls.port = cls.httpd.server_address[1]
        cls.thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.store.close()
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def setUp(self):
        self.base = "http://127.0.0.1:%d" % self.port

    def post(self, path, body):
        req = request.Request(self.base + path, data=json.dumps(body).encode("utf-8"),
                              headers={"Content-Type": "application/json"}, method="POST")
        with request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def get(self, path):
        with request.urlopen(self.base + path, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def frame(self, device_id, seq, source):
        now = datetime.now(TZ).replace(microsecond=0)
        return {
            "deviceId": device_id,
            "seq": seq,
            "ts": now.isoformat(),
            "pir": 1,
            "lastMotionTs": now.isoformat(),
            "temperature": 24.0,
            "humidity": 50.0,
            "lux": 120,
            "doorOpen": False,
            "battery": 92,
            "powerState": "mains",
            "rssi": -55,
            "online": True,
            "source": source,
            "fwVersion": "sim-1.0",
        }

    def find(self, items, device_id):
        for item in items:
            if item["deviceId"] == device_id:
                return item
        return None

    def test_01_hardware_device_labelled(self):
        """真实设备上报后，设备列表里的 source 必须是 hardware。"""
        self.post("/api/v1/snapshot", self.frame("dev-hw", 1, "hardware"))
        items = self.get("/api/v1/devices")["items"]
        one = self.find(items, "dev-hw")
        self.assertIsNotNone(one)
        self.assertEqual("hardware", one["source"])

    def test_02_test_device_labelled(self):
        """模拟设备上报后必须是 test，不能被当成真实数据。"""
        self.post("/api/v1/snapshot", self.frame("dev-sim", 1, "test"))
        items = self.get("/api/v1/devices")["items"]
        one = self.find(items, "dev-sim")
        self.assertEqual("test", one["source"])

    def test_03_source_follows_latest_frame(self):
        """来源跟着最新一帧走：模拟设备换成真实上报后要变 hardware。"""
        self.post("/api/v1/snapshot", self.frame("dev-mix", 1, "test"))
        self.post("/api/v1/snapshot", self.frame("dev-mix", 2, "hardware"))
        one = self.find(self.get("/api/v1/devices")["items"], "dev-mix")
        self.assertEqual("hardware", one["source"])

    def test_04_manual_registration_has_no_source(self):
        """只登记没上报的占位设备没有数据来源，APP 应显示“未上报”，不能编。"""
        self.post("/api/v1/devices", {"deviceId": "bench-99"})
        one = self.find(self.get("/api/v1/devices")["items"], "bench-99")
        self.assertIsNotNone(one)
        self.assertIsNone(one["source"])
        self.assertFalse(one["online"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
