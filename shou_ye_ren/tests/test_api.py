# -*- coding: utf-8 -*-
"""接口与状态机自测（标准库 unittest，不依赖 pytest）。

    python run_tests.py
    python run_tests.py -v
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
from urllib import error, request

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)

from gateway.api import Api            # noqa: E402
from gateway.server import Handler      # noqa: E402
from gateway.store import Store         # noqa: E402

TZ = timezone(timedelta(hours=8))


def today_at(hour, minute=0):
    now = datetime.now(TZ)
    return now.replace(hour=hour, minute=minute, second=0, microsecond=0)


def iso(dt):
    return dt.replace(microsecond=0).isoformat()


class ApiTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-test-")
        cls.store = Store(os.path.join(cls.tmpdir, "test.db"))
        cls.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.httpd.api = Api(cls.store)
        cls.httpd.daemon_threads = True
        cls.port = cls.httpd.server_address[1]
        cls.thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = "http://127.0.0.1:%d" % cls.port
        cls.seq = 0

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.store.close()
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    # ------------------------------------------------------------------
    def call(self, method, path, body=None):
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = request.Request(self.base + path, data=data, method=method,
                              headers={"Content-Type": "application/json"})
        try:
            with request.urlopen(req, timeout=5) as resp:
                return resp.status, json.loads(resp.read().decode("utf-8") or "{}")
        except error.HTTPError as exc:
            return exc.code, json.loads(exc.read().decode("utf-8") or "{}")

    def push(self, device, ts, pir=False, zone=None, temp=None, hum=None,
             door=None, seq=None):
        ApiTestCase.seq += 1
        payload = {
            "deviceId": device,
            "ts": iso(ts),
            "seq": seq if seq is not None else ApiTestCase.seq,
            "pirDetected": pir,
            "lastMotionTs": iso(ts) if pir else None,
            "lastMotionZone": zone,
            "temperature": temp,
            "humidity": hum,
            "doorOpen": door,
            "battery": 97.0,
            "powerState": "mains",
            "rssi": -50,
            "online": True,
            "source": "test",
            "fwVersion": "test-1.0",
        }
        return self.call("POST", "/api/v1/snapshot", payload)

    # ------------------------------------------------------------- basic
    def test_01_health(self):
        status, body = self.call("GET", "/health")
        self.assertEqual(status, 200)
        self.assertTrue(body["ok"])
        self.assertIn("uptime", body)

    def test_02_unknown_route(self):
        status, body = self.call("GET", "/api/v1/nothing")
        self.assertEqual(status, 404)
        self.assertEqual(body["code"], "NOT_FOUND")

    def test_03_snapshot_requires_seq(self):
        status, body = self.call("POST", "/api/v1/snapshot",
                                 {"deviceId": "d-x", "ts": iso(today_at(12)), "seq": 0})
        self.assertEqual(status, 400)
        self.assertEqual(body["code"], "BAD_SEQ")

    def test_04_snapshot_requires_iso_time(self):
        status, body = self.call("POST", "/api/v1/snapshot",
                                 {"deviceId": "d-x", "ts": "2026/11/02 12:00", "seq": 5})
        self.assertEqual(status, 400)
        self.assertEqual(body["code"], "BAD_TIME")

    # ---------------------------------------------------------- 告警判定
    def test_05_no_motion_day_l2(self):
        device = "dev-day"
        self.call("POST", "/api/v1/config/threshold", {"no_motion_min": 1})
        base = today_at(12, 0)
        status, _ = self.push(device, base, pir=True, zone="living", temp=23.0, hum=50, door=False)
        self.assertEqual(status, 200)

        status, body = self.push(device, base + timedelta(minutes=2), pir=False,
                                 temp=23.0, hum=50, door=False)
        self.assertEqual(status, 200)
        self.assertEqual(len(body["alarms"]), 1, "应产生一条无活动告警")

        status, alarms = self.call("GET", "/api/v1/alarms?status=pending")
        target = [a for a in alarms["items"] if a["deviceId"] == device]
        self.assertEqual(len(target), 1)
        self.assertEqual(target[0]["type"], "NO_MOTION")
        self.assertEqual(target[0]["level"], "L2")

    def test_06_night_bathroom_l3(self):
        device = "dev-night"
        self.call("POST", "/api/v1/config/threshold", {"night_bathroom_min": 2, "night_no_motion_min": 3})
        base = today_at(2, 10)
        self.push(device, base, pir=True, zone="bathroom", temp=22.0, hum=55, door=False)
        status, body = self.push(device, base + timedelta(minutes=3), pir=False,
                                 temp=22.0, hum=55, door=False)
        self.assertEqual(status, 200)
        self.assertEqual(len(body["alarms"]), 1)

        _, alarms = self.call("GET", "/api/v1/alarms?status=pending")
        target = [a for a in alarms["items"] if a["deviceId"] == device]
        self.assertEqual(target[0]["level"], "L3")
        self.assertEqual(target[0]["type"], "NO_MOTION")

    def test_07_dedupe_same_type(self):
        device = "dev-dup"
        base = today_at(9, 0)
        self.push(device, base, pir=True, zone="living", door=False)
        _, first = self.push(device, base + timedelta(minutes=2), pir=False, door=False)
        _, second = self.push(device, base + timedelta(minutes=4), pir=False, door=False)
        self.assertEqual(len(first["alarms"]), 1)
        self.assertEqual(len(second["alarms"]), 0, "同类型告警活动期内不应重复建单")

    def test_08_away_suppresses_no_motion(self):
        device = "dev-away"
        base = today_at(10, 0)
        self.push(device, base, pir=True, zone="living", door=False)
        self.push(device, base + timedelta(minutes=1), pir=False, zone="entry", door=True)
        self.push(device, base + timedelta(minutes=2), pir=False, zone="entry", door=False)
        status, body = self.push(device, base + timedelta(minutes=30), pir=False, door=False)
        self.assertEqual(status, 200)
        self.assertEqual(len(body["alarms"]), 0, "外出模式应抑制无活动告警")

    def test_09_env_threshold_l1(self):
        device = "dev-env"
        base = today_at(15, 0)
        status, body = self.push(device, base, pir=False, temp=35.0, hum=50, door=False)
        self.assertEqual(status, 200)
        _, alarms = self.call("GET", "/api/v1/alarms?status=pending")
        target = [a for a in alarms["items"] if a["deviceId"] == device]
        self.assertEqual(target[0]["type"], "TEMP_HIGH")
        # 分级调整后：>=35℃ 为“过高”(L2)，28~35℃ 才是“偏热”(L1)
        self.assertEqual(target[0]["level"], "L2")

    def test_10_sos_event_l3(self):
        status, body = self.call("POST", "/api/v1/events",
                                 {"deviceId": "dev-sos", "ts": iso(today_at(16, 0)),
                                  "type": "SOS", "source": "test"})
        self.assertEqual(status, 200)
        self.assertEqual(body["created"]["level"], "L3")
        self.assertEqual(body["created"]["type"], "SOS")

    # ------------------------------------------------------------ 闭环
    def test_11_ack_and_close_flow(self):
        device = "dev-flow"
        base = today_at(10, 0)
        self.push(device, base, pir=True, zone="living", door=False)
        self.push(device, base + timedelta(minutes=2), pir=False, door=False)

        _, alarms = self.call("GET", "/api/v1/alarms?status=pending")
        event_id = [a for a in alarms["items"] if a["deviceId"] == device][0]["eventId"]

        status, body = self.call("POST", "/api/v1/alarms/%s/ack" % event_id, {"operator": "daughter"})
        self.assertEqual(status, 200)
        self.assertEqual(body["status"], "acknowledged")
        self.assertIsNotNone(body["ackTs"])

        # 重复确认 -> 409
        status, body = self.call("POST", "/api/v1/alarms/%s/ack" % event_id, {})
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "INVALID_STATE")

        # 非法处置类型 -> 400
        status, body = self.call("POST", "/api/v1/alarms/%s/close" % event_id,
                                 {"handleType": "whatever", "handleResult": "x"})
        self.assertEqual(status, 400)

        # 缺处置结果 -> 400
        status, body = self.call("POST", "/api/v1/alarms/%s/close" % event_id,
                                 {"handleType": "visited"})
        self.assertEqual(status, 400)

        # 正常关闭
        status, body = self.call("POST", "/api/v1/alarms/%s/close" % event_id,
                                 {"handleType": "visited", "handleResult": "已上门查看，老人安好"})
        self.assertEqual(status, 200)
        self.assertEqual(body["status"], "closed")
        self.assertEqual(body["handleType"], "visited")
        self.assertEqual(body["handleResult"], "已上门查看，老人安好", "中文处置结果必须原样往返")

        # 重复关闭 -> 409
        status, body = self.call("POST", "/api/v1/alarms/%s/close" % event_id,
                                 {"handleType": "visited", "handleResult": "重复"})
        self.assertEqual(status, 409)

    def test_12_false_alarm_close(self):
        device = "dev-false"
        base = today_at(10, 0)
        self.push(device, base, pir=True, zone="living", door=False)
        self.push(device, base + timedelta(minutes=2), pir=False, door=False)
        _, alarms = self.call("GET", "/api/v1/alarms?status=pending")
        event_id = [a for a in alarms["items"] if a["deviceId"] == device][0]["eventId"]

        status, body = self.call("POST", "/api/v1/alarms/%s/close" % event_id,
                                 {"handleType": "false_alarm", "handleResult": "老人一切正常，误报"})
        self.assertEqual(status, 200)
        self.assertEqual(body["status"], "closed")

    def test_13_ack_missing_alarm(self):
        status, body = self.call("POST", "/api/v1/alarms/EVT-NOPE/ack", {})
        self.assertEqual(status, 404)

    # ------------------------------------------------------------ 报表
    def test_14_daily_report(self):
        day = today_at(12).date().isoformat()
        status, body = self.call("GET", "/api/v1/reports/daily?date=%s" % day)
        self.assertEqual(status, 200)
        self.assertEqual(body["date"], day)
        self.assertEqual(len(body["hourly"]), 24)
        self.assertGreaterEqual(body["totalCount"], 1)
        self.assertIn(body["level"], ("normal", "low", "high"))

    def test_15_daily_report_bad_date(self):
        status, body = self.call("GET", "/api/v1/reports/daily?date=2026-13-40")
        self.assertEqual(status, 400)

    def test_16_weekly_report(self):
        status, body = self.call("GET", "/api/v1/reports/weekly")
        self.assertEqual(status, 200)
        self.assertEqual(len(body["dailyCounts"]), 7)
        self.assertIn("deltaVsLastWeek", body)
        self.assertIn("dailyAvg", body)

    # ------------------------------------------------------------ 配置
    def test_17_threshold_update(self):
        status, body = self.call("POST", "/api/v1/config/threshold",
                                 {"no_motion_min": 3, "temp_high": 31.5})
        self.assertEqual(status, 200)
        self.assertTrue(body["ok"])
        status, body = self.call("GET", "/api/v1/config/threshold")
        self.assertEqual(body["no_motion_min"], 3)
        self.assertEqual(body["temp_high"], 31.5)

    def test_18_threshold_rejects_bad_value(self):
        status, body = self.call("POST", "/api/v1/config/threshold", {"no_motion_min": 0})
        self.assertEqual(status, 400)

    # ------------------------------------------------------------ 设备
    def test_19_device_status(self):
        status, body = self.call("GET", "/api/v1/device")
        self.assertEqual(status, 200)
        self.assertIn("online", body)
        self.assertIn("powerState", body)

    def test_20_latest_snapshot(self):
        status, body = self.call("GET", "/api/v1/snapshot")
        self.assertEqual(status, 200)
        self.assertIn("seq", body)
        self.assertIn("pirDetected", body)


    # ------------------------------------------------ 硬件接入相关回归
    def test_21_two_devices_same_seq(self):
        """两台设备各自从 seq=1 开始，不能互相覆盖（旧设计是全局主键）。"""
        base = today_at(8, 30)
        self.push("dev-a", base, pir=True, zone="living", door=False, seq=1)
        self.push("dev-b", base, pir=True, zone="bedroom", door=False, seq=1)
        _, body = self.call("GET", "/api/v1/snapshots?limit=100")
        rows = [r for r in body["items"] if r["deviceId"] in ("dev-a", "dev-b")]
        self.assertEqual(len(rows), 2, "两台设备的 seq=1 必须各存一条")
        self.assertEqual({r["deviceId"] for r in rows}, {"dev-a", "dev-b"})

    def test_22_reboot_seq_reset_keeps_history(self):
        """设备重启后 seq 归零，网关应自动重编号，不覆盖上一轮记录。"""
        device = "dev-reboot"
        base = today_at(7, 0)
        self.push(device, base, pir=True, zone="living", door=False, seq=1)
        self.push(device, base + timedelta(minutes=1), pir=True, zone="living", door=False, seq=2)

        # 把已存记录的接收时间改成 10 分钟前，模拟“上一轮已经过去很久”
        with self.store.lock:
            self.store.conn.execute(
                "UPDATE snapshot SET received_at=? WHERE device_id=?", 
                ("2026-01-01T00:00:00+08:00", device))
            self.store.conn.commit()

        # 重启后重新从 seq=1 开始
        self.push(device, base + timedelta(minutes=5), pir=True, zone="kitchen", door=False, seq=1)

        with self.store.lock:
            count = self.store.conn.execute(
                "SELECT COUNT(*) AS c FROM snapshot WHERE device_id=?", (device,)).fetchone()["c"]
        self.assertEqual(count, 3, "重启后的 seq=1 不应覆盖历史，总行数应为 3")

    def test_23_future_timestamp_is_corrected(self):
        """设备时钟跑到未来 -> 用服务器时间纠偏并打标。"""
        device = "dev-future"
        future = datetime.now(TZ) + timedelta(hours=5)
        status, body = self.push(device, future, pir=True, zone="living", door=False)
        self.assertEqual(status, 200)
        self.assertTrue(body.get("timeAdjusted"), "未来时间应被标记为已纠偏")

        _, snap = self.call("GET", "/api/v1/snapshot")
        self.assertLessEqual(snap["ts"], iso(datetime.now(TZ) + timedelta(minutes=1)))

    def test_24_past_timestamp_is_kept(self):
        """补传/历史数据是过去时间，必须原样保留，不能被改写。"""
        device = "dev-backfill"
        past = datetime.now(TZ) - timedelta(hours=3)
        status, body = self.push(device, past, pir=True, zone="living", door=False)
        self.assertEqual(status, 200)
        self.assertFalse(body.get("timeAdjusted"), "过去的合法时间不应被纠偏")


    # ------------------------------------------------------- Web 看板
    def test_25_dashboard_page(self):
        """根路径返回看板 HTML：评委不装 APP 也能在浏览器里看状态。"""
        url = self.base + "/"
        with request.urlopen(url, timeout=5) as resp:
            self.assertEqual(200, resp.status)
            self.assertIn("text/html", resp.headers.get("Content-Type", ""))
            html = resp.read().decode("utf-8")
        self.assertIn("守夜灯", html)
        self.assertIn("/api/v1/snapshot", html)
        self.assertIn("setInterval", html, "看板必须自动刷新")

    def test_26_dashboard_alias(self):
        """三个入口等价：/ 与 /dashboard 都该出页面。"""
        for path in ("/dashboard", "/index.html"):
            with request.urlopen(self.base + path, timeout=5) as resp:
                self.assertEqual(200, resp.status)

    # --------------------------------------------- 多设备（v1.1 新增）
    def test_27_devices_list_and_snapshot_filter(self):
        """多设备：/api/v1/devices 出清单；snapshot 带 deviceId 只看这台，不带回最近一台。"""
        self.push("dev-m1", today_at(9, 0), pir=True, temp=24.0)
        self.push("dev-m2", today_at(9, 1), pir=True, temp=33.0)

        status, body = self.call("GET", "/api/v1/devices")
        self.assertEqual(200, status)
        self.assertGreaterEqual(body["count"], 2)
        ids = [d["deviceId"] for d in body["items"]]
        self.assertIn("dev-m1", ids)
        self.assertIn("dev-m2", ids)
        self.assertIsInstance(body["items"][0]["online"], bool)
        self.assertIn("staleSeconds", body["items"][0])

        status, one = self.call("GET", "/api/v1/snapshot?deviceId=dev-m1")
        self.assertEqual(200, status)
        self.assertEqual("dev-m1", one["deviceId"])
        self.assertAlmostEqual(24.0, one["temperature"])

        status, latest = self.call("GET", "/api/v1/snapshot")
        self.assertEqual("dev-m2", latest["deviceId"], "不带 deviceId 时回最近上报的那台")

    def test_28_unknown_device_snapshot_is_404(self):
        """查一个不存在的设备必须 404，不能静默把别的设备数据串过去。"""
        status, body = self.call("GET", "/api/v1/snapshot?deviceId=nobody-here")
        self.assertEqual(404, status)
        self.assertEqual("NO_DATA", body["code"])


    def test_30_register_device_before_any_data(self):
        """手工登记的设备：列表里可见、online=false、没有编造指标。"""
        status, data = self.call("POST", "/api/v1/devices", {"deviceId": "bench-09"})
        self.assertEqual(status, 200, data)
        self.assertFalse(data["online"])
        status, data = self.call("GET", "/api/v1/devices")
        ids = [d["deviceId"] for d in data["items"]]
        self.assertIn("bench-09", ids)

    def test_31_register_device_rejects_bad_id(self):
        """设备号只允许字母数字和 -_. ，挡住把中文或路径塞进来。"""
        for bad in ["", "设备一", "../etc/passwd"]:
            status, _data = self.call("POST", "/api/v1/devices", {"deviceId": bad})
            self.assertEqual(status, 400, bad)

    def test_32_family_create_and_join(self):
        """创建家庭组 -> 户主 1 人、6 位密码；用密码加入 -> 监护人 1 人。"""
        status, fam = self.call("POST", "/api/v1/family/create",
                                {"familyName": "张阿姨家", "ownerName": "大女儿"})
        self.assertEqual(status, 200, fam)
        self.assertEqual(len(fam["inviteCode"]), 6)
        self.assertTrue(fam["inviteCode"].isdigit())
        self.assertEqual(fam["members"][0]["roleText"], "户主")

        status, joined = self.call("POST", "/api/v1/family/join",
                                   {"inviteCode": fam["inviteCode"], "memberName": "网格员小李"})
        self.assertEqual(status, 200, joined)
        self.assertEqual(joined["memberCount"], 2)
        self.assertEqual(joined["members"][1]["roleText"], "监护人")

        status, again = self.call("POST", "/api/v1/family/join",
                                  {"inviteCode": fam["inviteCode"], "memberName": "网格员小李"})
        self.assertEqual(status, 200, again)
        self.assertEqual(again["memberCount"], 2, "同一个人重复加入不应重复占位")

    def test_33_family_join_rejects_wrong_code(self):
        """错密码：404，且提示语不区分“不存在/过期”，不给爆破线索。"""
        status, data = self.call("POST", "/api/v1/family/join",
                                 {"inviteCode": "000000", "memberName": "路人"})
        self.assertEqual(status, 404, data)
        status, data = self.call("POST", "/api/v1/family/join",
                                 {"inviteCode": "12ab", "memberName": "路人"})
        self.assertEqual(status, 400, data)

    def test_34_family_lookup_does_not_create_member(self):
        """预检密码只回组名，不加成员；按 familyId 能查到完整成员表。"""
        status, fam = self.call("POST", "/api/v1/family/create",
                                {"familyName": "李家", "ownerName": "儿子"})
        self.assertEqual(status, 200, fam)
        status, look = self.call("GET", "/api/v1/family/lookup?inviteCode=" + fam["inviteCode"])
        self.assertEqual(status, 200, look)
        self.assertEqual(look["familyName"], "李家")
        self.assertEqual(look["memberCount"], 1)
        status, detail = self.call("GET", "/api/v1/family?familyId=" + fam["familyId"])
        self.assertEqual(status, 200, detail)
        self.assertEqual(detail["memberCount"], 1)

class IngestTestCase(unittest.TestCase):
    """数据接入层：三种通道的公共逻辑。"""

    def test_01_normalize_source(self):
        from gateway.ingest import normalize_source
        self.assertEqual(normalize_source("hardware"), "hardware")
        self.assertEqual(normalize_source("real"), "hardware")      # 兼容旧字段
        self.assertEqual(normalize_source("HARDWARE"), "hardware")
        self.assertEqual(normalize_source("test"), "test")
        self.assertEqual(normalize_source("key_sim"), "test")       # 按键模拟 -> test
        self.assertEqual(normalize_source(None), "test")            # 缺省不冒充硬件
        self.assertEqual(normalize_source(""), "test")

    def test_02_parse_payload_rejects_bad(self):
        from gateway.ingest import parse_payload
        for bad in ("{bad json", "", "   ", "[1,2,3]", "123"):
            data, err = parse_payload(bad)
            self.assertIsNone(data, "非法报文不应被接受：%r" % bad)
            self.assertTrue(err)

    def test_03_extract_serial_json(self):
        from gateway.ingest import extract_serial_json
        line = '[12:00:01][net] #JSON#{"deviceId":"bearpi-001","seq":7}'
        self.assertEqual(extract_serial_json(line),
                         '{"deviceId":"bearpi-001","seq":7}')
        self.assertEqual(extract_serial_json("[app] boot ok"), "")
        self.assertEqual(extract_serial_json(""), "")

    def test_04_udp_ingest_end_to_end(self):
        import socket
        import time
        from gateway.api import Api
        from gateway.ingest import UdpIngest
        from gateway.store import Store

        tmp = tempfile.mkdtemp(prefix="udp-")
        store = Store(os.path.join(tmp, "udp.db"))
        api = Api(store)

        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()

        reader = UdpIngest(api, "127.0.0.1", port, logger=lambda m: None)
        reader.start()
        time.sleep(0.3)

        payload = {
            "deviceId": "dev-udp",
            "ts": iso(datetime.now(TZ)),
            "seq": 1,
            "pirDetected": True,
            "lastMotionZone": "living",
            "source": "real",          # 旧字段，应被归一化成 hardware
        }
        sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sender.sendto(json.dumps(payload).encode("utf-8"), ("127.0.0.1", port))
        sender.close()
        time.sleep(0.6)

        snap, _ = store.latest_snapshot()
        self.assertIsNotNone(snap, "UDP 报文应被写入快照表")
        self.assertEqual(snap["deviceId"], "dev-udp")
        self.assertEqual(snap["source"], "hardware")

        # 再发一条非法报文，服务不能崩，也不能写库
        before = store.latest_snapshot()[0]["seq"]
        sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sender.sendto(b"{not-json", ("127.0.0.1", port))
        sender.close()
        time.sleep(0.4)
        after = store.latest_snapshot()[0]["seq"]
        self.assertEqual(before, after, "非法报文不应影响已有数据")
        self.assertEqual(reader.dropped, 1)

        reader.stop()
        store.close()
        shutil.rmtree(tmp, ignore_errors=True)



class BoardEventsTestCase(unittest.TestCase):
    """板端本地阈值判定事件 -> 台账 -> 下行确认，一条链路跑通。

    这条链路是赛题“开发板识别异常 + 家属处置闭环”的最小可验证单元：
      固件判出 L2 -> POST /api/v1/events -> 网关建单
      -> APP 确认 -> 固件 GET /api/v1/alarms/active 看到 pendingCount=0
    """

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-board-")
        cls.store = Store(os.path.join(cls.tmpdir, "board.db"))
        cls.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.httpd.api = Api(cls.store)
        cls.httpd.daemon_threads = True
        cls.port = cls.httpd.server_address[1]
        cls.thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = "http://127.0.0.1:%d" % cls.port

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.store.close()
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def call(self, method, path, body=None):
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = request.Request(self.base + path, data=data, method=method,
                              headers={"Content-Type": "application/json"})
        try:
            with request.urlopen(req, timeout=5) as resp:
                return resp.status, json.loads(resp.read().decode("utf-8") or "{}")
        except error.HTTPError as exc:
            return exc.code, json.loads(exc.read().decode("utf-8") or "{}")

    def board_event(self, device, etype, level=None, message=None, source="hardware"):
        now = datetime.now(TZ).replace(microsecond=0)
        payload = {"zone": "unknown"}
        if level is not None:
            payload["level"] = level
        if message is not None:
            payload["message"] = message
        return self.call("POST", "/api/v1/events", {
            "deviceId": device,
            "ts": now.isoformat(),
            "type": etype,
            "source": source,
            "payload": payload,
        })

    def active(self, device):
        return self.call("GET", "/api/v1/alarms/active?deviceId=%s" % device)

    # ------------------------------------------------------------------
    def test_01_board_event_creates_alarm_and_ack_clears_pending(self):
        """板端上报 L2 温度事件 -> 建单 -> 确认后 pendingCount 归零。"""
        status, body = self.board_event("board-a", "TEMP_HIGH", "L2", "室内温度 36.0℃，过高")
        self.assertEqual(status, 200)
        created = body["created"]
        self.assertIsNotNone(created, "板端事件应能建单")
        self.assertEqual(created["type"], "TEMP_HIGH")
        self.assertEqual(created["level"], "L2")
        self.assertEqual(created["deviceId"], "board-a")
        self.assertEqual(created["source"], "hardware")

        status, counts = self.active("board-a")
        self.assertEqual(status, 200)
        self.assertEqual(counts["pendingCount"], 1)
        self.assertEqual(counts["acknowledgedCount"], 0)
        self.assertEqual(counts["pending"][0]["eventId"], created["eventId"])

        status, _ = self.call("POST", "/api/v1/alarms/%s/ack" % created["eventId"],
                              {"operator": "family"})
        self.assertEqual(status, 200)

        status, counts = self.active("board-a")
        self.assertEqual(counts["pendingCount"], 0, "家属确认后板端不应再闪")
        self.assertEqual(counts["acknowledgedCount"], 1)

    def test_02_same_type_not_reported_twice(self):
        """同一设备同类型告警还在活动期时，板端反复上报也只建一条单。"""
        status, first = self.board_event("board-b", "HUM_HIGH", "L2", "室内湿度过高")
        self.assertEqual(status, 200)
        self.assertIsNotNone(first["created"])

        status, second = self.board_event("board-b", "HUM_HIGH", "L2", "室内湿度过高")
        self.assertEqual(status, 200)
        self.assertIsNone(second["created"], "冷却去重应拦掉重复建单")

        status, counts = self.active("board-b")
        self.assertEqual(counts["pendingCount"], 1)

    def test_03_devices_do_not_share_led_state(self):
        """A 家确认了，不能把 B 家的灯点亮：统计必须按设备隔离。"""
        status, alarm_c = self.board_event("board-c", "TEMP_FIRE", "L3", "疑似火情")
        self.assertIsNotNone(alarm_c["created"])
        status, alarm_d = self.board_event("board-d", "DOOR_OPEN", "L1", "门窗长时间未关")
        self.assertIsNotNone(alarm_d["created"])

        self.call("POST", "/api/v1/alarms/%s/ack" % alarm_c["created"]["eventId"],
                  {"operator": "family"})

        status, counts_c = self.active("board-c")
        self.assertEqual(counts_c["pendingCount"], 0)
        self.assertEqual(counts_c["acknowledgedCount"], 1)

        status, counts_d = self.active("board-d")
        self.assertEqual(counts_d["pendingCount"], 1, "B 家没处理，必须仍是待确认")
        self.assertEqual(counts_d["acknowledgedCount"], 0)

    def test_04_unknown_type_is_ignored(self):
        """不认识的事件类型不建单、也不报错，避免固件升级后把网关搞崩。"""
        status, body = self.board_event("board-e", "GAS", "L3", "燃气泄漏")
        self.assertEqual(status, 200)
        self.assertIsNotNone(body["created"], "GAS 属于直报类型，仍应建单")

        status, body = self.board_event("board-e", "NOT_A_TYPE", None, None)
        self.assertEqual(status, 200)
        self.assertIsNone(body["created"])


class _FakeSiren(object):
    """替身：只实现播报线程里被 HTTP 层用到的那几个属性/方法。"""

    def __init__(self):
        self.muted = False
        self.arm_all = False

    def set_mode(self, mode):
        if mode not in ("on", "off", "auto"):
            raise ValueError("mode 只能是 on / off / auto")
        self.muted = (mode == "off")
        self.arm_all = (mode == "on")
        return {"mode": mode, "pending": 0}


class SirenControlTestCase(unittest.TestCase):
    """播报开关：把"启动服务"和"开始报警"分开。"""

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="shadowlamp-siren-")
        cls.store = Store(os.path.join(cls.tmpdir, "siren.db"))
        cls.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.httpd.api = Api(cls.store)
        cls.httpd.daemon_threads = True
        cls.port = cls.httpd.server_address[1]
        cls.thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = "http://127.0.0.1:%d" % cls.port

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.store.close()
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def call(self, method, path, body=None):
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = request.Request(self.base + path, data=data, method=method,
                              headers={"Content-Type": "application/json"})
        try:
            with request.urlopen(req, timeout=5) as resp:
                return resp.status, json.loads(resp.read().decode("utf-8") or "{}")
        except error.HTTPError as exc:
            return exc.code, json.loads(exc.read().decode("utf-8") or "{}")

    def test_01_without_siren_returns_409(self):
        """网关没带 --siren 时，开关接口要明确说"没开"，而不是假装成功。"""
        status, body = self.call("GET", "/api/v1/siren")
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "NO_SIREN")

    def test_02_bad_mode_returns_400(self):
        """乱传模式要 400，不能悄悄当成开。"""
        self.httpd.api.siren_control = _FakeSiren()
        status, body = self.call("POST", "/api/v1/siren", {"mode": "loud"})
        self.assertEqual(status, 400)
        self.assertEqual(body["code"], "BAD_MODE")

    def test_03_on_off_switch(self):
        """on 立刻播报、off 立刻静音，GET 能读回当前模式。"""
        fake = _FakeSiren()
        self.httpd.api.siren_control = fake

        status, body = self.call("POST", "/api/v1/siren", {"mode": "on"})
        self.assertEqual(status, 200)
        self.assertEqual(body["mode"], "on")
        self.assertTrue(fake.arm_all)

        status, body = self.call("GET", "/api/v1/siren")
        self.assertEqual(body["mode"], "on")

        status, body = self.call("POST", "/api/v1/siren", {"mode": "off"})
        self.assertEqual(body["mode"], "off")
        self.assertTrue(fake.muted)
        status, body = self.call("GET", "/api/v1/siren")
        self.assertEqual(body["mode"], "off")


if __name__ == "__main__":
    unittest.main(verbosity=2)
