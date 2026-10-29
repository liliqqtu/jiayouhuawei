# -*- coding: utf-8 -*-
"""数据接入层：UDP 直收 + 串口桥接 + 来源归一化。

三种通道共用同一套校验与入库逻辑（Api.ingest_snapshot），
避免"HTTP 能过、UDP 不能过"这种两套规则的问题。

来源归一化：
    需求规定只有两种来源 —— hardware（真实硬件）/ test（模拟、按键演示）。
    为兼容早期报文，real/key_sim/history 一律映射过去，绝不把 test 洗成 hardware。
"""
from __future__ import annotations

import json
import socket
import threading
import time

from gateway import util

HARDWARE_ALIASES = ("hardware", "real")
MAX_DATAGRAM = 4096
JSON_MARK = "#JSON#"


def normalize_source(value):
    """只允许 hardware / test；其它（含缺失）一律按 test 处理。"""
    if isinstance(value, str) and value.strip().lower() in HARDWARE_ALIASES:
        return "hardware"
    return "test"


def parse_payload(text):
    """返回 (dict, err)。非法报文只报错，不抛异常。"""
    if not isinstance(text, str):
        return None, "payload is not text"
    raw = text.strip()
    if not raw:
        return None, "empty payload"

    # 串口行里可能混着调试日志，截取最外层花括号
    start = raw.find("{")
    end = raw.rfind("}")
    if start >= 0 and end > start:
        raw = raw[start:end + 1]

    try:
        data = json.loads(raw)
    except ValueError as exc:
        return None, "invalid json: %s" % exc
    if not isinstance(data, dict):
        return None, "payload is not a json object"
    return data, ""


def normalize_telemetry_payload(payload, channel="udp"):
    """将开发板发出的精简遥测报文或标准快照统一转换为网关规范结构。"""
    if not isinstance(payload, dict):
        return payload

    if payload.get("type") == "telemetry":
        pir = bool(payload.get("motion", 0))
        door = bool(payload.get("door", 0))
        temp = util.as_float(payload.get("temp")) or 25.0
        humi = util.as_float(payload.get("humi")) or 50.0
        uptime = util.as_int(payload.get("uptime")) or 1
        device_id = str(payload.get("deviceId") or "bearpi-001")
        power_state = "battery" if channel == "udp" else "mains"

        normalized = {
            "deviceId": device_id,
            "ts": payload.get("ts") or util.iso_now(),
            "seq": uptime,
            "pirDetected": pir,
            "lastMotionTs": util.iso_now() if pir else None,
            "lastMotionZone": "living",
            "temperature": round(temp, 1),
            "humidity": round(humi, 1),
            "lux": 150.0,
            "doorOpen": door,
            "battery": 95.0,
            "powerState": power_state,
            "rssi": -48,
            "online": True,
            "source": "hardware",
            "fwVersion": "bearpi-1.0.0",
            "sos": util.as_int(payload.get("sos", 0)),
            "alert": payload.get("alert"),
            "level": payload.get("level"),
        }
        return normalized

    # 兜底默认字段
    if "source" not in payload:
        payload["source"] = "hardware"
    if "ts" not in payload:
        payload["ts"] = util.iso_now()
    if "seq" not in payload:
        payload["seq"] = 1
    if "deviceId" not in payload:
        payload["deviceId"] = "bearpi-001"
    return payload


def extract_serial_json(line):
    """从一行串口输出里取 JSON；没有则返回空串。"""
    if not line:
        return ""
    if JSON_MARK in line:
        line = line.split(JSON_MARK, 1)[1]
    line = line.strip()
    if not line.startswith("{"):
        return ""
    end = line.rfind("}")
    return line[:end + 1] if end > 0 else ""


class UdpIngest(threading.Thread):
    """Wi-Fi 模式：开发板把 JSON 直接 UDP 打到网关。"""

    def __init__(self, api, host="0.0.0.0", port=9999, logger=None):
        threading.Thread.__init__(self, daemon=True, name="UdpIngest")
        self.api = api
        self.host = host
        self.port = port
        self.log = logger or (lambda msg: None)
        self.stop_event = threading.Event()
        self.sock = None
        self.received = 0
        self.dropped = 0

    def run(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.settimeout(0.5)
        try:
            self.sock.bind((self.host, self.port))
        except OSError as exc:
            self.log("[udp] 绑定失败 %s:%s -> %s" % (self.host, self.port, exc))
            return
        self.log("[udp] 监听 %s:%d（Wi-Fi 模式）" % (self.host, self.port))

        while not self.stop_event.is_set():
            try:
                data, addr = self.sock.recvfrom(MAX_DATAGRAM)
            except socket.timeout:
                continue
            except OSError:
                break

            self.received += 1
            text = data.decode("utf-8", "replace")
            payload, err = parse_payload(text)
            if payload is None:
                self.dropped += 1
                self.log("[udp] 丢弃来自 %s 的非法报文：%s" % (addr[0], err))
                continue

            payload = normalize_telemetry_payload(payload, channel="udp")
            if payload.get("sos") == 1:
                try:
                    self.api.engine.on_event({
                        "deviceId": payload.get("deviceId", "bearpi-001"),
                        "ts": payload.get("ts") or util.iso_now(),
                        "type": "SOS",
                        "source": "hardware",
                        "payload": {"zone": "living", "trigger": "f1_key", "channel": "udp_wifi"}
                    })
                except Exception as e:
                    self.log("[udp] SOS 事件触发异常: %s" % e)

            try:
                result = self.api.ingest_snapshot(payload, channel="udp")
                self.log("[udp] %s <- %s seq=%s alarms=%d"
                         % (payload.get("deviceId", "?"), addr[0],
                            result.get("seq"), len(result.get("alarms", []))))
            except Exception as exc:          # 单包异常不影响继续收
                self.dropped += 1
                self.log("[udp] 处理异常：%s" % exc)

    def stop(self):
        self.stop_event.set()
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass


class SerialIngest(threading.Thread):
    """USB 串口桥接模式：解析固件 printf 出来的 JSON 行。

    不依赖 pyserial：Windows 下直接用 `\\\\.\\COM3` 打开设备文件即可。
    若环境里有 pyserial 则优先用（波特率设置更可靠）。
    """

    def __init__(self, api, port="COM3", baud=115200, logger=None):
        threading.Thread.__init__(self, daemon=True, name="SerialIngest")
        self.api = api
        self.port = port
        self.baud = baud
        self.log = logger or (lambda msg: None)
        self.stop_event = threading.Event()
        self.received = 0
        self.dropped = 0

    def _open(self):
        try:
            import serial  # type: ignore
            return serial.Serial(self.port, self.baud, timeout=0.5)
        except ImportError:
            pass
        # 退回标准库：先用 mode 命令设置波特率，再按文件打开
        import subprocess
        try:
            subprocess.run(["mode", self.port, "BAUD=%d" % self.baud,
                            "PARITY=N", "DATA=8", "STOP=1"],
                           capture_output=True, timeout=5)
        except Exception:
            pass
        return open("\\\\.\\%s" % self.port, "rb", buffering=0)

    def run(self):
        try:
            handle = self._open()
        except Exception as exc:
            self.log("[serial] 打开 %s 失败：%s" % (self.port, exc))
            return
        self.log("[serial] 已连接 %s @%d（串口桥接模式）" % (self.port, self.baud))

        buf = b""
        while not self.stop_event.is_set():
            try:
                chunk = handle.read(256) if hasattr(handle, "read") else b""
            except Exception as exc:
                self.log("[serial] 读取异常：%s" % exc)
                break
            if not chunk:
                time.sleep(0.05)
                continue

            buf += chunk
            while b"\n" in buf:
                line_bytes, buf = buf.split(b"\n", 1)
                line = line_bytes.decode("utf-8", "replace").strip("\r")
                if not line:
                    continue
                body = extract_serial_json(line)
                if not body:
                    continue          # 普通调试日志，跳过
                self.received += 1
                payload, err = parse_payload(body)
                if payload is None:
                    self.dropped += 1
                    self.log("[serial] 丢弃非法 JSON：%s" % err)
                    continue

                payload = normalize_telemetry_payload(payload, channel="serial")
                if payload.get("sos") == 1:
                    try:
                        self.api.engine.on_event({
                            "deviceId": payload.get("deviceId", "bearpi-001"),
                            "ts": payload.get("ts") or util.iso_now(),
                            "type": "SOS",
                            "source": "hardware",
                            "payload": {"zone": "living", "trigger": "f1_key", "channel": "serial_usb"}
                        })
                    except Exception as e:
                        self.log("[serial] SOS 事件触发异常: %s" % e)

                try:
                    self.api.ingest_snapshot(payload, channel="serial")
                except Exception as exc:
                    self.dropped += 1
                    self.log("[serial] 处理异常：%s" % exc)

        try:
            handle.close()
        except Exception:
            pass

    def stop(self):
        self.stop_event.set()
