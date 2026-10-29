# -*- coding: utf-8 -*-
"""用词合规扫描：不允许出现把“无活动”说成体态医学结论的词。

背景：赛题要求只做“疑似长时间无活动”这类可证伪、可人工确认的判断，不能暗示做过
体态识别。这类词一旦出现在界面文案、注释、文档里，答辩时会被直接问住，所以做成
脚本，提交前跑一遍（退出码非 0 就别提交）。

用法：
    python scripts/check_words.py          # 扫软件目录
    python scripts/check_words.py -v       # 连命中行一起打印

范围：*.ets / *.ts / *.js / *.py / *.c / *.h / *.json / *.json5 / *.md / *.gn
     自动跳过 node_modules、build、.git、__pycache__、oh_modules。
"""
import argparse
import io
import os
import sys

# 禁词：tuple(词, 说明)
FORBIDDEN = [
    # 用 chr() 组装，脚本自己不出现这两个字的字面量，否则扫描会把自己也算命中。
    (chr(0x8DCC) + chr(0x5012), "把“无活动”说成体态医学结论"),
    (chr(0x660F) + chr(0x8FF7), "同上，医学描述"),
]

EXTS = (".ets", ".ts", ".js", ".py", ".c", ".h", ".json", ".json5",
        ".md", ".gn", ".txt", ".html", ".css")
SKIP_DIRS = {"node_modules", "build", ".git", "__pycache__", "oh_modules",
             ".hvigor", "data", ".idea", "dist", "cache"}

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def walk(base):
    for cur, dirs, files in os.walk(base):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for name in files:
            if name.endswith(EXTS):
                yield os.path.join(cur, name)


def main():
    parser = argparse.ArgumentParser(description="用词合规扫描")
    parser.add_argument("-v", "--verbose", action="store_true", help="打印命中行")
    parser.add_argument("path", nargs="?", default=ROOT, help="扫描根目录，默认 software/")
    args = parser.parse_args()

    hits = []
    scanned = 0
    for path in walk(args.path):
        scanned += 1
        try:
            text = io.open(path, encoding="utf-8").read()
        except (UnicodeDecodeError, OSError):
            continue
        for word, why in FORBIDDEN:
            if word in text:
                for lineno, line in enumerate(text.splitlines(), 1):
                    if word in line:
                        hits.append((os.path.relpath(path, args.path), lineno, why,
                                     line.strip()[:90]))

    print("扫描 %d 个文件，禁词命中 %d 处" % (scanned, len(hits)))
    if hits:
        for rel, lineno, why, line in hits:
            print("  %s:%d  (%s)" % (rel, lineno, why))
            if args.verbose:
                print("      " + line)
        print("用法建议：改成“疑似长时间无活动”“需要人工确认的状态变化”等可证伪表述。")
        return 1
    print("通过：界面文案、注释、文档里没有体态医学结论类用词。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
