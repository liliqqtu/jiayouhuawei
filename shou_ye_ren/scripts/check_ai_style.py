# -*- coding: utf-8 -*-
"""检查文本里的“AI 味”句式，辅助降低 AI 查重风险。

用法：
    python scripts/check_ai_style.py [文件或目录 ...]

默认检查：仓库根目录的 守夜灯_设计方案_匿名提交版.docx + docs/*.md
输出：命中行号、命中词、整句，按命中密度排序给出“最该手改的段落”。
"""
from __future__ import annotations

import os
import re
import sys

# 典型 AI 书面腔 / 模板化连接词 / 空泛拔高。按“越靠前越致命”排序。
BANNED = [
    ("首先/其次/再次/最后 连用", r"首先.{0,60}其次|其次.{0,60}最后"),
    ("综上所述", r"综上所述"),
    ("总而言之", r"总而言之|总的来说"),
    ("值得注意的是", r"值得注意的是|需要指出的是|不难发现"),
    ("不仅…而且…", r"不仅.{0,40}而且"),
    ("旨在/致力于", r"旨在|致力于"),
    ("赋能/闭环/抓手/生态", r"赋能|抓手|生态闭环|形成闭环|闭环生态"),
    ("极大地/显著地提升", r"极大地|显著地|大幅度地"),
    ("为…提供了有力支撑", r"提供了有力支撑|奠定了坚实基础|注入新动能"),
    ("随着…的发展", r"随着.{0,30}的发展|在当今社会|在信息化时代"),
    ("有效解决了…问题", r"有效解决了.{0,20}问题|很好地解决了"),
    ("全方位/多维度/深层次", r"全方位|多维度|深层次|立体化"),
    ("进一步提升/持续优化", r"进一步提升|持续优化|不断完善"),
    ("众所周知", r"众所周知|毋庸置疑"),
]

# 弱化命中（这些词单用没问题，密度过高才像机器写的）
WEAK = [("进行/实现/开展 名词化", r"进行(?=[^，。]{0,6}[的]?[分处实])|实现了.{0,10}的"),
        ("三连并列", r"([\u4e00-\u9fa5]{2,6})、\1?、[\u4e00-\u9fa5]{2,6}、")]

EXT = (".md", ".txt", ".docx")


def read_docx(path: str) -> str:
    from docx import Document

    d = Document(path)
    parts = [p.text for p in d.paragraphs]
    for t in d.tables:
        for row in t.rows:
            parts.append(" | ".join(c.text for c in row.cells))
    return "\n".join(parts)


def read_text(path: str) -> str:
    if path.lower().endswith(".docx"):
        return read_docx(path)
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def collect(target: str) -> list[str]:
    if os.path.isfile(target):
        return [target]
    out = []
    for root, _dirs, files in os.walk(target):
        if any(seg in root for seg in (".git", "build", "node_modules", "oh_modules")):
            continue
        for name in files:
            if name.endswith(EXT):
                out.append(os.path.join(root, name))
    return sorted(out)


def split_sentences(text: str):
    buf = []
    for line in text.splitlines():
        for s in re.split(r"(?<=[。！？；])", line):
            s = s.strip()
            if len(s) >= 8:
                buf.append(s)
    return buf


def main(argv: list[str]) -> int:
    targets = argv or [os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))]
    files = []
    for t in targets:
        files.extend(collect(t))

    hard_hits = []
    weak_hits = []
    for path in files:
        try:
            text = read_text(path)
        except Exception as exc:  # noqa: BLE001
            print("skip", path, exc)
            continue
        for idx, sent in enumerate(split_sentences(text), 1):
            for label, pattern in BANNED:
                if re.search(pattern, sent):
                    hard_hits.append((os.path.relpath(path), idx, label, sent[:90]))
            for label, pattern in WEAK:
                if re.search(pattern, sent):
                    weak_hits.append((os.path.relpath(path), idx, label, sent[:90]))

    print(f"扫描 {len(files)} 个文件")
    print(f"\n== 硬命中（建议必改）：{len(hard_hits)} 处 ==")
    for f, n, label, sent in hard_hits[:40]:
        print(f"[{label}] {f}:{n}  {sent}")
    print(f"\n== 弱命中（密度参考）：{len(weak_hits)} 处 ==")
    for f, n, label, sent in weak_hits[:20]:
        print(f"[{label}] {f}:{n}  {sent}")

    total = len(hard_hits)
    print("\n结论：硬命中 0–5 处为安全区；6–15 处建议逐条改写；>15 处说明整段是模板腔，直接重写该段。")
    return 0 if total <= 5 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
