# -*- coding: utf-8 -*-
"""设备模拟器：在开发板到货之前，用它把网关与 APP 先跑通。

用法：
    python run_sim.py --scenario daily      # 一天的正常活动
    python run_sim.py --scenario night      # 夜间卫生间长时间无活动 -> L3
    python run_sim.py --scenario away       # 出门 -> 外出抑制
    python run_sim.py --scenario sos        # 随身按钮求助
    python run_sim.py --scenario env        # 温度超限
    python run_sim.py --loop --interval 1   # 持续发心跳，供 APP 联调

设计说明：
    每个场景是一串“相对分钟”的脚本，模拟器按脚本推进虚拟时钟。
    这样阈值（分钟级）在几秒内就能被复现，且结果确定、可重复。
"""
from __future__ import annotations

import argparse
import io
import json
import os
import random
import sys
import time
from datetime import datetime, timedelta, timezone
from urllib import error, request

TZ_OFFSET = timezone(timedelta(hours=8))

# 演示保持开关：incident.py 写入一组异常数值，模拟器照这组值持续上报，
# 既能保持异常可见，又不会让设备被判离线。
DEMO_HOLD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", ".demo_hold")
ZONES = ["living", "bedroom", "kitchen", "bathroom", "entry"]


def iso(dt):
    return dt.replace(microsecond=0).isoformat()


class SimClient(object):
    def __init__(self, base_url, device_id="bearpi-001", verbose=True):
        self.base = base_url.rstrip("/")
        self.device_id = device_id
        self.seq = 0
        self.verbose = verbose
        self.created = []

    def _post(self, path, payload):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        req = request.Request(self.base + path, data=data, method="POST",
                              headers={"Content-Type": "application/json"})
        try:
            with request.urlopen(req, timeout=5) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except error.HTTPError as exc:
            body = exc.read().decode("utf-8", "replace")
            print("  ! HTTP %s %s" % (exc.code, body))
            return None
        except Exception as exc:  # 网关没启动是最常见的情况
            print("  ! 连接失败：%s（网关起了吗？）" % exc)
            return None

    def _get(self, path):
        try:
            with request.urlopen(self.base + path, timeout=5) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except Exception:
            return None

    def snapshot(self, ts, pir=False, zone=None, temperature=None, humidity=None,
                 door_open=None, battery=98.0, last_motion_ts=None, lux=None):
        self.seq += 1
        payload = {
            "deviceId": self.device_id,
            "ts": iso(ts),
            "seq": self.seq,
            "pirDetected": bool(pir),
            # 真实固件会一直保留“最后一次活动时间”，只有从未触发过才是 null。
            # 这里同样保持，否则 APP 首页的“最后活动”会在数值和“—”之间闪。
            "lastMotionTs": iso(last_motion_ts) if last_motion_ts is not None
                            else (iso(ts) if pir else None),
            "lastMotionZone": zone,
            "temperature": temperature,
            "humidity": humidity,
            "lux": lux,
            "doorOpen": door_open,
            "battery": battery,
            "powerState": "mains",
            "rssi": -52,
            "online": True,
            "source": "key_sim",       # 模拟器一律标 key_sim，界面会显式提示
            "fwVersion": "sim-1.0",
        }
        result = self._post("/api/v1/snapshot", payload)
        if result and result.get("alarms"):
            self.created.extend(result["alarms"])
            print("  -> 生成告警 %s" % ", ".join(result["alarms"]))
        return result

    def _has_pending(self):
        import json as _json
        try:
            with request.urlopen(self.base + '/api/v1/alarms?status=pending', timeout=3) as resp:
                return len(_json.loads(resp.read().decode('utf-8')).get('items', [])) > 0
        except Exception:
            return True

    def _close_pending(self, reason="演示重置，关闭历史告警"):
        """把当前所有待确认告警关掉（演示重置 / 可重复注入用）。"""
        import json as _json
        try:
            with request.urlopen(self.base + "/api/v1/alarms?status=pending", timeout=5) as resp:
                items = _json.loads(resp.read().decode("utf-8")).get("items", [])
        except Exception:
            return 0
        closed = 0
        for alarm in items:
            body = _json.dumps({"handleType": "other",
                                "handleResult": reason}).encode("utf-8")
            req = request.Request(self.base + "/api/v1/alarms/%s/close" % alarm["eventId"],
                                  data=body, method="POST",
                                  headers={"Content-Type": "application/json"})
            try:
                request.urlopen(req, timeout=5).read()
                closed += 1
            except Exception:
                pass
        return closed

    def event(self, etype, ts, payload=None):
        body = {"deviceId": self.device_id, "ts": iso(ts), "type": etype,
                "source": "key_sim", "payload": payload or {}}
        result = self._post("/api/v1/events", body)
        if result and result.get("created"):
            self.created.append(result["created"]["eventId"])
            print("  -> 生成告警 %s (%s)" % (result["created"]["eventId"],
                                             result["created"]["message"]))
        return result


def base_time(hhmm):
    """以今天为基准构造起始时间，支持 02:10 这种夜间场景。"""
    hour, minute = [int(x) for x in hhmm.split(":")]
    now = datetime.now(TZ_OFFSET)
    return now.replace(hour=hour, minute=minute, second=0, microsecond=0)


# ----------------------------------------------------------------------
# 场景脚本：(相对分钟, 动作)
# ----------------------------------------------------------------------
def scenario_daily(client, start):
    print("场景：正常的一天")
    for offset, zone in [(0, "bedroom"), (2, "living"), (5, "kitchen"), (7, "living")]:
        client.snapshot(start + timedelta(minutes=offset), pir=True, zone=zone,
                        temperature=23.4, humidity=51, door_open=False)
        time.sleep(0.4)


def scenario_night(client, start):
    print("场景：夜间卫生间长时间无活动（预期 L3）")
    client.snapshot(start, pir=True, zone="bathroom", temperature=22.1, humidity=55, door_open=False)
    time.sleep(0.4)
    client.snapshot(start + timedelta(minutes=3), pir=False, zone="bathroom",
                    temperature=22.0, humidity=55, door_open=False)
    time.sleep(0.4)
    print("预期结果：L3「夜间卫生间连续 3 分钟未检测到活动」")


def scenario_away(client, start):
    print("场景：出门（预期：不产生无活动告警，仅在超时后提醒）")
    client.snapshot(start, pir=True, zone="living", temperature=24.0, humidity=50, door_open=False)
    time.sleep(0.3)
    client.snapshot(start + timedelta(minutes=1), pir=False, zone="entry", door_open=True)
    time.sleep(0.3)
    client.snapshot(start + timedelta(minutes=2), pir=False, zone="entry", door_open=False)
    time.sleep(0.3)
    client.snapshot(start + timedelta(minutes=30), pir=False, door_open=False)
    time.sleep(0.3)
    print("预期结果：此时不应出现 NO_MOTION（外出抑制生效）")


def scenario_sos(client, start):
    print("场景：随身按钮求助（预期 L3）")
    client.event("SOS", start, {"zone": "living"})
    print("预期结果：L3「紧急求助」")


def scenario_env(client, start):
    print("场景：室内温度超限（预期 L1）")
    client.snapshot(start, pir=True, zone="living", temperature=33.5, humidity=72, door_open=False)
    print("预期结果：L1「室内温度 33.5℃，高于阈值 30.0℃」")


def scenario_loop(client, start, interval=1.0, duration=600):
    print("持续心跳模式（Ctrl+C 停止）：每 %.1fs 一帧，数值缓慢变化" % interval)
    begin = time.time()
    temp = 23.0
    hum = 50.0
    battery = 95.0
    last_motion = None
    last_zone = None   # 最后活动区域，和 lastMotionTs 一样要保持
    burst = 0          # 剩余“活动帧”数，模拟人在屋里走动一小会儿
    frames = 0         # 已发帧数，用于定期打印“我还活着”

    while time.time() - begin < duration:
        now = datetime.now(TZ_OFFSET)

        # 有保持文件时，用注入的数值持续上报（设备保持在线，异常保持可见）
        if os.path.exists(DEMO_HOLD):
            try:
                with io.open(DEMO_HOLD, encoding="utf-8") as f:
                    hold = json.load(f)
            except Exception:
                hold = None
            # 保持文件的两种退出：手工 --type reset，或到期（incident.py 写入 expires_at）。
            # 不再用“没有待确认告警”来判断 —— 注入瞬间告警尚未创建，会误清。
            if hold:
                exp = hold.get("expires_at")
                if exp and time.time() > float(exp):
                    try:
                        os.remove(DEMO_HOLD)
                    except OSError:
                        pass
                    hold = None
                    print("[demo] 注入数据已到期，恢复正常上报")

            if hold:
                ago = int(hold.get("last_motion_min_ago", 0))
                lm = now - timedelta(minutes=ago)
                client.snapshot(now,
                                pir=bool(hold.get("pir", True)),
                                zone=hold.get("zone"),
                                temperature=hold.get("temperature"),
                                humidity=hold.get("humidity"),
                                lux=hold.get("lux"),
                                door_open=hold.get("door_open"),
                                battery=round(battery, 1),
                                last_motion_ts=lm)
                time.sleep(interval)
                continue
        zone = random.choice(ZONES[:-1])

        # 爆发模型：安静一段时间后，连续 2~4 帧有活动。
        # 比“每帧独立 35%”更接近真实，也不会让“最后活动”每几秒就归零。
        if burst == 0 and random.random() < 0.06:
            burst = random.randint(2, 4)
        pir = burst > 0
        if pir:
            burst -= 1
            last_motion = now
            last_zone = zone

        # 随机游走而不是每帧重新随机：曲线连续，看起来才像真实传感器
        temp = min(26.0, max(21.0, temp + random.uniform(-0.2, 0.2)))
        hum = min(60.0, max(45.0, hum + random.uniform(-0.5, 0.5)))
        battery = max(80.0, battery - 0.01)

        lux = 180.0 if 6 <= now.hour < 18 else 12.0
        client.snapshot(now, pir=pir, zone=last_zone,
                        temperature=round(temp, 1),
                        humidity=round(hum, 0),
                        lux=lux,
                        door_open=False,
                        battery=round(battery, 1),
                        last_motion_ts=last_motion)

        frames += 1
        if frames % 10 == 0:
            print("[%s] 第 %d 帧  温度 %.1f 度  湿度 %.0f%%  光照 %.0f lx  pir=%s"
                  % (now.strftime("%H:%M:%S"), frames, temp, hum, lux, pir))
        time.sleep(interval)


SCENARIOS = {
    "daily": scenario_daily,
    "night": scenario_night,
    "away": scenario_away,
    "sos": scenario_sos,
    "env": scenario_env,
}


def main(argv=None):
    parser = argparse.ArgumentParser(description="守夜灯 · 设备模拟器")
    parser.add_argument("--url", default="http://127.0.0.1:8780", help="网关地址")
    parser.add_argument("--device", default="bearpi-001", help="设备编号")
    parser.add_argument("--scenario", default="daily", choices=sorted(SCENARIOS) + ["loop"])
    parser.add_argument("--loop", action="store_true",
                        help="等价于 --scenario loop，保持心跳持续输出")
    parser.add_argument("--base", default="12:00", help="场景起始时间 HH:MM，夜间场景用 02:10")
    parser.add_argument("--interval", type=float, default=1.0, help="loop 模式每帧间隔（秒）")
    parser.add_argument("--duration", type=float, default=600, help="loop 模式持续时长（秒）")
    args = parser.parse_args(argv)

    scenario = "loop" if args.loop else args.scenario

    client = SimClient(args.url, args.device)
    health = client._get("/health")
    if not health:
        print("无法连接网关 %s —— 先运行：python run_gateway.py --lan" % args.url)
        return 1
    print("网关在线：uptime=%ss version=%s" % (health.get("uptime"), health.get("version")))

    start = base_time(args.base)
    if scenario == "loop":
        try:
            scenario_loop(client, start, args.interval, args.duration)
        except KeyboardInterrupt:
            pass
    else:
        SCENARIOS[scenario](client, start)

    print("完成。本次共触发 %d 条告警。" % len(client.created))
    return 0


if __name__ == "__main__":
    sys.exit(main())
