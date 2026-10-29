# -*- coding: utf-8 -*-
"""验证旧库（seq 作主键）能被新代码平滑迁移，数据不丢。"""
import os
import sqlite3
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from gateway.store import Store  # noqa: E402

OLD = """
CREATE TABLE snapshot (
    seq INTEGER PRIMARY KEY,
    device_id TEXT NOT NULL,
    ts TEXT NOT NULL,
    pir INTEGER NOT NULL DEFAULT 0,
    last_motion_ts TEXT, last_motion_zone TEXT,
    temperature REAL, humidity REAL, lux REAL,
    door_open INTEGER, battery REAL, power_state TEXT, rssi INTEGER,
    online INTEGER NOT NULL DEFAULT 1,
    source TEXT NOT NULL DEFAULT 'real',
    fw_version TEXT, received_at TEXT NOT NULL
);
"""

tmp = tempfile.mkdtemp(prefix="migrate-")
path = os.path.join(tmp, "old.db")

conn = sqlite3.connect(path)
conn.executescript(OLD)
conn.execute(
    "INSERT INTO snapshot(seq, device_id, ts, pir, temperature, humidity, source, received_at) "
    "VALUES(1,'bearpi-001','2026-10-26T10:00:00+08:00',1,23.5,50.0,'real','2026-10-26T10:00:01+08:00')")
conn.execute(
    "INSERT INTO snapshot(seq, device_id, ts, pir, temperature, humidity, source, received_at) "
    "VALUES(2,'bearpi-001','2026-10-26T10:00:01+08:00',0,23.6,50.1,'real','2026-10-26T10:00:02+08:00')")
conn.commit()
conn.close()

store = Store(path)  # 触发迁移
latest, raw = store.latest_snapshot()
count = store.conn.execute("SELECT COUNT(*) AS c FROM snapshot").fetchone()["c"]

print("迁移后行数 :", count)
print("最新一条   :", latest["ts"], "seq=", latest["seq"], "temp=", latest["temperature"])
print("是否有 id 列:", "id" in [r[1] for r in store.conn.execute("PRAGMA table_info(snapshot)")])
print("旧表是否残留:", store.conn.execute(
    "SELECT COUNT(*) FROM sqlite_master WHERE name='snapshot_legacy'").fetchone()[0])

assert count == 2, "迁移丢数据"
assert raw["seq"] == 2, "排序应按 id 取最新"
print("迁移 OK")
store.close()
