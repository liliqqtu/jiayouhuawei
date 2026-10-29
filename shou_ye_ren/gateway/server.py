# -*- coding: utf-8 -*-
"""HTTP 服务入口：ThreadingHTTPServer + JSON 路由。

只依赖标准库，Python 3.8 可直接运行：
    python run_gateway.py --port 8780
"""
from __future__ import annotations

import argparse
import json
import socket
import sys
import threading
import traceback
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from gateway import util
from gateway.api import Api, ApiError
from gateway import dashboard
from gateway import siren_wav
from gateway.ingest import SerialIngest, UdpIngest
from gateway.config import DEFAULT_DB, DEFAULT_HOST, DEFAULT_PORT, check_python_version
from gateway.store import Store

MAX_BODY = 1024 * 1024


LEVEL_ORDER = {"L3": 3, "L2": 2, "L1": 1}
# 每轮播报的间隔（秒）：L3 是 6 秒扫频警笛，放完隔 1 秒立刻重来，接近连续；
# L2 三声提示音约 1.5 秒，间隔 5 秒；L1 两声轻提示约 0.8 秒，间隔 9 秒。
LEVEL_INTERVAL = {"L3": 1, "L2": 5, "L1": 9}
# 报警音本身的时长（秒）：用来决定这一轮要等多久再进入下一轮
TONE_SECONDS = {"L3": 6.0, "L2": 1.5, "L1": 0.8}


class SirenThread(threading.Thread):
    """分级报警循环：先报最严重的，级别不同则节奏与话术都不同。

    L3 紧急：三声急促 + "紧急告警，……请立即处理"（语速偏快）
    L2 关注：三声中速 + "注意，……请尽快确认"
    L1 提醒：两声轻响 + "提醒，……"（语速偏慢）
    """

    def __init__(self, api, logger=None):
        threading.Thread.__init__(self, daemon=True, name="SirenThread")
        self.api = api
        self.log = logger or (lambda msg: None)
        self.stop_event = threading.Event()
        self.rotate = 0
        # 启动基线：启动前就已经存在的待确认告警属于"历史"，不自动播报，
        # 否则一启动就响个不停（演示/现场都很吵）。要立刻播报请跑 scripts\siren.py on。
        self.start_iso = util.iso_now()
        self.arm_all = False      # True = 连历史待确认也播（siren.py on）
        self.muted = False        # True = 完全静音（siren.py off）
        self.ringing = False
        self._history_logged = False
        try:
            self.wavs = siren_wav.build()
        except Exception:
            self.wavs = {}
        self.looping = False

    # ---------------- 声音 ----------------
    def _beep(self, freq, ms, volume_scale=1.0):
        try:
            import winsound
            winsound.Beep(int(freq), int(ms))
        except Exception:
            pass

    def _pattern(self, level):
        """不同等级的报警声，响度差别要明显，并且**每一轮都要重新响**。

        历史 bug：早先给 L3 加了 `if not self.looping` 只放一次，
        结果 L3 只在第一轮响，后面每轮只剩语音；测试里表现为
        “按了求助只蜂鸣一次、夜间告警隔很久才再响”。
        """
        if not self.wavs:
            # 兜底：WAV 不可用时退回系统蜂鸣
            if level == "L3":
                for _ in range(3):
                    siren_wav.beep(1700, 150)
            elif level == "L2":
                siren_wav.beep(1200, 250)
            else:
                siren_wav.beep(800, 200)
            return

        siren_wav.play_once(level, self.wavs)

    def _silence(self):
        siren_wav.stop()
        self.looping = False

    def _speak(self, level, message):
        """语音播报：后台进程播放，失败/超时都不拖住警笛循环。"""
        if not getattr(self.api, "voice", False):
            return
        prefix = {
            "L3": "紧急告警，",
            "L2": "注意，",
            "L1": "提醒，",
        }.get(level, "")
        tail = {
            "L3": "，请立即处理",
            "L2": "，请尽快确认",
            "L1": "",
        }.get(level, "")
        rate = {"L3": 1, "L2": 0, "L1": -2}.get(level, 0)
        text = "%s%s%s" % (prefix, message, tail)
        try:
            import subprocess
            clean = text.replace("'", "").replace('"', "")
            script = ("$v = New-Object -ComObject SAPI.SpVoice; "
                      "$v.Rate = %d; $v.Speak('%s')" % (rate, clean))
            # 上一句没说完就掐掉，避免语音排队、越拖越久
            old = getattr(self, "_voice_proc", None)
            if old is not None and old.poll() is None:
                try:
                    old.kill()
                except Exception:
                    pass
            self._voice_proc = subprocess.Popen(
                ["powershell", "-NoProfile", "-Command", script],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

    # ---------------- 主循环 ----------------
    def _next_alarm(self):
        """按等级排序后轮流取，保证最严重的先报、且都能轮到。

        返回 (要播的那条告警, 本轮参与轮转的告警列表)；本轮没得播时告警为 None。
        """
        try:
            pending = self.api.store.list_alarms(status="pending", limit=30)
        except Exception:
            return None, []
        if not pending or self.muted:
            return None, pending

        if self.arm_all:
            playable = list(pending)
        else:
            base = util.parse_ts(self.start_iso)
            playable = []
            for item in pending:
                when = util.parse_ts(item.get("createdTs") or item.get("ts"))
                if base and when and when >= base:
                    playable.append(item)

        if not playable:
            if not self._history_logged and pending:
                self._history_logged = True
                self.log("[siren] 有 %d 条历史待确认告警，启动时不自动播报"
                         "（要立即提示：python scripts\siren.py on）" % len(pending))
            return None, pending

        self._history_logged = False
        playable.sort(key=lambda a: (-LEVEL_ORDER.get(a["level"], 0), a["ts"]))
        alarm = playable[self.rotate % len(playable)]
        self.rotate += 1
        return alarm, playable

    # ---------------- 运行期开关（启动与报警分开） ----------------
    def set_mode(self, mode):
        """on = 连历史待确认一起播；off = 静音；auto = 只播启动后的新告警。"""
        if mode == "on":
            self.muted = False
            self.arm_all = True
        elif mode == "off":
            self.muted = True
            self.arm_all = False
        elif mode == "auto":
            self.muted = False
            self.arm_all = False
            self.start_iso = util.iso_now()   # 以"现在"为基线，已有告警不再自动播
        else:
            raise ValueError("mode 只能是 on / off / auto")
        if self.muted:
            self.ringing = False
            self._silence()
        try:
            pending = len(self.api.store.list_alarms(status="pending", limit=200))
        except Exception:
            pending = 0
        return {"mode": mode, "pending": pending}

    def run(self):
        while not self.stop_event.is_set():
            alarm, pending = self._next_alarm()

            if alarm is None:
                if self.ringing:
                    self.ringing = False
                    self._silence()
                    if pending:
                        self.log("[siren] 已停止播报（历史待确认不在自动播报范围）")
                    else:
                        self.log("[siren] 告警已全部解除，停止提示")
                self.stop_event.wait(3)
                continue

            if not self.ringing:
                self.ringing = True
                top = pending[0]
                self.log("[siren] 开始分级提示：共 %d 条，最高 %s（%s）"
                         % (len(pending), top["level"], top["message"]))

            level = alarm["level"]
            # 只要最高等级不是 L3，就别再让警笛响着
            if level != "L3" and self.looping:
                self._silence()
            self.log("[siren] 播报 %s：%s" % (level, alarm["message"]))
            self._pattern(level)
            self._speak(level, alarm["message"])

            # 以当前最严重那条的等级决定下一轮：先等音效放完，再等间隔
            top_level = pending[0]["level"]
            self.stop_event.wait(TONE_SECONDS.get(top_level, 1.0)
                                 + LEVEL_INTERVAL.get(top_level, 9))

    def stop(self):
        self.stop_event.set()


class Handler(BaseHTTPRequestHandler):
    server_version = "ShadowLamp/1.0"

    def log_message(self, fmt, *args):
        host = self.headers.get("Host", "-") if hasattr(self, "headers") else "-"
        sys.stderr.write("[http] %s  host=%s\n" % (fmt % args, host))

    def _send(self, status, payload):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()
        self.wfile.write(data)

    def _send_html(self, html):
        data = html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self):
        self._send(200, {"ok": True})

    def do_GET(self):
        self._handle("GET")

    def do_POST(self):
        self._handle("POST")

    def _handle(self, method):
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        if method == "GET" and parsed.path in ("/", "/dashboard", "/index.html"):
            # 浏览器直接打开就能看：不依赖 APP，断网也能渲染（零外部资源）
            base = "http://%s" % (parsed.netloc or self.headers.get("Host") or "")
            self._send_html(dashboard.render(base))
            return
        body = {}
        try:
            length = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            length = 0
        if length > 0:
            raw = self.rfile.read(min(length, MAX_BODY))
            try:
                body = json.loads(raw.decode("utf-8")) if raw else {}
            except ValueError:
                self._send(400, {"code": "BAD_JSON", "message": "请求体不是合法 JSON"})
                return
        try:
            result = self.server.api.dispatch(method, parsed.path, query, body)
            self._send(200, result)
        except ApiError as exc:
            self._send(exc.status, {"code": exc.code, "message": exc.message})
        except Exception as exc:
            traceback.print_exc()
            self._send(500, {"code": "INTERNAL", "message": str(exc)})


def port_busy(port, timeout=0.4):
    """端口上已经有别的服务在应答吗？

    Windows 下 ThreadingHTTPServer 带 allow_reuse_address，两个进程能同时
    绑到同一个端口，后起的那个看起来"启动成功"，请求却被先起的接走——排查
    起来非常费劲。所以启动前先探一下，占用了就直接报错退出。
    """
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.settimeout(timeout)
    try:
        probe.connect(("127.0.0.1", int(port)))
        return True
    except Exception:
        return False
    finally:
        try:
            probe.close()
        except Exception:
            pass


class QuietServer(ThreadingHTTPServer):
    """客户端主动断开（ConnectionReset）不打印堆栈，保持日志干净。"""

    def server_bind(self):
        self.server_name = "localhost"
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(self.server_address)
        self.server_address = self.socket.getsockname()
        self.server_port = self.server_address[1]

    def handle_error(self, request, client_address):
        import sys
        exc = sys.exc_info()[1]
        if isinstance(exc, (ConnectionResetError, BrokenPipeError)):
            return
        ThreadingHTTPServer.handle_error(self, request, client_address)


def local_ip():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 80))
        ip = sock.getsockname()[0]
        sock.close()
        return ip
    except Exception:
        return "127.0.0.1"


def main(argv=None):
    check_python_version()
    parser = argparse.ArgumentParser(description="独居老人居家安全监测 · 本地网关")
    parser.add_argument("--host", default=DEFAULT_HOST, help="监听地址，默认 127.0.0.1")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="监听端口，默认 8780")
    parser.add_argument("--db", default=DEFAULT_DB, help="SQLite 路径，默认 data/gateway.db")
    parser.add_argument("--lan", action="store_true", help="监听 0.0.0.0，允许手机/开发板访问")
    parser.add_argument("--udp-port", type=int, default=9999,
                        help="同时监听 UDP 端口收 JSON（默认 9999，0=关闭），Wi-Fi 独立供电模式通道")
    parser.add_argument("--serial", default="",
                        help="USB 串口桥接模式，例如 COM3；与 --udp-port 可同时启用")
    parser.add_argument("--baud", type=int, default=115200, help="串口波特率")
    parser.add_argument("--siren", action="store_true",
                        help="收到新告警时电脑蜂鸣（模拟板端蜂鸣器）")
    parser.add_argument("--voice", action="store_true",
                        help="收到新告警时语音播报（模拟板端语音模块）")
    args = parser.parse_args(argv)

    host = "0.0.0.0" if args.lan else args.host
    if port_busy(args.port):
        print("端口 %d 已被占用：很可能已经有一个网关在跑（两个进程绑同一端口会出现"
              "“看着启动成功、请求却被旧进程接走”的怪现象）。" % args.port)
        print("  停掉旧的：powershell -ExecutionPolicy Bypass -File scripts\demo.ps1 -Stop")
        print("  或换端口：--port 8790")
        return 2
    store = Store(args.db)
    server = QuietServer((host, args.port), Handler)
    server.api = Api(store)
    server.api.siren = args.siren
    server.api.voice = args.voice
    server.daemon_threads = True

    def log(msg):
        print("[%s] %s" % (util.now().strftime("%H:%M:%S"), msg))

    readers = []
    if args.udp_port:
        udp = UdpIngest(server.api, "0.0.0.0", args.udp_port, logger=log)
        udp.start()
        readers.append(udp)
    if args.serial:
        ser = SerialIngest(server.api, args.serial, args.baud, logger=log)
        ser.start()
        readers.append(ser)

    ip = local_ip() if host == "0.0.0.0" else host
    print("=" * 62)
    print(" 守夜灯 · 本地网关已启动")
    print("   监听    : %s:%d" % (host, args.port))
    print("   数据库  : %s" % args.db)
    print("   本机访问: http://127.0.0.1:%d/health" % args.port)
    siren_thread = None
    if args.siren:
        siren_thread = SirenThread(server.api, logger=log)
        siren_thread.start()
        readers.append(siren_thread)
        # 暴露给 HTTP：POST /api/v1/siren {"mode": "on|off|auto"}
        server.api.siren_control = siren_thread

    if readers:
        print("   接入通道: %s" % " / ".join(r.name for r in readers))
    else:
        print("   接入通道: HTTP only（固件也可直接 POST /api/v1/snapshot）")
    if args.siren:
        print("   播报策略: 只响启动后的新告警（历史待确认不自动响）")
        print("             要立刻播报：python scripts\siren.py on ｜ 静音：siren.py off")
    if host == "0.0.0.0":
        print("   局域网  : http://%s:%d  （APP 里填这个地址）" % (ip, args.port))
    else:
        print("   提示    : 手机要访问请用 --lan 启动")
    print("   停止    : Ctrl+C")
    print("=" * 62)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n正在关闭…")
    finally:
        for reader in readers:
            reader.stop()
        server.shutdown()
        server.server_close()
        store.close()


if __name__ == "__main__":
    main()
