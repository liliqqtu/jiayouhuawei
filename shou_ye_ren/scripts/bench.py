# -*- coding: utf-8 -*-
"""网关压测/容量评估：给"局域网 HTTP 够不够用"一个可复现的数字。

用法：
    python scripts/bench.py                          # 默认 300 帧、并发 4，打本机 8780
    python scripts/bench.py --frames 2000 --concurrency 8
    python scripts/bench.py --device bench-02 --cleanup   # 跑完清掉压测数据

为什么要这个脚本：答辩一定会问"一台网关能接几台设备""1 秒一帧会不会崩"。
光说"没问题"没人信；跑一遍，把 p50/p95、吞吐、错误率贴出来，
再顺带算出"1 Hz 单设备占用的余量"，结论就有据可依。
"""
from __future__ import annotations

import argparse
import json
import os
import sqlite3
import statistics
import sys
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime, timedelta, timezone

TZ = timezone(timedelta(hours=8))

LAT = []          # 全局延迟样本（秒），线程安全靠 GIL 下的 append
LAT_LOCK = threading.Lock()
COUNTS = {"ok": 0, "http4xx": 0, "http5xx": 0, "error": 0}
CNT_LOCK = threading.Lock()


def iso_now():
    return datetime.now(TZ).replace(microsecond=0).isoformat()


def make_frame(device, seq):
    """一帧 16 字段快照，数值随 seq 轻微变化，避免恒值被误判为"卡住"。"""
    base_t = 22.0 + (seq % 20) * 0.1
    return {
        "deviceId": device,
        "ts": iso_now(),
        "seq": seq,
        "pirDetected": seq % 7 == 0,
        "lastMotionTs": iso_now(),
        "lastMotionZone": "living",
        "temperature": round(base_t, 1),
        "humidity": round(45.0 + (seq % 15), 1),
        "lux": 120 + (seq % 40),
        "doorOpen": False,
        "battery": 95.0,
        "powerState": "mains",
        "rssi": -58,
        "online": True,
        "source": "test",
        "fwVersion": "bench-1.0",
    }


def post(url, payload, timeout=10.0):
    body = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            resp.read()
        dt = time.time() - t0
        with LAT_LOCK:
            LAT.append(dt)
        with CNT_LOCK:
            COUNTS["ok"] += 1
    except urllib.error.HTTPError as exc:
        with CNT_LOCK:
            COUNTS["http4xx" if exc.code < 500 else "http5xx"] += 1
        if COUNTS["http4xx"] + COUNTS["http5xx"] <= 3:
            print("  HTTP %d: %s" % (exc.code, exc.read()[:200].decode("utf-8", "ignore")))
    except Exception as exc:                                    # noqa: BLE001
        with CNT_LOCK:
            COUNTS["error"] += 1
        if COUNTS["error"] <= 3:
            print("  请求失败：%s" % exc)


def worker(url, device, total, concurrency, idx):
    per = total // concurrency
    start = idx * per + 1
    seq = start
    end = total + 1 if idx == concurrency - 1 else start + per
    while seq < end:
        post("%s/api/v1/snapshot" % url, make_frame(device, seq))
        seq += 1


def pct(values, p):
    if not values:
        return 0.0
    ordered = sorted(values)
    k = int(round((len(ordered) - 1) * p))
    return ordered[k]


def cleanup(db_path, like):
    if not os.path.exists(db_path):
        print("清理跳过：数据库不存在 %s" % db_path)
        return
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.execute("DELETE FROM snapshot WHERE device_id LIKE ?", (like,))
        deleted = cur.rowcount
        conn.commit()
    finally:
        conn.close()
    print("已清理压测数据：%d 行（device_id LIKE %s）" % (deleted, like))


def main(argv=None):
    parser = argparse.ArgumentParser(description="守夜灯网关压测")
    parser.add_argument("--url", default="http://127.0.0.1:8780", help="网关地址")
    parser.add_argument("--frames", type=int, default=300, help="总帧数，默认 300")
    parser.add_argument("--concurrency", type=int, default=4, help="并发线程数，默认 4")
    parser.add_argument("--device", default="bench-01", help="压测用的 deviceId")
    parser.add_argument("--db", default="data/gateway.db", help="清理用数据库路径")
    parser.add_argument("--cleanup", action="store_true", help="跑完删除压测设备的数据")
    args = parser.parse_args(argv)

    total = max(1, args.frames)
    concurrency = max(1, min(args.concurrency, total))

    print("压测目标：%s" % args.url)
    print("设备：%s · 总帧数 %d · 并发 %d" % (args.device, total, concurrency))

    # 先探活，避免对着一堆连接错误做统计
    try:
        with urllib.request.urlopen("%s/health" % args.url, timeout=3) as resp:
            health = json.loads(resp.read().decode("utf-8"))
        print("网关在线：uptime=%ss version=%s" % (health.get("uptime"), health.get("version")))
    except Exception as exc:                                    # noqa: BLE001
        print("网关不可达：%s" % exc)
        print("先启动网关：python run_gateway.py --port 8780")
        return 2

    threads = []
    t0 = time.time()
    for i in range(concurrency):
        th = threading.Thread(target=worker,
                              args=(args.url, args.device, total, concurrency, i))
        th.start()
        threads.append(th)
    for th in threads:
        th.join()
    elapsed = time.time() - t0

    ok = COUNTS["ok"]
    fail = COUNTS["http4xx"] + COUNTS["http5xx"] + COUNTS["error"]
    rate = ok / elapsed if elapsed > 0 else 0
    print("")
    print("结果：成功 %d / 失败 %d · 耗时 %.2fs · 吞吐 %.1f 帧/秒"
          % (ok, fail, elapsed, rate))
    if LAT:
        print("延迟：平均 %.1fms · p50 %.1fms · p95 %.1fms · p99 %.1fms"
              % (statistics.mean(LAT) * 1000, pct(LAT, 0.50) * 1000,
                 pct(LAT, 0.95) * 1000, pct(LAT, 0.99) * 1000))
    print("错误分类：4xx=%d 5xx=%d 网络=%d"
          % (COUNTS["http4xx"], COUNTS["http5xx"], COUNTS["error"]))

    if ok and rate > 0:
        devices_1hz = int(rate / 1)
        print("")
        print("容量结论：按 1 秒 1 帧估算，本机约能同时承载 %d 台设备（理论上限，"
              "留 3 倍余量的演示建议 ≤ %d 台）。" % (devices_1hz, max(1, devices_1hz // 3)))
        print("注意：这是「本机 + SQLite + 短连接」的数字，广域网/云平台应换 MQTT + 长连接，"
              "本轮不实现。")

    if args.cleanup:
        cleanup(args.db, args.device + "%")

    return 0 if fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
