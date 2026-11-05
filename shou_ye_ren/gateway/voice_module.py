# -*- coding: utf-8 -*-
"""板端语音模块驱动（BY8301 类串口语音模块）+ 无硬件仿真后端。

需求（2026-11-05）：
    老人按下随身求助按钮（SOS）时，现场语音模块要直接播报预设语音
    “我需要帮助”，让邻居/家人不用看手机也知道出事了。

为什么网关侧要有这个模块：
    真机上语音模块挂在开发板串口上，由固件发指令；但比赛现场演示用的
    是我们自己的电脑（没有接板子），所以这里把“同一套触发逻辑”做成
    可切换后端，演示时用 log/tts，接上模块后换成 serial 即可，触发点不变。

三个后端：
    log     只写日志（默认；任何演示环境都能跑，答辩时可展示日志时间戳）
    tts     用 Windows 语音合成代播（路演现场听得到声音）
    serial  真串口发帧（接上 BY8301 就用它）

BY8301 播放指定曲目的串口帧（通用写法，7 字节指令 + 校验）：
    7E FF 06 03 00 00 <曲目号> <校验和> EF
    校验和 = (0x100 - (0xFF+0x06+0x03+0x00+0x00+曲目号) & 0xFF) & 0xFF
如果手上模块说明书的帧头/校验不一样，只改 build_by8301_frame 一处，
触发逻辑（play/短语表）不用动。

诚实边界（答辩要如实说）：
    1. 当前代码路径是“网关收到 SOS 事件 -> 语音模块播报”，真机上这条链路
       由固件直连模块完成（网关不可用时也能响），固件部分不在本仓库；
    2. serial 后端未在真模块上跑过（我们手头没接硬件），帧格式按通用
       BY8301 指令写，接上后需要按说明书核对一次曲目号。
"""
from __future__ import annotations

import os
import subprocess
import time

# 预设语音曲目号：模块里录好的第几段音频。接上模块后按实际烧录顺序改这里。
DEFAULT_TRACKS = {
    "SOS": 1,
    "GAS": 2,
    "SMOKE": 3,
    "FALL": 4,
    "FALL_DOWN": 4,
    "TEMP_FIRE": 5,
}

# 预设语音文本（tts/log 后端用；serial 后端只用曲目号）
DEFAULT_PHRASES = {
    "SOS": "我需要帮助",
    "GAS": "检测到燃气泄漏，请立即查看",
    "SMOKE": "检测到烟雾，请立即查看",
    "FALL": "检测到紧急情况，请立即查看",
    "FALL_DOWN": "检测到紧急情况，请立即查看",
    "TEMP_FIRE": "检测到火情，请立即查看",
}

DEFAULT_FALLBACK = "家里发出安全告警，请立即查看"


def build_by8301_frame(track):
    """构造 BY8301「播放指定曲目」指令帧。"""
    track = max(1, min(255, int(track)))
    body = [0xFF, 0x06, 0x03, 0x00, 0x00, track]
    checksum = (0x100 - (sum(body) & 0xFF)) & 0xFF
    return bytes([0x7E] + body + [checksum, 0xEF])


class VoiceModule(object):
    """一条语音模块通道：play(类型) -> 播报对应预设语音。

    线程安全：只在触发线程里调用，内部只维护播放计数与最近一次记录。
    """

    def __init__(self, backend="log", port=None, baud=9600, tracks=None,
                 phrases=None, log_path=None, dry_run=False):
        self.backend = str(backend or "log").lower()
        if self.backend not in ("log", "tts", "serial"):
            self.backend = "log"
        self.port = port
        self.baud = int(baud or 9600)
        self.tracks = dict(DEFAULT_TRACKS)
        self.phrases = dict(DEFAULT_PHRASES)
        if tracks:
            self.tracks.update(tracks)
        if phrases:
            self.phrases.update(phrases)
        self.log_path = log_path or os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "..", "data", "voice.log")
        self.dry_run = dry_run
        self._serial = None
        self._serial_error = ""
        self.plays = 0
        self.last = None
        self._procs = []

    # ------------------------------------------------------------ 解析
    def phrase_for(self, key, message=""):
        key = str(key or "").upper()
        text = self.phrases.get(key)
        if text:
            return text
        message = str(message or "").strip()
        return message or DEFAULT_FALLBACK

    def track_for(self, key):
        key = str(key or "").upper()
        return self.tracks.get(key)

    # ------------------------------------------------------------ 播报
    def play(self, key, message=""):
        """播报一条预设语音。返回本次播放记录（失败也会返回记录，ok=False）。"""
        key = str(key or "").upper()
        text = self.phrase_for(key, message)
        track = self.track_for(key)
        record = {
            "key": key,
            "text": text,
            "track": track,
            "backend": self.backend,
            "port": self.port,
            "ts": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "ok": True,
            "note": "",
        }
        try:
            if self.backend == "serial":
                self._play_serial(track, record)
            elif self.backend == "tts":
                self._play_tts(text)
            else:
                self._play_log(key, text, track)
        except Exception as exc:  # 语音失败不能拖垮告警主流程
            record["ok"] = False
            record["note"] = "voice backend failed: %s" % exc
        self.plays += 1
        record["plays"] = self.plays
        self.last = record
        return record

    def _play_log(self, key, text, track):
        line = "[%s] key=%s track=%s text=%s\n" % (
            time.strftime("%Y-%m-%d %H:%M:%S"), key, track, text)
        try:
            folder = os.path.dirname(self.log_path)
            if folder and not os.path.isdir(folder):
                os.makedirs(folder)
            with open(self.log_path, "a", encoding="utf-8") as handle:
                handle.write(line)
        except Exception:
            pass
        print("[voice] %s -> %s" % (key, text))

    def _play_tts(self, text):
        if self.dry_run:
            return
        script = ("$v = New-Object -ComObject SAPI.SpVoice; $v.Rate = 0; "
                  "$v.Speak('%s')" % text.replace("'", "").replace('"', ""))
        proc = subprocess.Popen(["powershell", "-NoProfile", "-Command", script],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self._procs = [p for p in self._procs if p.poll() is None] + [proc]
        self._procs = self._procs[-3:]

    def _play_serial(self, track, record):
        if track is None:
            record["ok"] = False
            record["note"] = "该告警类型没有配预设曲目"
            return
        frame = build_by8301_frame(track)
        record["frame"] = frame.hex()
        if not self.port:
            record["ok"] = False
            record["note"] = "未配置串口（--voice-port COM3）"
            return
        ser = self._open_serial()
        ser.write(frame)
        ser.flush()

    def _open_serial(self):
        if self._serial is not None:
            return self._serial
        try:
            import serial  # pyserial
        except Exception as exc:
            raise RuntimeError("未安装 pyserial：pip install pyserial（%s）" % exc)
        self._serial = serial.Serial(self.port, self.baud, timeout=0.5)
        return self._serial

    # ------------------------------------------------------------ 状态
    def status(self):
        return {
            "backend": self.backend,
            "port": self.port,
            "baud": self.baud,
            "enabled": True,
            "plays": self.plays,
            "last": self.last,
            "tracks": dict(self.tracks),
            "phrases": dict(self.phrases),
            "logPath": self.log_path,
        }

    def close(self):
        try:
            if self._serial is not None:
                self._serial.close()
        except Exception:
            pass
        self._serial = None
        for proc in self._procs:
            try:
                if proc.poll() is None:
                    proc.kill()
            except Exception:
                pass
        self._procs = []
