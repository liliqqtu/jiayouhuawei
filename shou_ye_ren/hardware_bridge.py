#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
小熊派硬件数据 (CH340 / COM7) -> 守夜人网关 (8780) -> 手机鸿蒙 APP 实时桥接器
"""

import sys
import time
import json
import socket
from datetime import datetime, timezone, timedelta
from urllib import request, error

# 保证无论在 pythonw 无窗口后台 还是 CMD 窗口下都不崩溃
if sys.stdout is None:
    class DummyStream:
        def write(self, *args, **kwargs): pass
        def flush(self, *args, **kwargs): pass
    sys.stdout = DummyStream()
    sys.stderr = DummyStream()
elif sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

TZ_OFFSET = timezone(timedelta(hours=8))
GATEWAY_URL = "http://127.0.0.1:8780/api/v1"
DEVICE_ID = "bearpi-001"

def iso_now():
    return datetime.now(TZ_OFFSET).replace(microsecond=0).isoformat()

def post_snapshot(payload):
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = request.Request(f"{GATEWAY_URL}/snapshot", data=data, method="POST",
                          headers={"Content-Type": "application/json"})
    try:
        with request.urlopen(req, timeout=3) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        print(f"[Bridge Error] 上报网关失败: {e}")
        return None

def get_latest_snapshot():
    req = request.Request(f"{GATEWAY_URL}/snapshot", method="GET")
    try:
        with request.urlopen(req, timeout=1.5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception:
        return None

def post_event(event_type, payload=None):
    body = {
        "deviceId": DEVICE_ID,
        "ts": iso_now(),
        "type": event_type,
        "source": "hardware",
        "payload": payload or {}
    }
    data = json.dumps(body, ensure_ascii=False).encode("utf-8")
    req = request.Request(f"{GATEWAY_URL}/events", data=data, method="POST",
                          headers={"Content-Type": "application/json"})
    try:
        with request.urlopen(req, timeout=3) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        print(f"[Bridge Error] 事件上报失败: {e}")
        return None

def find_bearpi_port():
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        # 1. 优先匹配 CH340 或 USB 串口
        for p in ports:
            desc = (p.description or "").upper()
            hwid = (p.hwid or "").upper()
            if "BTHENUM" in hwid or "BLUETOOTH" in desc:
                continue
            if "CH340" in desc or "CH340" in hwid or "1A86:7523" in hwid or "USB-SERIAL" in desc:
                return p.device
        # 2. 如果明确指定了 COM7
        for p in ports:
            if p.device.upper() == "COM7":
                return p.device
        # 3. 其它非蓝牙串口
        for p in ports:
            desc = (p.description or "").upper()
            hwid = (p.hwid or "").upper()
            if "BTHENUM" in hwid or "BLUETOOTH" in desc:
                continue
            if p.device.upper().startswith("COM"):
                return p.device
    except Exception as e:
        print(f"[Bridge] 串口枚举异常: {e}")
    return None

def main():
    print("=" * 65)
    print("  独居老人居家安全系统 · 小熊派硬件 -> 手机端实时数据桥接器")
    print(f"  目标网关: {GATEWAY_URL}")
    print("=" * 65)

    target_port = find_bearpi_port()
    ser = None
    if target_port:
        try:
            import serial
            ser = serial.Serial(target_port, 115200, timeout=0.8)
            print(f"[成功] 连接小熊派开发板串口: {target_port} (波特率 115200)")
        except Exception as e:
            print(f"[提示] 打开串口 {target_port} 异常: {e}")
    else:
        print("[提示] 未检测到小熊派 USB 串口 (CH340)，进入自动扫描与守护模式...")

    seq = 0
    temp = 24.5
    humi = 53.0
    pir = False
    door = False
    last_motion_ts = iso_now()
    last_sos_state = 0

    while True:
        seq += 1

        # 如果串口未打开，尝试重新发现与打开
        if ser is None:
            new_port = find_bearpi_port()
            if new_port:
                try:
                    import serial
                    ser = serial.Serial(new_port, 115200, timeout=0.8)
                    print(f"[成功] 重新检测并连接到串口: {new_port}")
                except Exception:
                    ser = None

        # 若无串口连接，检测小熊派是否正在通过无线 Wi-Fi (UDP 9999) 独立供电上报
        if ser is None:
            latest = get_latest_snapshot()
            if latest and latest.get("source") == "hardware":
                tag = "[无线 Wi-Fi 模式]"
                alarms = latest.get("alarms") or []
                alarm_str = f" [告警: {', '.join(alarms)}]" if alarms else ""
                print(f"[{iso_now()}] {tag} 接收到小熊派无线数据 (UDP:9999): 温度={latest.get('temperature')}℃, 门磁={'开门' if latest.get('doorOpen') else '关闭'}, 人体={'有人' if latest.get('pirDetected') else '静止'}{alarm_str}")
                time.sleep(1)
                continue

        # 从串口读取数据
        if ser:
            try:
                line = ser.readline().decode("utf-8", "ignore").strip()
                if line.startswith("{") and line.endswith("}"):
                    data = json.loads(line)
                    if data.get("type") == "telemetry":
                        pir = bool(data.get("motion", 0))
                        door = bool(data.get("door", 0))
                        temp = float(data.get("temp", temp))
                        humi = float(data.get("humi", humi))
                        sos_val = int(data.get("sos", 0))
                        
                        if pir:
                            last_motion_ts = iso_now()
                            
                        # SOS 按键边缘触发（按下时上报）
                        if sos_val == 1 and last_sos_state == 0:
                            post_event("SOS", {"zone": "living", "trigger": "f1_key"})
                            print(">>> [SOS 紧急求助] 检测到小熊派板载按键按下！已上报手机端！ <<<")
                        last_sos_state = sos_val
            except Exception as e:
                print(f"[提示] 串口通讯暂断，自动重连中... ({e})")
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None

        # 封装为网关标准 snapshot 结构
        payload = {
            "deviceId": DEVICE_ID,
            "ts": iso_now(),
            "seq": seq,
            "pirDetected": pir,
            "lastMotionTs": last_motion_ts,
            "lastMotionZone": "living",
            "temperature": round(temp, 1),
            "humidity": round(humi, 1),
            "lux": 150.0,
            "doorOpen": door,
            "battery": 98.0,
            "powerState": "mains",
            "rssi": -48,
            "online": True,
            "source": "hardware" if ser else "simulation",
            "fwVersion": "bearpi-1.0.0"
        }

        res = post_snapshot(payload)
        if res:
            alarms = res.get("alarms", [])
            alarm_str = f" [告警: {', '.join(alarms)}]" if alarms else ""
            tag = "[真实硬件 COM7]" if ser else "[智能模拟]"
            print(f"[{iso_now()}] {tag} 帧#{seq} 同步成功: 温度={temp:.1f}℃, 门磁={'开门' if door else '关闭'}, 人体={'有人' if pir else '静止'}{alarm_str}")

        time.sleep(1)

if __name__ == "__main__":
    main()
