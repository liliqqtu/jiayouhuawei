# -*- coding: utf-8 -*-
"""SQLite 数据层：建表、写入、查询、序列化。

设计取舍：
1. 只用标准库 sqlite3，不引入 ORM——演示机器上少一个依赖就少一个坑。
2. 时间统一存带时区的 ISO 字符串，日期/小时用 substr 直接截取，
   避免在 SQL 里做时区转换（设备本来就发 +08:00）。
3. 连接用 check_same_thread=False + 读写锁，配合 ThreadingHTTPServer。
"""
from __future__ import annotations

import os
import sqlite3
import threading

from gateway import util
from gateway.config import Thresholds

SCHEMA = """
CREATE TABLE IF NOT EXISTS snapshot (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id      TEXT NOT NULL,
    seq            INTEGER NOT NULL,
    ts             TEXT NOT NULL,
    pir            INTEGER NOT NULL DEFAULT 0,
    last_motion_ts TEXT,
    last_motion_zone TEXT,
    temperature    REAL,
    humidity       REAL,
    lux            REAL,
    door_open      INTEGER,
    battery        REAL,
    power_state    TEXT,
    rssi           INTEGER,
    online         INTEGER NOT NULL DEFAULT 1,
    source         TEXT NOT NULL DEFAULT 'real',
    fw_version     TEXT,
    received_at    TEXT NOT NULL,
    time_adjusted  INTEGER NOT NULL DEFAULT 0,
    UNIQUE(device_id, seq)
);
CREATE INDEX IF NOT EXISTS idx_snapshot_ts ON snapshot(ts);

CREATE TABLE IF NOT EXISTS alarm (
    event_id      TEXT PRIMARY KEY,
    device_id     TEXT,
    ts            TEXT NOT NULL,
    type          TEXT NOT NULL,
    level         TEXT NOT NULL,
    message       TEXT NOT NULL,
    status        TEXT NOT NULL,
    ack_ts        TEXT,
    ack_operator  TEXT,
    handle_type   TEXT,
    handle_result TEXT,
    closed_ts     TEXT,
    zone          TEXT,
    source        TEXT NOT NULL DEFAULT 'real',
    evidence      TEXT,
    snapshot_json TEXT,
    escalated     INTEGER NOT NULL DEFAULT 0,
    created_at    TEXT,
    updated_at    TEXT
);
CREATE INDEX IF NOT EXISTS idx_alarm_status ON alarm(status, ts);
CREATE INDEX IF NOT EXISTS idx_alarm_type ON alarm(type, status);

CREATE TABLE IF NOT EXISTS activity (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    ts           TEXT NOT NULL,
    zone         TEXT,
    event_type   TEXT NOT NULL,
    count_window INTEGER,
    source       TEXT NOT NULL DEFAULT 'real'
);
CREATE INDEX IF NOT EXISTS idx_activity_ts ON activity(ts);

CREATE TABLE IF NOT EXISTS device (
    device_id       TEXT PRIMARY KEY,
    online          INTEGER NOT NULL DEFAULT 0,
    battery         REAL,
    power_state     TEXT,
    rssi            INTEGER,
    fw_version      TEXT,
    last_seen       TEXT,
    uptime          INTEGER,
    last_motion_ts  TEXT,
    last_zone       TEXT,
    door_open       INTEGER,
    door_open_since TEXT,
    away_since      TEXT,
    boot_ts         TEXT
);

CREATE TABLE IF NOT EXISTS kv (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
"""

LEVEL_ORDER = {"L1": 1, "L2": 2, "L3": 3}


def _b(value):
    if value is None:
        return None
    return bool(value)


def snapshot_json(row):
    if row is None:
        return None
    return {
        "deviceId": row["device_id"],
        "ts": row["ts"],
        "seq": row["seq"],
        "pirDetected": _b(row["pir"]),
        "lastMotionTs": row["last_motion_ts"],
        "lastMotionZone": row["last_motion_zone"],
        "temperature": row["temperature"],
        "humidity": row["humidity"],
        "lux": row["lux"],
        "doorOpen": _b(row["door_open"]),
        "battery": row["battery"],
        "powerState": row["power_state"],
        "rssi": row["rssi"],
        "online": _b(row["online"]),
        "source": row["source"],
        "fwVersion": row["fw_version"],
        "timeAdjusted": bool(row["time_adjusted"]),
    }


def alarm_json(row):
    if row is None:
        return None
    return {
        "eventId": row["event_id"],
        "deviceId": row["device_id"],
        "ts": row["ts"],
        "type": row["type"],
        "level": row["level"],
        "message": row["message"],
        "status": row["status"],
        "ackTs": row["ack_ts"],
        "ackOperator": row["ack_operator"],
        "handleType": row["handle_type"],
        "handleResult": row["handle_result"],
        "closedTs": row["closed_ts"],
        "zone": row["zone"],
        "source": row["source"],
        "evidence": row["evidence"],
        "escalated": bool(row["escalated"]),
        # 入网时间：判断"这条告警是不是网关启动之后才产生的"要用它
        "createdTs": (row["created_at"] if "created_at" in row.keys() else None),
    }


class Store(object):
    def __init__(self, path):
        directory = os.path.dirname(os.path.abspath(path))
        if directory and not os.path.isdir(directory):
            os.makedirs(directory)
        self.path = path
        self.lock = threading.RLock()
        self.conn = sqlite3.connect(path, check_same_thread=False)
        self.conn.row_factory = sqlite3.Row
        self.conn.execute("PRAGMA journal_mode=WAL")
        self._migrate_snapshot()
        self.conn.executescript(SCHEMA)
        # 轻量迁移：老库补 created_at（升级判断要用入网时间，而不是设备事件时间）
        try:
            self.conn.execute("ALTER TABLE alarm ADD COLUMN created_at TEXT")
        except sqlite3.OperationalError:
            pass
        self.conn.execute(
            "UPDATE alarm SET created_at=COALESCE(created_at, updated_at, ts) WHERE created_at IS NULL")
        self.conn.commit()

    def _migrate_snapshot(self):
        """老库（seq 作主键）平滑迁移到新结构；数据保留。"""
        try:
            cols = [row[1] for row in self.conn.execute("PRAGMA table_info(snapshot)")]
        except sqlite3.DatabaseError:
            return
        if not cols or "id" in cols:
            return
        self.conn.execute("ALTER TABLE snapshot RENAME TO snapshot_legacy")
        self.conn.commit()
        self.conn.executescript(SCHEMA)
        self.conn.execute(
            "INSERT OR IGNORE INTO snapshot(device_id, seq, ts, pir, last_motion_ts, "
            "last_motion_zone, temperature, humidity, lux, door_open, battery, power_state, "
            "rssi, online, source, fw_version, received_at) "
            "SELECT device_id, seq, ts, pir, last_motion_ts, last_motion_zone, temperature, "
            "humidity, lux, door_open, battery, power_state, rssi, online, source, fw_version, "
            "received_at FROM snapshot_legacy")
        self.conn.execute("DROP TABLE snapshot_legacy")
        self.conn.commit()

    def close(self):
        with self.lock:
            self.conn.close()

    # ---------------------------------------------------------------- kv
    def kv_get(self, key, default=None):
        with self.lock:
            row = self.conn.execute("SELECT value FROM kv WHERE key=?", (key,)).fetchone()
        if row is None:
            return default
        return util.load(row["value"], default)

    def kv_set(self, key, value):
        with self.lock:
            self.conn.execute(
                "INSERT INTO kv(key, value) VALUES(?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                (key, util.dump(value)))
            self.conn.commit()

    # ------------------------------------------------------------ config
    def thresholds(self):
        data = self.kv_get("thresholds", None)
        return Thresholds.from_dict(data) if data else Thresholds()

    def save_thresholds(self, data):
        th = Thresholds.merge(data)
        self.kv_set("thresholds", th.to_dict())
        return th

    # ------------------------------------------------------------ device
    def get_device(self, device_id):
        with self.lock:
            row = self.conn.execute("SELECT * FROM device WHERE device_id=?", (device_id,)).fetchone()
        return dict(row) if row else None

    def upsert_device(self, device_id, **fields):
        with self.lock:
            exists = self.conn.execute(
                "SELECT 1 FROM device WHERE device_id=?", (device_id,)).fetchone()
            if not exists:
                self.conn.execute("INSERT INTO device(device_id) VALUES(?)", (device_id,))
            if fields:
                cols = ", ".join("%s=?" % k for k in fields)
                self.conn.execute(
                    "UPDATE device SET %s WHERE device_id=?" % cols,
                    list(fields.values()) + [device_id])
            self.conn.commit()

    def _device_dict(self, row):
        """device 表行 -> 对外的设备状态（单个设备与设备列表共用同一口径）。

        单独抽出来是为了"多设备"和"单设备"返回的字段完全一致，
        避免 APP 端出现两种形状。
        """
        from gateway.config import OFFLINE_SECONDS
        data = dict(row)
        seen = util.parse_ts(data.get("last_seen"))
        age = (util.now() - seen).total_seconds() if seen else None
        return {
            "deviceId": data["device_id"],
            "online": bool(age is not None and age <= OFFLINE_SECONDS),
            "battery": data["battery"],
            "powerState": data["power_state"],
            "rssi": data["rssi"],
            "fwVersion": data["fw_version"],
            "lastSeen": data["last_seen"],
            "uptime": data["uptime"],
            "staleSeconds": int(age) if age is not None else None,
            # 这台设备最近一帧数据的来源：hardware=真实设备，test=模拟演示。
            # APP 设备列表用它打标签，避免把模拟数据看成真传感器（数据诚信要求）。
            "source": self._latest_source(data["device_id"]),
        }

    def _latest_source(self, device_id):
        """这台设备最近一帧快照的 source；从没上报过就是 None。

        只用一条 ORDER BY id DESC LIMIT 1 的查询：snapshot 主键自增，id 最大即最新，
        比按 received_at 排序更稳（设备补传历史帧时 received_at 会乱序）。
        """
        with self.lock:
            row = self.conn.execute(
                "SELECT source FROM snapshot WHERE device_id=? ORDER BY id DESC LIMIT 1",
                (device_id,)).fetchone()
        return row["source"] if row else None

    def device_status(self, device_id=None):
        """device_id 为空时返回最近在线的那台（保持旧行为）。"""
        with self.lock:
            if device_id:
                row = self.conn.execute(
                    "SELECT * FROM device WHERE device_id=?", (device_id,)).fetchone()
            else:
                row = self.conn.execute("SELECT * FROM device ORDER BY last_seen DESC LIMIT 1").fetchone()
        if row is None:
            return None
        return self._device_dict(row)

    def list_devices(self):
        """全部设备，按最后在线时间倒序；给 APP 的"多老人/多设备"切换用。

        没有 last_seen 的设备（只建了档、还没上报过）排最后。
        """
        with self.lock:
            rows = self.conn.execute(
                "SELECT * FROM device ORDER BY "
                "CASE WHEN last_seen IS NULL THEN 1 ELSE 0 END, last_seen DESC, rowid DESC").fetchall()
        return [self._device_dict(r) for r in rows]

    # ---------------------------------------------------------- snapshot
    def save_snapshot(self, snap):
        row = {
            "seq": snap["seq"],
            "device_id": snap["deviceId"],
            "ts": snap["ts"],
            "pir": 1 if snap.get("pirDetected") else 0,
            "last_motion_ts": snap.get("lastMotionTs"),
            "last_motion_zone": snap.get("lastMotionZone"),
            "temperature": snap.get("temperature"),
            "humidity": snap.get("humidity"),
            "lux": snap.get("lux"),
            "door_open": None if snap.get("doorOpen") is None else (1 if snap["doorOpen"] else 0),
            "battery": snap.get("battery"),
            "power_state": snap.get("powerState"),
            "rssi": snap.get("rssi"),
            "online": 1 if snap.get("online", True) else 0,
            "source": snap.get("source", "real"),
            "fw_version": snap.get("fwVersion"),
            "received_at": util.iso_now(),
            "time_adjusted": 1 if snap.get("timeAdjusted") else 0,
        }
        with self.lock:
            # 设备重启后 seq 会从 1 重新计数。若同一个 (device_id, seq)
            # 隔了超过 2 分钟才再次出现，说明不是重试而是新一轮，分配新序号，
            # 避免把上一轮的历史记录覆盖掉。
            dup = self.conn.execute(
                "SELECT received_at FROM snapshot WHERE device_id=? AND seq=?",
                (row["device_id"], row["seq"])).fetchone()
            if dup is not None:
                prev = util.parse_ts(dup["received_at"])
                age = (util.now() - prev).total_seconds() if prev else 3600
                if age > 120:
                    top = self.conn.execute(
                        "SELECT MAX(seq) AS m FROM snapshot WHERE device_id=?",
                        (row["device_id"],)).fetchone()
                    row["seq"] = int(top["m"] or 0) + 1

            cols = ", ".join(row.keys())
            marks = ", ".join(["?"] * len(row))
            # 同一 seq 重复上报（重试/补传）直接覆盖，保证幂等
            self.conn.execute(
                "INSERT OR REPLACE INTO snapshot(%s) VALUES(%s)" % (cols, marks),
                list(row.values()))
            self.conn.commit()
        return row

    def latest_snapshot(self, device_id=None):
        """最新一帧；传 device_id 时只取这台设备的（多设备互不干扰）。"""
        with self.lock:
            if device_id:
                row = self.conn.execute(
                    "SELECT * FROM snapshot WHERE device_id=? ORDER BY id DESC LIMIT 1",
                    (device_id,)).fetchone()
            else:
                row = self.conn.execute("SELECT * FROM snapshot ORDER BY id DESC LIMIT 1").fetchone()
        return snapshot_json(row), dict(row) if row else None

    def list_snapshots(self, from_ts=None, to_ts=None, limit=200, device_id=None):
        sql = "SELECT * FROM snapshot WHERE 1=1"
        args = []
        if device_id:
            sql += " AND device_id = ?"
            args.append(device_id)
        if from_ts:
            sql += " AND ts >= ?"
            args.append(from_ts)
        if to_ts:
            sql += " AND ts <= ?"
            args.append(to_ts)
        sql += " ORDER BY id ASC LIMIT ?"
        args.append(int(limit))
        with self.lock:
            rows = self.conn.execute(sql, args).fetchall()
        return [snapshot_json(r) for r in rows]

    # ------------------------------------------------------------ alarm
    def _next_event_id(self, day):
        """按“当天已有编号的最大值 + 1”取号。

        历史缺陷：早先用 COUNT(*)+1，一旦删过告警（例如清理测试设备、
        手动清库）或并发插入，算出来的号会和已有记录撞车，报
        `UNIQUE constraint failed: alarm.event_id`，表现是“注入场景但没生成告警”。
        改成取最大后缀 + 1，删除历史记录不再影响新编号。
        """
        prefix = "EVT-%s-" % day.replace("-", "")
        key = "alarm_seq:%s" % day.replace("-", "")
        row = self.conn.execute(
            "SELECT event_id FROM alarm WHERE event_id LIKE ? "
            "ORDER BY event_id DESC LIMIT 1", (prefix + "%",)).fetchone()
        live = 0
        if row is not None:
            try:
                live = int(str(row["event_id"]).rsplit("-", 1)[1])
            except (ValueError, IndexError):
                live = 0
        # kv 里存当天已用过的最大号：即使把当天的告警全删了，也不会重发旧编号
        meta = self.conn.execute("SELECT value FROM kv WHERE key=?", (key,)).fetchone()
        saved = 0
        if meta is not None:
            try:
                saved = int(meta["value"])
            except (TypeError, ValueError):
                saved = 0
        seq = max(live, saved) + 1
        self.conn.execute("INSERT OR REPLACE INTO kv(key, value) VALUES(?, ?)",
                          (key, str(seq)))
        return "%s%04d" % (prefix, seq)

    def active_alarm(self, alarm_type, device_id=None):
        """同类型 + 同设备的活动期告警；多设备部署时互不影响。"""
        sql = ("SELECT * FROM alarm WHERE type=? AND status IN ('pending','acknowledged')")
        args = [alarm_type]
        if device_id:
            sql += " AND device_id=?"
            args.append(device_id)
        sql += " ORDER BY ts DESC LIMIT 1"
        with self.lock:
            row = self.conn.execute(sql, args).fetchone()
        return alarm_json(row)

    def create_alarm(self, alarm_type, level, message, ts, device_id=None,
                     zone=None, source="real", snapshot=None, evidence=None):
        with self.lock:
            event_id = self._next_event_id(util.day_of(ts))
            self.conn.execute(
                "INSERT INTO alarm(event_id, device_id, ts, type, level, message, status, "
                "zone, source, evidence, snapshot_json, created_at, updated_at) "
                "VALUES(?,?,?,?,?,?,'pending',?,?,?,?,?,?)",
                (event_id, device_id, ts, alarm_type, level, message, zone, source,
                 evidence, util.dump(snapshot) if snapshot else None,
                 util.iso_now(), util.iso_now()))
            self.conn.commit()
            row = self.conn.execute("SELECT * FROM alarm WHERE event_id=?", (event_id,)).fetchone()
        return alarm_json(row)

    def pending_counts_by_device(self):
        """每台设备当前有几条待确认告警：多设备场景下"是谁出事了"一眼可见。"""
        with self.lock:
            rows = self.conn.execute(
                "SELECT device_id, COUNT(*) AS c FROM alarm WHERE status='pending' "
                "GROUP BY device_id").fetchall()
        return dict((r["device_id"], int(r["c"])) for r in rows)

    def list_alarms(self, status=None, from_ts=None, to_ts=None, limit=100, device_id=None):
        sql = "SELECT * FROM alarm WHERE 1=1"
        args = []
        if status:
            sql += " AND status=?"
            args.append(status)
        if device_id:
            sql += " AND device_id=?"
            args.append(device_id)
        if from_ts:
            sql += " AND ts >= ?"
            args.append(from_ts)
        if to_ts:
            sql += " AND ts <= ?"
            args.append(to_ts)
        sql += " ORDER BY ts DESC LIMIT ?"
        args.append(int(limit))
        with self.lock:
            rows = self.conn.execute(sql, args).fetchall()
        return [alarm_json(r) for r in rows]

    def get_alarm(self, event_id):
        with self.lock:
            row = self.conn.execute("SELECT * FROM alarm WHERE event_id=?", (event_id,)).fetchone()
        return alarm_json(row)

    def alarm_counts(self, device_id=None):
        """按设备统计：待确认几条、已处理几条。

        板端 LED 靠这两个数字决定“闪 / 常亮 / 灭”，所以必须只看本设备 ——
        两台设备挂同一个网关时，不能因为 A 家确认了就把 B 家的灯点亮。
        """
        sql = ("SELECT "
               "SUM(CASE WHEN status='pending' THEN 1 ELSE 0 END) AS pending, "
               "SUM(CASE WHEN status IN ('acknowledged','closed') THEN 1 ELSE 0 END) AS handled "
               "FROM alarm")
        args = []
        if device_id:
            sql += " WHERE device_id=?"
            args.append(device_id)
        with self.lock:
            row = self.conn.execute(sql, args).fetchone()
        return {"pending": int(row["pending"] or 0), "handled": int(row["handled"] or 0)}

    def update_alarm(self, event_id, **fields):
        if not fields:
            return None
        fields["updated_at"] = util.iso_now()
        with self.lock:
            cols = ", ".join("%s=?" % k for k in fields)
            self.conn.execute("UPDATE alarm SET %s WHERE event_id=?" % cols,
                              list(fields.values()) + [event_id])
            self.conn.commit()
        return self.get_alarm(event_id)

    def escalate_stale(self, minutes):
        """待确认超过 N 分钟的告警标记升级（只标一次，避免重复刷屏）。"""
        cutoff = util.minutes_ago(minutes)
        with self.lock:
            rows = self.conn.execute(
                "SELECT event_id FROM alarm WHERE status='pending' AND escalated=0 "
                "AND COALESCE(created_at, updated_at, ts) < ?",
                (cutoff,)).fetchall()
            for row in rows:
                self.conn.execute(
                    "UPDATE alarm SET escalated=1, message=message||'（超时未确认，已自动升级）', "
                    "updated_at=? WHERE event_id=?", (util.iso_now(), row["event_id"]))
            self.conn.commit()
        return len(rows)

    # --------------------------------------------------------- activity
    def add_activity(self, ts, zone, event_type, count_window=1, source="real"):
        with self.lock:
            self.conn.execute(
                "INSERT INTO activity(ts, zone, event_type, count_window, source) VALUES(?,?,?,?,?)",
                (ts, zone, event_type, count_window, source))
            self.conn.commit()

    def hourly_counts(self, day):
        with self.lock:
            rows = self.conn.execute(
                "SELECT CAST(substr(ts,12,2) AS INTEGER) AS h, COUNT(*) AS c "
                "FROM activity WHERE substr(ts,1,10)=? GROUP BY h", (day,)).fetchall()
        buckets = [0] * 24
        for row in rows:
            hour = row["h"]
            if hour is not None and 0 <= hour < 24:
                buckets[hour] = row["c"]
        return buckets

    def day_count(self, day):
        with self.lock:
            row = self.conn.execute(
                "SELECT COUNT(*) AS c FROM activity WHERE substr(ts,1,10)=?", (day,)).fetchone()
        return row["c"] or 0

    def alarm_count(self, day):
        with self.lock:
            row = self.conn.execute(
                "SELECT COUNT(*) AS c FROM alarm WHERE substr(ts,1,10)=?", (day,)).fetchone()
        return row["c"] or 0

    def env_average(self, start_day, end_day):
        with self.lock:
            row = self.conn.execute(
                "SELECT AVG(temperature) AS t, AVG(humidity) AS h FROM snapshot "
                "WHERE substr(ts,1,10) BETWEEN ? AND ?", (start_day, end_day)).fetchone()
        t = row["t"] if row else None
        h = row["h"] if row else None
        return (round(t, 1) if t is not None else None,
                round(h, 1) if h is not None else None)
