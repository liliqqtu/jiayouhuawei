# -*- coding: utf-8 -*-
"""命令行小工具：PowerShell 里没有好用的 curl 时的替代品（纯标准库）。

背景：Windows 自带 PowerShell 5.1 把 `curl` 映射到 Invoke-WebRequest，
而 `curl.exe -d '{"a":1}'` 这种内联 JSON 在 PowerShell 里会被拆引号，服务端收到
的是非法 JSON。要避开引号地狱，用这个脚本最省事——参数里不需要写任何双引号。

    python scripts/api.py health
    python scripts/api.py get /api/v1/snapshot
    python scripts/api.py get "/api/v1/snapshot?deviceId=elder-002"
    python scripts/api.py push --device elder-002 --no-motion
    python scripts/api.py post /api/v1/alarms/EVT-1/ack --kv operator=daughter
    python scripts/api.py post /api/v1/snapshot --json-file frame.json

返回体原样打印（UTF-8），HTTP 状态码 >=400 时退出码为 1，方便脚本判断。
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone

TZ = timezone(timedelta(hours=8))
DEFAULT_BASE = "http://127.0.0.1:8780"


def fmt_json(obj):
    return json.dumps(obj, ensure_ascii=False, sort_keys=False)


def iso_now():
    return datetime.now(TZ).replace(microsecond=0).isoformat()


def build_frame(args):
    """拼一帧符合数据契约的 SensorSnapshot（未知值一律 null，不猜）。"""
    now = iso_now()
    motion = not args.no_motion
    return {
        "deviceId": args.device,
        "seq": args.seq,
        "ts": args.ts or now,
        "pirDetected": motion,
        "lastMotionTs": (args.ts or now) if motion else None,
        "lastMotionZone": args.zone if motion else None,
        "temperature": args.temp,
        "humidity": args.hum,
        "lux": args.lux,
        "doorOpen": args.door,
        "sosPressed": args.sos,
        "gas": None,
        "smoke": None,
        "powerState": "mains",
        "battery": 95.0,
        "rssi": -55,
        "online": True,
        "fwVersion": "1.0.0",
        "source": args.source,
    }


def request(base, method, path, body=None):
    url = base.rstrip("/") + path
    data = None if body is None else fmt_json(body).encode("utf-8")
    req = urllib.request.Request(url, data=data, method=method)
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=8) as resp:
            return resp.getcode(), resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode("utf-8", "replace")
    except urllib.error.URLError as exc:
        print("连不上 %s（网关没启动？端口对不对？）：%s" % (url, exc.reason))
        sys.exit(2)


def parse_kv(pairs):
    out = {}
    for item in pairs or []:
        if "=" not in item:
            raise SystemExit("--kv 需要 key=value 形式：%s" % item)
        k, v = item.split("=", 1)
        # 纯数字的给 number，其余按字符串（处置结果、操作人都是字符串）
        out[k] = int(v) if v.isdigit() else v
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description="守夜灯网关命令行小工具")
    ap.add_argument("--base", default=DEFAULT_BASE, help="网关地址")
    sub = ap.add_subparsers(dest="cmd", required=True)

    for name in ("get", "health"):
        p = sub.add_parser(name)
        p.add_argument("path", nargs="?", default="/health")

    p = sub.add_parser("post")
    p.add_argument("path")
    p.add_argument("--kv", action="append", help="key=value，可重复")
    p.add_argument("--json", default=None, help="直接给 JSON 字符串")
    p.add_argument("--json-file", default=None, help="从文件读 JSON（避开引号问题）")

    p = sub.add_parser("push", help="上报一帧传感器数据（演示用）")
    p.add_argument("--device", default="bearpi-001")
    p.add_argument("--seq", type=int, default=None)
    p.add_argument("--ts", default=None, help="ISO 时间，默认现在")
    p.add_argument("--motion", dest="no_motion", action="store_false", default=False)
    p.add_argument("--no-motion", dest="no_motion", action="store_true")
    p.add_argument("--zone", default="living")
    p.add_argument("--temp", type=float, default=23.4)
    p.add_argument("--hum", type=float, default=52.0)
    p.add_argument("--lux", type=float, default=180.0)
    p.add_argument("--door", action="store_true")
    p.add_argument("--sos", action="store_true")
    p.add_argument("--source", default="test", choices=["hardware", "test"])

    args = ap.parse_args(argv)

    if args.cmd in ("get", "health"):
        path = args.path
        if args.cmd == "health":
            path = "/health"
        code, text = request(args.base, "GET", path)
    else:
        if args.cmd == "push":
            seq = args.seq
            if seq is None:  # 没给序号就按时间造一个，保证单调递增
                seq = int(datetime.now(TZ).timestamp()) % 100000
            args.seq = seq
            body = build_frame(args)
            path = "/api/v1/snapshot"
        else:
            path = args.path
            if args.json_file:
                with open(args.json_file, "r", encoding="utf-8") as f:
                    body = json.load(f)
            elif args.json:
                body = json.loads(args.json)
            else:
                body = parse_kv(args.kv)
        code, text = request(args.base, "POST", path, body)
        if args.cmd == "push":
            print("→ POST %s  seq=%s device=%s" % (path, body["seq"], body["deviceId"]))

    try:
        print(fmt_json(json.loads(text)) if text else text)
    except ValueError:
        print(text)
    if code >= 400:
        print("HTTP %d" % code)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
