# -*- coding: utf-8 -*-
"""分级播报开关：把"启动服务"和"开始报警"分开。

网关带 --siren 启动后，**只播报启动之后新产生的告警**——历史待确认告警
不会在启动瞬间哗啦响起来（现场演示最怕这个）。要立刻听声音，就跑：

    python scripts\\siren.py on      # 连历史待确认一起播（马上响）
    python scripts\\siren.py off     # 立刻静音（正在响的也停）
    python scripts\\siren.py auto    # 回到默认：只播启动后的新告警
    python scripts\\siren.py status  # 看当前模式 + 还有几条待确认

默认连 http://127.0.0.1:8780，换端口用 --base。
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request

DEFAULTS = {
    "on": "连历史待确认一起播报（开始响）",
    "off": "静音（正在响的也停）",
    "auto": "只播报之后新产生的告警",
}


def call(base, method="GET", payload=None, timeout=5):
    url = base.rstrip("/") + "/api/v1/siren"
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def main(argv=None):
    parser = argparse.ArgumentParser(description="守夜灯 · 分级播报开关")
    parser.add_argument("mode", nargs="?", default="status",
                        choices=["on", "off", "auto", "status"],
                        help="on=马上响 / off=静音 / auto=只响新告警 / status=看状态")
    parser.add_argument("--base", default="http://127.0.0.1:8780", help="网关地址")
    args = parser.parse_args(argv)

    try:
        if args.mode == "status":
            info = call(args.base)
        else:
            info = call(args.base, "POST", {"mode": args.mode})
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", "replace")
        try:
            detail = json.loads(body).get("message") or body
        except Exception:
            detail = body
        print("失败（HTTP %d）：%s" % (exc.code, detail))
        if exc.code == 409:
            print("提示：网关启动时没带 --siren，播报功能没开。")
        return 1
    except Exception as exc:
        print("连不上网关 %s：%s" % (args.base, exc))
        print("提示：先确认网关窗口在跑（http://127.0.0.1:8780/health）。")
        return 1

    mode = info.get("mode")
    pending = info.get("pending", 0)
    label = {"on": "已开启（含历史待确认）", "off": "已静音", "auto": "自动（只播新告警）"}.get(mode, mode)
    print("播报模式：%s，待确认告警 %d 条。" % (label, pending))
    if args.mode != "status":
        print("  " + DEFAULTS[args.mode])
    return 0


if __name__ == "__main__":
    sys.exit(main())
