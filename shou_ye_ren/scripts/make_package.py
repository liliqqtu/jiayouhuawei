# -*- coding: utf-8 -*-
"""打包提交包：把软件目录打成 zip（排除构建产物、缓存、数据库）。

    python scripts/make_package.py                       # 默认输出到上级目录
    python scripts/make_package.py --out D:\\提交\\守夜灯.zip

排除规则（不然包会从 0.7MB 涨到几十 MB 且含本机路径/运行数据）：
    oh_modules / node_modules  三方依赖，评委不需要
    build / .hvigor / .preview 构建产物
    __pycache__ / *.pyc        Python 缓存
    data/*.db*                 运行产生的数据库（含模拟数据）
例外：dist/ 下预置的 HAP 必须带进包——队友机器没装 DevEco 时靠它把 APP 装到模拟器上。
"""
from __future__ import annotations

import argparse
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)            # software/
NAME = '守夜灯_软件包'
BACKSLASH = chr(92)                     # 反斜杠，避免源码里写转义字符

SKIP_DIRS = {'oh_modules', 'node_modules', 'build', '.hvigor', '.preview',
             '__pycache__', '.idea', '.cxx', 'worker'}
SKIP_FILES = {'.DS_Store', 'local.properties'}
SKIP_SUFFIX = ('.pyc', '.db', '.db-wal', '.db-shm', '.log', '.hap', '.hsp')

# 这些文件即使命中 SKIP_SUFFIX 也要保留
KEEP = {'dist/entry-default-unsigned.hap'}


def norm(rel: str) -> str:
    """统一成正斜杠的相对路径。"""
    return rel.replace(BACKSLASH, '/')


def include(rel: str) -> bool:
    rel = norm(rel)
    parts = rel.split('/')
    if any(p in SKIP_DIRS for p in parts):
        return False
    if parts[-1] in SKIP_FILES:
        return False
    if rel.startswith('data/'):
        return False
    if rel in KEEP:
        return True
    if parts[-1].endswith(SKIP_SUFFIX):
        return False
    return True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=None)
    args = ap.parse_args()
    out = args.out or os.path.join(os.path.dirname(ROOT), '%s_20261027.zip' % NAME)
    out = os.path.abspath(out)

    hap = os.path.join(ROOT, 'dist', 'entry-default-unsigned.hap')
    if os.path.exists(hap):
        print('预置 HAP：%.0f KB（队友不装 DevEco 也能装 APP）'
              % (os.path.getsize(hap) / 1024.0))
    else:
        print('[!] 缺少 dist/entry-default-unsigned.hap')
        print('    没有它，队友必须先装 DevEco 自己构建；建议先跑 scripts/build_app.ps1')

    if os.path.exists(out):
        os.remove(out)

    count = 0
    total = 0
    missing_keep = set(KEEP)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for dirpath, dirnames, filenames in os.walk(ROOT):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for fn in sorted(filenames):
                full = os.path.join(dirpath, fn)
                rel = norm(os.path.relpath(full, ROOT))
                if not include(rel):
                    continue
                missing_keep.discard(rel)
                z.write(full, NAME + '/' + rel)
                count += 1
                total += os.path.getsize(full)

        if missing_keep:
            print('[!] 以下必需文件没进包：%s' % '、'.join(sorted(missing_keep)))

    size = os.path.getsize(out)
    print('打包完成：%s' % out)
    print('  文件 %d 个，原始 %.2f MB，压缩后 %.2f MB'
          % (count, total / 1048576.0, size / 1048576.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
