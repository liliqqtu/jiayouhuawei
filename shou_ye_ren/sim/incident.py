# -*- coding: utf-8 -*-
"""演示中途注入异常：让首页/告警页立刻出现变化，用来讲“我们怎么应对”。

用法：
    python sim/incident.py --type temp       # 温度 35℃ -> L1 环境提醒
    python sim/incident.py --type hum        # 湿度 88%  -> L1 环境提醒
    python sim/incident.py --type night      # 夜间卫生间长时间无活动 -> L3 紧急
    python sim/incident.py --type idle       # 日间长时间无活动 -> L2 关注
    python sim/incident.py --type door       # 门磁开启超时 -> L1
    python sim/incident.py --type sos        # 随身按钮求助 -> L3 紧急

说明：注入的数据 source=test，APP 会标“测试/模拟”，不冒充真实采集。
"""
import argparse
import os
import sys
import time
from datetime import datetime, timedelta

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from device_sim import TZ_OFFSET, SimClient, iso  # noqa: E402

# 与 device_sim 共用的“保持”文件：写入一组异常值，模拟器持续上报
HOLD_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", ".demo_hold")
# 注入的异常值最多“挂”这么久，超时自动恢复，避免忘清理影响下一轮
HOLD_TTL_SEC = 12 * 60


def set_hold(hold):
    """写入异常保持值。

    两个退出条件（任一满足即恢复正常）：
      1. 手动跑 `--type reset`；
      2. 超过 HOLD_TTL_SEC（默认 12 分钟）自动过期，防止忘了清、下一轮演示看到怪数据。

    注意：**不要**用“当前没有待确认告警”来清 —— 注入瞬间告警还没创建，
    模拟器会在 2 秒内把保持文件删掉，于是门磁/无活动场景刚注入就被正常帧覆盖，
    表现就是“注入了但首页没变化”。
    """
    import json as _json
    import time as _time
    hold["clear_when_no_pending"] = False
    hold["expires_at"] = int(_time.time()) + HOLD_TTL_SEC
    d = os.path.dirname(HOLD_FILE)
    if not os.path.isdir(d):
        os.makedirs(d)
    with open(HOLD_FILE, "w", encoding="utf-8") as f:
        _json.dump(hold, f, ensure_ascii=False)


def clear_hold():
    if os.path.exists(HOLD_FILE):
        os.remove(HOLD_FILE)


def frame(client, ts, **kw):
    client.snapshot(ts, last_motion_ts=kw.pop("last_motion_ts", None), **kw)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--type", required=True,
                        choices=["temp", "fire", "hum", "lux", "night", "idle", "door", "sos", "reset"])
    parser.add_argument("--url", default="http://127.0.0.1:8780")
    parser.add_argument("--device", default="bearpi-001")
    parser.add_argument("--auto-close", action="store_true",
                        help="（可选）注入后立刻自关告警，仅用于快速自测")
    args = parser.parse_args()

    client = SimClient(args.url, args.device)
    health = client._get("/health")
    if not health:
        print("网关连不上：%s" % args.url)
        return 2

    now = datetime.now(TZ_OFFSET)

    # 注入前先关掉同类型的活动告警，保证“再跑一次还能看到新告警”
    if args.type == "reset":
        closed = client._close_pending()
        clear_hold()
        print("已关闭 %d 条待确认告警，数据已恢复正常" % closed)
        return 0

    client._close_pending()
    print("（已先关闭历史待确认告警，保证本次能生成新告警）")

    # 归零设备状态：先补一帧“门关着、有人活动”的实时帧。
    # 否则上一场景留下的“门开着 / 疑似外出”会把这一次的判定压掉
    # （例如先演示门磁再演示夜间无活动，就会看到夜间场景不报警）。
    try:
        client.snapshot(now, last_motion_ts=now, pir=True, zone="living",
                        temperature=23.0, humidity=50.0, lux=180.0, door_open=False)
    except Exception:
        pass

    if args.type == "temp":
        print("注入：室内温度 30.0℃（偏热，L1 提醒）")
        set_hold({"temperature": 30.0, "humidity": 52.0, "lux": 180.0,
                  "door_open": False, "pir": True, "zone": "living",
                  "last_motion_min_ago": 0})
        frame(client, now, pir=True, zone="living", temperature=30.0, humidity=52.0,
              lux=180.0, door_open=False, last_motion_ts=now)

    elif args.type == "fire":
        print("注入：2 分钟前 26℃，现在 58℃（升温速率 + 绝对高温）")
        base = now - timedelta(minutes=2)
        frame(client, base, pir=False, zone="living", temperature=26.0, humidity=50.0,
              lux=180.0, door_open=False, last_motion_ts=now - timedelta(minutes=20))
        set_hold({"temperature": 58.0, "humidity": 50.0, "lux": 500.0,
                  "door_open": False, "pir": False, "zone": "living",
                  "last_motion_min_ago": 20})
        frame(client, now, pir=False, zone="living", temperature=58.0, humidity=50.0,
              lux=500.0, door_open=False, last_motion_ts=now - timedelta(minutes=20))

    elif args.type == "lux":
        # 光照规则只在“夜间偏亮 / 白天采光不足”时才成立，
        # 所以这里固定注入凌晨 02:30 这个时刻，白天演示也能复现。
        base = now.replace(hour=2, minute=30, second=0, microsecond=0)
        print("注入：夜间 02:30 室内光照 420 lx（疑似忘关灯，L1 提醒）")
        set_hold({"temperature": 22.0, "humidity": 55.0, "lux": 420.0,
                  "door_open": False, "pir": True, "zone": "bedroom",
                  "last_motion_min_ago": 0})
        frame(client, base, pir=True, zone="bedroom", temperature=22.0, humidity=55.0,
              lux=420.0, door_open=False, last_motion_ts=base)

    elif args.type == "hum":
        print("注入：室内湿度 88%（超出 80% 阈值）")
        set_hold({"temperature": 23.0, "humidity": 88.0, "lux": 150.0,
                  "door_open": False, "pir": True, "zone": "living",
                  "last_motion_min_ago": 0})
        frame(client, now, pir=True, zone="living", temperature=23.0, humidity=88.0,
              lux=150.0, door_open=False, last_motion_ts=now)

    elif args.type == "night":
        base = now.replace(hour=2, minute=10, second=0, microsecond=0)
        print("注入：夜间 02:10 卫生间有过活动，02:13 起持续无人活动（疑似晕倒/摔倒）")
        print("期望：L3 紧急 · 夜间卫生间连续 3 分钟未检测到活动（阈值 night_bathroom_min=2）")
        set_hold({"temperature": 22.0, "humidity": 55.0, "lux": 12.0,
                  "door_open": False, "pir": False, "zone": "bathroom",
                  "last_motion_min_ago": 3})
        frame(client, base, pir=True, zone="bathroom", temperature=22.0, humidity=55.0,
              lux=12.0, door_open=False, last_motion_ts=base)
        time.sleep(0.5)
        frame(client, base + timedelta(minutes=3), pir=False, zone="bathroom",
              temperature=22.0, humidity=55.0, lux=12.0, door_open=False,
              last_motion_ts=base)

    elif args.type == "idle":
        # 日间长时间无活动：白天人在客厅，10 分钟没动静 —— 这是“晕倒/摔倒”的典型日间形态。
        # 走白天阈值 no_motion_min（默认 5 分钟），给到 10 分钟保证稳定触发 L2。
        print("注入：白天客厅已连续 10 分钟没有检测到活动（疑似晕倒/摔倒）")
        print("期望：L2 关注 · 客厅连续 10 分钟未检测到活动（阈值 no_motion_min=5）")
        set_hold({"temperature": 23.0, "humidity": 50.0, "lux": 320.0,
                  "door_open": False, "pir": False, "zone": "living",
                  "last_motion_min_ago": 11})
        # 两帧“过去时刻”的回放帧：先有人，再静默 10.5 分钟
        t_motion = now - timedelta(minutes=11)
        t_now = now - timedelta(seconds=30)
        frame(client, t_motion, pir=True, zone="living", temperature=23.0, humidity=50.0,
              lux=320.0, door_open=False, last_motion_ts=t_motion)
        time.sleep(0.3)
        frame(client, t_now, pir=False, zone="living", temperature=23.0, humidity=50.0,
              lux=320.0, door_open=False, last_motion_ts=t_motion)

    elif args.type == "door":
        base = now - timedelta(minutes=12)
        print("注入：门磁 12 分钟前开启且一直未关")
        print("期望：L1 提醒 · 入户门已持续开启 12 分钟；首页「门磁」应变成「已开」")
        set_hold({"temperature": 23.0, "humidity": 50.0, "lux": 180.0,
                  "door_open": True, "pir": False, "zone": "entry",
                  "last_motion_min_ago": 30})
        frame(client, base, pir=False, zone="entry", temperature=23.0, humidity=50.0,
              lux=180.0, door_open=True, last_motion_ts=now - timedelta(minutes=30))
        time.sleep(0.5)
        frame(client, now, pir=False, zone="entry", temperature=23.0, humidity=50.0,
              lux=180.0, door_open=True, last_motion_ts=now - timedelta(minutes=30))

    else:
        print("注入：随身按钮求助")
        client.event("SOS", now, {"zone": "living"})

    print("已注入。异常值会保持 %d 分钟（或跑 --type reset 立刻恢复）。" % (HOLD_TTL_SEC // 60))
    print("看现象：APP 首页/告警页盯 3 秒内应该有变化；"
          "若首页没动静，先确认「我的」里当前设备是 %s。" % args.device)
    return 0


if __name__ == "__main__":
    sys.exit(main())
