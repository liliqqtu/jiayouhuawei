# -*- coding: utf-8 -*-
"""报警音生成与播放（只用标准库）。

为什么不直接用 winsound.Beep：
    Beep 走主板蜂鸣器 / 系统提示音，音量很小，像"信息提示"。
    这里生成满幅 16bit WAV，用 PlaySound 播放，能到扬声器最大音量
    （最终响度还取决于系统音量，演示前把音量拉到 100%）。

三种声音：
    siren_l3.wav  6 秒扫频警笛（600→1600Hz 来回 6 遍），满幅，每轮播一次
    beep_l2.wav   三声中速提示音
    beep_l1.wav   两声轻提示
"""
from __future__ import annotations

import math
import os
import struct
import tempfile
import wave


def _write_wav(path, samples, rate=22050):
    with wave.open(path, "wb") as fp:
        fp.setnchannels(1)
        fp.setsampwidth(2)
        fp.setframerate(rate)
        frames = bytearray()
        for value in samples:
            v = int(max(-1.0, min(1.0, value)) * 32767)
            frames += struct.pack("<h", v)
        fp.writeframes(bytes(frames))


def _tone(freq, seconds, amp=0.9, rate=22050, square=True):
    out = []
    total = int(rate * seconds)
    for i in range(total):
        phase = 2.0 * math.pi * freq * (i / rate)
        value = math.sin(phase)
        if square:
            value = 1.0 if value >= 0 else -1.0   # 方波更"刺耳"，更容易吵醒
        out.append(value * amp)
    return out


def _sweep(f_start, f_end, seconds, amp=1.0, rate=22050):
    out = []
    total = int(rate * seconds)
    for i in range(total):
        ratio = i / float(total)
        freq = f_start + (f_end - f_start) * ratio
        phase = 2.0 * math.pi * freq * (i / rate)
        value = 1.0 if math.sin(phase) >= 0 else -1.0
        out.append(value * amp)
    return out


def build(force=False):
    """生成三种报警音，返回路径字典。"""
    folder = os.path.join(tempfile.gettempdir(), "shadowlamp_siren")
    if not os.path.isdir(folder):
        os.makedirs(folder)

    paths = {
        "L3": os.path.join(folder, "siren_l3.wav"),
        "L2": os.path.join(folder, "beep_l2.wav"),
        "L1": os.path.join(folder, "beep_l1.wav"),
    }

    if force or not os.path.exists(paths["L3"]):
        # 警笛：两轮上下扫频，满幅
        # 做长一点（6 秒），这样"每轮播一次"就能盖住 6 秒的播报间隔，
        # 又不会像 SND_LOOP 那样长期占着音频设备（会把语音播报卡住）。
        siren = (_sweep(600, 1600, 0.5) + _sweep(1600, 600, 0.5)) * 6
        _write_wav(paths["L3"], siren)

    if force or not os.path.exists(paths["L2"]):
        l2 = (_tone(1200, 0.18, 0.85) + [0.0] * 1200 +
              _tone(1200, 0.18, 0.85) + [0.0] * 1200 +
              _tone(950, 0.28, 0.85))
        _write_wav(paths["L2"], l2)

    if force or not os.path.exists(paths["L1"]):
        l1 = (_tone(800, 0.16, 0.35) + [0.0] * 1600 +
              _tone(800, 0.16, 0.35))
        _write_wav(paths["L1"], l1)

    return paths


def play_loop(key, paths):
    """循环播放（用于 L3，直到被叫停）。"""
    try:
        import winsound
        winsound.PlaySound(paths[key],
                           winsound.SND_FILENAME | winsound.SND_ASYNC | winsound.SND_LOOP)
        return True
    except Exception:
        return False


def play_once(key, paths):
    try:
        import winsound
        winsound.PlaySound(paths[key],
                           winsound.SND_FILENAME | winsound.SND_ASYNC)
        return True
    except Exception:
        return False


def stop():
    try:
        import winsound
        winsound.PlaySound(None, winsound.SND_PURGE)
    except Exception:
        pass


def beep(freq, ms):
    """兜底：WAV 播不出来时用系统蜂鸣。"""
    try:
        import winsound
        winsound.Beep(freq, ms)
    except Exception:
        pass
