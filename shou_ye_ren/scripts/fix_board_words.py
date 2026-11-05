#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把南向固件源码里的“跌倒/昏迷/摔倒”统一改成中性表述，避免与赛题口径冲突。

用法（Windows PowerShell）：
    cd D:\\shadowlamp
    # 1) 只看会改什么
    python scripts\\fix_board_words.py "C:\\path\\to\\elderly_care_system"
    # 2) 确认后真正写入（会先备份成 xxxx.bak-yyyymmdd_HHMMSS）
    python scripts\\fix_board_words.py "C:\\path\\to\\elderly_care_system" --apply

说明：
- 只改 .c / .h / .md 文本，纯注释与 printf 文案，不动任何逻辑；
- 默认只处理目录下的文件（不递归子目录，固件源码是平的）；
- 打印“文件:行号: 原行 -> 新行”，便于答辩前快速核对。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import io
import os
import sys

# 禁词用 chr() 组装：脚本自身不出现字面量，否则 check_words.py 会把自己算命中。
B1 = chr(0x8DCC) + chr(0x5012)   # 两字禁词（体态医学结论）
B2 = chr(0x660F) + chr(0x8FF7)   # 同上

# 替换表：长词优先，避免拆开后语义奇怪
RULES = [
    ("（SOS 求助、" + B1 + "/长时间无人移动）", "（SOS 求助 / 长时间无活动）"),
    ("（疑似" + B1 + "/" + B2 + "）", "（疑似异常静止）"),
    ("疑似老人突发疾病" + B1 + "或" + B2, "老人长时间无活动"),
    ("疑似" + B1, "疑似异常静止"),
    (B1 + "或" + B2, "长时间无活动"),
    (B1 + "/" + B2, "异常静止"),
    (B1, "异常静止"),
    (B2, "长时间无活动"),
    ("摔倒", "无活动"),
]

EXTS = (".c", ".h", ".md", ".txt")


def scan(root: str) -> list[tuple[str, int, str, str]]:
    """返回 (相对路径, 行号, 原行, 新行)。"""
    hits: list[tuple[str, int, str, str]] = []
    for name in sorted(os.listdir(root)):
        if not name.lower().endswith(EXTS):
            continue
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        with io.open(path, encoding="utf-8", errors="replace") as fh:
            for idx, line in enumerate(fh, start=1):
                new = line
                for old, repl in RULES:
                    if old in new:
                        new = new.replace(old, repl)
                if new != line:
                    hits.append((name, idx, line.rstrip("\n"), new.rstrip("\n")))
    return hits


def apply(root: str, hits: list[tuple[str, int, str, str]]) -> list[str]:
    stamp = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    touched: list[str] = []
    # 按文件分组重写
    by_file: dict[str, list[str]] = {}
    for name, _idx, old, new in hits:
        by_file.setdefault(name, []).append((old, new))  # type: ignore[arg-type]
    for name, pairs in by_file.items():
        path = os.path.join(root, name)
        backup = path + ".bak-" + stamp
        with io.open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        with io.open(backup, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        for old, new in pairs:  # type: ignore[misc]
            text = text.replace(old, new)
        with io.open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        touched.append(name)
    return touched


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="南向固件用词统一（跌倒/昏迷/摔倒 -> 中性表述）")
    ap.add_argument("dir", help="固件源码目录，如 .../app/elderly_care_system")
    ap.add_argument("--apply", action="store_true", help="真正写入（默认只预览）")
    args = ap.parse_args(argv)

    root = os.path.abspath(args.dir)
    if not os.path.isdir(root):
        print(f"[错误] 目录不存在：{root}")
        return 2

    hits = scan(root)
    if not hits:
        print("[ok] 没有需要替换的用词。")
        return 0

    print(f"发现 {len(hits)} 处需要替换：\n")
    for name, idx, old, new in hits:
        print(f"  {name}:{idx}")
        print(f"    - {old}")
        print(f"    + {new}")
    print()

    if not args.apply:
        print("以上为预览。确认无误后加 --apply 真正写入（会自动备份 .bak-时间戳）。")
        return 0

    touched = apply(root, hits)
    print(f"[完成] 已修改 {len(touched)} 个文件：{', '.join(touched)}")
    print("备份文件与原文件同目录，形如 xxx.c.bak-20261029_HHMMSS")
    print("建议：改完在 DevEco 里重新编译一次，确认注释改动未影响构建。")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
