# -*- coding: utf-8 -*-
"""清理设备列表里的“多余设备”，让「我的 → 设备切换」只剩主设备。

之前默认只清 bench-01/bench-02，结果 verify_multi.py 造出来的 bench-07/08 还在，
APP 里看着就是“多余设备”。现在改成：**默认清掉除主设备以外的全部设备**，
并连带清空它们的快照、告警、活动记录。

用法：
    python scripts/clean_test_devices.py                 # 默认：除 bearpi-001 外全清
    python scripts/clean_test_devices.py --dry-run       # 只看会删什么
    python scripts/clean_test_devices.py --keep bench-07 # 额外保留某台
    python scripts/clean_test_devices.py --device bench-08   # 只清指定设备
    python scripts/clean_test_devices.py --db data/lab.db

注意：APP 端的设备列表是每次进页面重新拉的（`GET /api/v1/devices`），
清完不需要重启 APP，等下一次 10 秒轮询或手动切一次 Tab 就刷新了。
"""
from __future__ import annotations

import argparse
import os
import sqlite3
import sys

BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_DB = os.path.join(BASE, "data", "gateway.db")
# 这些 id 绝不删：真实板子或演示主设备
KEEP = set(["bearpi-001"])


def main():
    parser = argparse.ArgumentParser(description="清理设备列表里的多余设备")
    parser.add_argument("--db", default=DEFAULT_DB, help="数据库路径，默认 data/gateway.db")
    parser.add_argument("--device", action="append", default=None,
                        help="只清指定的 deviceId，可重复")
    parser.add_argument("--keep", action="append", default=None,
                        help="额外保留的 deviceId，可重复")
    parser.add_argument("--all-extra", action="store_true",
                        help="（默认行为，保留兼容）除主设备外全清")
    parser.add_argument("--dry-run", action="store_true", help="只统计，不删除")
    args = parser.parse_args()

    db = os.path.abspath(args.db)
    if not os.path.exists(db):
        print("数据库不存在：%s" % db)
        return 2

    keep = set(KEEP)
    if args.keep:
        keep |= set(args.keep)

    conn = sqlite3.connect(db)
    tables = [r[0] for r in conn.execute(
        "SELECT name FROM sqlite_master WHERE type='table'").fetchall()]

    if args.device:
        targets = [d for d in args.device if d not in keep]
    else:
        rows = conn.execute("SELECT DISTINCT device_id FROM device").fetchall()
        try:
            rows += conn.execute("SELECT DISTINCT device_id FROM snapshot").fetchall()
        except sqlite3.Error:
            pass
        ids = set(r[0] for r in rows if r[0])
        targets = sorted(i for i in ids if i not in keep)

    if not targets:
        print("没有可清理的设备（主设备 %s 受保护）" % "、".join(sorted(keep)))
        conn.close()
        return 0

    # 只处理带 device_id 列的表（activity 是全局活动流，没有设备维度，跳过）
    dev_tables = []
    for t in ("device", "snapshot", "alarm", "activity"):
        if t not in tables:
            continue
        cols = [r[1] for r in conn.execute("PRAGMA table_info(%s)" % t).fetchall()]
        if "device_id" in cols:
            dev_tables.append(t)

    total = 0
    for dev in targets:
        counts = {}
        for t in dev_tables:
                counts[t] = conn.execute(
                    "SELECT COUNT(*) FROM %s WHERE device_id=?" % t, (dev,)).fetchone()[0]
        n = sum(counts.values())
        total += n
        print("%-14s %s" % (dev, "  ".join("%s %d" % (k, v) for k, v in counts.items())))
        if args.dry_run:
            continue
        for t in counts:
            conn.execute("DELETE FROM %s WHERE device_id=?" % t, (dev,))
    if args.dry_run:
        print("dry-run：共 %d 条记录可清理，未改动数据库" % total)
    else:
        conn.commit()
        print("已清理 %d 条记录：%s" % (total, "、".join(targets)))
        left = conn.execute("SELECT device_id FROM device ORDER BY device_id").fetchall()
        print("现在库里还剩：%s" % "、".join(r[0] for r in left))
    conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
