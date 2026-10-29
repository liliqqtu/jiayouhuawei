# -*- coding: utf-8 -*-
"""模拟实验：在无硬件条件下，把整条链路按顺序跑一遍并打印每步结果。

用法（网关已启动时）：
    python scripts/lab_demo.py --base http://127.0.0.1:8780 --udp 9998
"""
import argparse
import json
import socket
import sys
import time
from datetime import datetime, timedelta, timezone
from urllib import error, request

TZ = timezone(timedelta(hours=8))


def call(base, method, path, body=None):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = request.Request(base + path, data=data, method=method,
                          headers={"Content-Type": "application/json"})
    try:
        with request.urlopen(req, timeout=5) as resp:
            return resp.status, json.loads(resp.read().decode("utf-8") or "{}")
    except error.HTTPError as exc:
        return exc.code, json.loads(exc.read().decode("utf-8") or "{}")
    except error.URLError as exc:
        # 连接被拒 / 网关没起：返回 0，由调用方给出人话提示
        return 0, {"error": str(exc.reason)}


def iso(dt):
    return dt.replace(microsecond=0).isoformat()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="http://127.0.0.1:8780")
    parser.add_argument("--udp", type=int, default=9998)
    parser.add_argument("--source", default="test", choices=["test", "hardware"],
                        help="模拟帧的来源标记。默认 test（本来就是模拟数据）；"
                             "要验证 APP 的“真实硬件”角标时传 hardware")
    args = parser.parse_args()
    base = args.base
    # 每次运行换一个 seq 段，避免与上一次实验的记录冲突
    seq = int(time.time()) % 100000 * 10
    now = datetime.now(TZ)
    print("本次运行 deviceId=lab-001  seq 起始=%d  source=%s" % (seq, args.source))

    # 等待网关就绪：最多 15 秒，避免"窗口 A 还没起来就报错"
    ready = False
    for attempt in range(15):
        status, _ = call(base, "GET", "/health")
        if status == 200:
            ready = True
            break
        if attempt == 0:
            print("等待网关就绪 …（最多 15 秒）")
        time.sleep(1)
    if not ready:
        print("")
        print("=" * 68)
        print("连不上网关：%s" % base)
        print("请先在 **另一个窗口** 执行：")
        print("    cd D:\\shadowlamp")
        print("    python run_gateway.py --port 8780 --db data/lab.db --udp-port 9998")
        print("看到“监听 : 0.0.0.0:8780”后再回来跑本脚本。")
        print("=" * 68)
        return 2

    def step(n, title):
        print("\n" + "=" * 68)
        print("STEP %d  %s" % (n, title))
        print("=" * 68)

    # ---------------- 1 健康检查
    step(1, "网关自检")
    status, body = call(base, "GET", "/health")
    print("GET /health ->", status, body)

    # ---------------- 2 手工上报一帧（模拟开发板）
    step(2, "手工上报一帧（等价于开发板的一次心跳）")
    seq += 1
    snap = {
        "deviceId": "lab-001",
        "ts": iso(now),
        "seq": seq,
        "pirDetected": True,
        "lastMotionTs": iso(now),
        "lastMotionZone": "living",
        "temperature": 23.4,
        "humidity": 51.0,
        "lux": 120.0,
        "doorOpen": False,
        "battery": None,
        "powerState": "mains",
        "rssi": -52,
        "online": True,
        "source": args.source,
        "fwVersion": "lab-0.1",
    }
    print("POST /api/v1/snapshot  body =", json.dumps(snap, ensure_ascii=False)[:120], "...")
    status, body = call(base, "POST", "/api/v1/snapshot", snap)
    print("-> ", status, body)

    # ---------------- 3 查询
    step(3, "APP 首页会看到什么")
    status, s = call(base, "GET", "/api/v1/snapshot")
    print("GET /api/v1/snapshot ->", status)
    for k in ("deviceId", "ts", "seq", "pirDetected", "temperature", "humidity",
              "lux", "doorOpen", "source", "online", "staleSeconds"):
        print("   %-14s %s" % (k, s.get(k)))
    status, d = call(base, "GET", "/api/v1/device")
    print("GET /api/v1/device  -> online=%s powerState=%s" % (d.get("online"), d.get("powerState")))

    # ---------------- 4 触发无活动告警（夜间卫生间 L3）
    step(4, "触发 L3：夜间卫生间长时间无活动")
    night = now.replace(hour=2, minute=10, second=0)
    seq += 1
    call(base, "POST", "/api/v1/snapshot", {
        "deviceId": "lab-001", "ts": iso(night), "seq": seq,
        "pirDetected": True, "lastMotionTs": iso(night),
        "lastMotionZone": "bathroom", "doorOpen": False,
        "source": args.source, "fwVersion": "lab-0.1",
    })
    seq += 1
    status, body = call(base, "POST", "/api/v1/snapshot", {
        "deviceId": "lab-001", "ts": iso(night + timedelta(minutes=3)), "seq": seq,
        "pirDetected": False, "lastMotionTs": iso(night),
        "lastMotionZone": "bathroom", "doorOpen": False,
        "source": args.source, "fwVersion": "lab-0.1",
    })
    print("两帧上报后 ->", status, body)

    # ---------------- 5 告警列表
    step(5, "APP 告警中心会看到什么")
    status, alarms = call(base, "GET", "/api/v1/alarms?status=pending")
    for a in alarms.get("items", [])[:3]:
        print("   %s  %s  %s  %s" % (a["eventId"], a["level"], a["status"], a["message"]))
    event_id = alarms["items"][0]["eventId"] if alarms.get("items") else None

    # ---------------- 6 闭环
    if event_id:
        step(6, "事件闭环：确认 -> 处置 -> 关闭")
        status, body = call(base, "POST", "/api/v1/alarms/%s/ack" % event_id, {"operator": "daughter"})
        print("ack   ->", status, "status=", body.get("status"), "ackTs=", body.get("ackTs"))
        status, body = call(base, "POST", "/api/v1/alarms/%s/close" % event_id,
                            {"handleType": "visited", "handleResult": "已上门查看，老人安好"})
        print("close ->", status, "status=", body.get("status"),
              "handleType=", body.get("handleType"))

        step(7, "重复关闭应被拒绝（状态机校验）")
        status, body = call(base, "POST", "/api/v1/alarms/%s/close" % event_id,
                            {"handleType": "visited", "handleResult": "再来一次"})
        print("close again ->", status, body)

    # ---------------- 8 日报
    step(8, "报告页数据")
    day = now.date().isoformat()
    status, rep = call(base, "GET", "/api/v1/reports/daily?date=%s" % day)
    print("totalCount=%s  alarmCount=%s  level=%s" % (
        rep.get("totalCount"), rep.get("alarmCount"), rep.get("level")))
    print("hourly(前8小时) =", rep.get("hourly", [])[:8])

    # ---------------- 9 UDP 通道
    step(9, "切换到 Wi-Fi UDP 通道")
    seq += 1
    payload = {
        "deviceId": "lab-udp", "ts": iso(datetime.now(TZ)), "seq": seq,
        "pirDetected": True, "lastMotionZone": "kitchen",
        "source": "test", "fwVersion": "lab-0.1",
    }
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    host = base.split("//")[1].split(":")[0]
    sock.sendto(json.dumps(payload).encode("utf-8"), (host, args.udp))
    sock.close()
    time.sleep(0.6)
    status, s = call(base, "GET", "/api/v1/snapshot")
    print("UDP 一帧后最新快照: deviceId=%s source=%s seq=%s"
          % (s.get("deviceId"), s.get("source"), s.get("seq")))
    print("（source=test 是故意标成测试数据，APP 会显示“测试/模拟”）")

    # ---------------- 10 非法报文
    step(10, "非法报文不应导致崩溃")
    status, body = call(base, "POST", "/api/v1/snapshot", {"deviceId": "bad"})
    print("缺字段 ->", status, body)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(b"{this is not json", (host, args.udp))
    sock.close()
    time.sleep(0.4)
    status, body = call(base, "GET", "/health")
    print("发完非法包后 /health ->", status, body.get("ok"))

    print("\n" + "=" * 68)
    print("实验完成。以上输出可直接作为实验记录截图。")
    print("=" * 68)


if __name__ == "__main__":
    main()
