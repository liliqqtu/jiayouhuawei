# -*- coding: utf-8 -*-
"""清掉 docx / pptx 里的文档属性（作者、最后修改人、公司等），避免盲审露出身份。

用法：
    python scripts/strip_meta.py 文件.docx 文件.pptx
    python scripts/strip_meta.py --all          # 清理工作区根目录三件套

原理：docx/pptx 就是 zip，docProps/core.xml 与 app.xml 里存着
    dc:creator / cp:lastModifiedBy / dc:description 等字段，
    python-docx、python-pptx 生成时会写进去（甚至带上模板作者名）。
    这里把非结构字段统一改写为中性值，其余内容不动。
"""
from __future__ import annotations

import glob
import io
import os
import re
import shutil
import sys
import zipfile

NEUTRAL = '守夜灯'
CREATOR_XML = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<cp:coreProperties '
    'xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" '
    'xmlns:dc="http://purl.org/dc/elements/1.1/" '
    'xmlns:dcterms="http://purl.org/dc/terms/" '
    'xmlns:dcmitype="http://purl.org/dc/dcmitype/" '
    'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
    '<dc:title>守夜灯 · 独居老人居家安全监测与应急辅助系统</dc:title>'
    '<dc:creator>%s</dc:creator>'
    '<cp:lastModifiedBy>%s</cp:lastModifiedBy>'
    '<dc:description></dc:description>'
    '</cp:coreProperties>' % (NEUTRAL, NEUTRAL)
)
APP_XML_DROP = ('Company', 'Manager', 'Application')


def clean_app_xml(xml: str) -> str:
    for tag in APP_XML_DROP:
        xml = re.sub(r'<%s>.*?</%s>' % (tag, tag), '', xml, flags=re.S)
    return xml


def strip(path: str) -> bool:
    if not path.lower().endswith(('.docx', '.pptx')):
        print('跳过（不是 docx/pptx）：%s' % path)
        return False
    tmp = path + '.tmp'
    changed = False
    with zipfile.ZipFile(path, 'r') as src, \
            zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as dst:
        for item in src.infolist():
            data = src.read(item.filename)
            if item.filename == 'docProps/core.xml':
                data = CREATOR_XML.encode('utf-8')
                changed = True
            elif item.filename == 'docProps/app.xml':
                data = clean_app_xml(data.decode('utf-8', 'ignore')).encode('utf-8')
                changed = True
            dst.writestr(item, data)
    if not changed:
        os.remove(tmp)
        print('没有需要清理的属性：%s' % path)
        return False
    shutil.move(tmp, path)
    print('已清理文档属性：%s' % os.path.basename(path))
    return True


def main(argv: list) -> int:
    if len(argv) >= 2 and argv[1] == '--all':
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        targets = []
        for pat in ('*.docx', '*.pptx'):
            targets.extend(sorted(glob.glob(os.path.join(root, pat))))
    else:
        targets = argv[1:]
    if not targets:
        print('用法：python scripts/strip_meta.py 文件.docx [文件.pptx ...] 或 --all')
        return 2
    n = 0
    for t in targets:
        if os.path.exists(t) and strip(t):
            n += 1
    print('完成：处理 %d 个文件' % n)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
