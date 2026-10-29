# -*- coding: utf-8 -*-
"""生成大赛提交包（rar），按官方命名规则：赛题序号+赛题名称+XX学校团队名称.rar

官方要求（通知附件1 二、参赛作品内容）：
  1) 设计方案：Word（*.docx），宋体小四、1.5 倍行距
  2) 演示视频：mp4，≤100MB，≤5 分钟
  3) PPT：讲解 10 分钟以内
  全部材料统一压缩为 rar，命名「赛题序号+赛题名称+XX 学校团队名称」
  所有材料不得出现团队、指导老师、院校信息（正文里）。

用法：
  python scripts/make_submission.py                       # 学校/队名用占位符
  python scripts/make_submission.py --school 河南xx大学 --team 守夜队
提交前把 `--school` / `--team` 换成真实信息再跑一次即可。
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                     # software/
WORKSPACE = os.path.dirname(ROOT)                # 工作区根（放 3 份提交材料）
RAR = r"C:\Program Files\WinRAR\Rar.exe"

DOCX_NAME = u"赛题四_设计方案_提交版.docx"
PPTX_NAME = u"赛题四_路演PPT_10分钟.pptx"
ZIP_NAME = u"守夜灯_软件包_20261027.zip"
TOPIC = u"独居老人居家安全监测与应急辅助系统"

README = u"""守夜灯 · 独居老人居家安全监测与应急辅助系统
赛题四（软硬件结合类）· 提交包说明

一、包内材料
1. {docx}
   —— 设计方案（Word，正文宋体小四、1.5 倍行距，含技术架构、软硬件选型、测试结果）
2. {pptx}
   —— 路演 PPT（21 页，对应讲解时长 10 分钟以内）
3. {zip}
   —— 软件工程包（源代码 + 部署/演示/测试文档，供评委复现）
4. 本说明文件

二、演示视频
   见通知要求：mp4、≤100MB、≤5 分钟。录制完成后放入本包同名目录，
   再按官方命名规则重新压缩即可（本脚本会自动把视频一起打进去）。

三、复现路径（软件侧，无需硬件即可跑通）
   解压 {zip} → 进入 software/ → 按 docs/部署操作手册.md 启动网关 → 按
   docs/演示走查清单.md 逐步操作；硬件未接时全部数据标注为「测试/模拟」，
   界面与文档都明确说明“非真实传感器采集”，不冒充真实数据。

四、数据与边界（如实声明）
   - 未接开发板时，APP 首页/告警页显示“测试/模拟”来源标签；
   - 硬件侧固件源码在 software/firmware/，需在 Hi3861 源码树内编译后烧录；
   - 成员/家庭组关系由网关内存维护，重启重置（生产应换云端账号体系）；
   - 仅局域网内使用，无鉴权，不含云端/推送/星闪/4G。

五、匿名声明
   三份材料正文与代码注释中均不含学校、姓名、指导教师、队名信息；
   文件名按官方模板包含学校信息，仅用于投稿命名。
""".format(docx=DOCX_NAME, pptx=PPTX_NAME, zip=ZIP_NAME)


def make_stage(stage):
    if os.path.isdir(stage):
        shutil.rmtree(stage)
    os.makedirs(stage)
    missing = []
    for name in (DOCX_NAME, PPTX_NAME, ZIP_NAME):
        src = os.path.join(WORKSPACE, name)
        if not os.path.isfile(src):
            missing.append(name)
            continue
        shutil.copy2(src, os.path.join(stage, name))
    # 演示视频（可选）：工作区里出现 mp4 就自动带上
    for fn in os.listdir(WORKSPACE):
        if fn.lower().endswith(".mp4"):
            p = os.path.join(WORKSPACE, fn)
            size_mb = os.path.getsize(p) / 1024.0 / 1024.0
            if size_mb > 100:
                print("  [warn] 视频超过 100MB，大赛不接收：%s (%.1f MB)" % (fn, size_mb))
                continue
            shutil.copy2(p, os.path.join(stage, fn))
            print("  [ok] 已带上演示视频：%s (%.1f MB)" % (fn, size_mb))
    with open(os.path.join(stage, u"提交说明.txt"), "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write(README)
    return missing


def pack(stage, out_rar):
    if os.path.isfile(out_rar):
        os.remove(out_rar)
    if os.path.isfile(RAR):
        cmd = [RAR, "a", "-idq", "-ep1", out_rar] + sorted(os.listdir(stage))
        r = subprocess.run(cmd, cwd=stage, capture_output=True)
        if r.returncode != 0:
            print("  [error] Rar 失败：", r.stdout.decode("gbk", "replace"), r.stderr.decode("gbk", "replace"))
            return False
        return True
    # 没有 WinRAR 时退化为 zip，但会明确提示不符合大赛要求
    print("  [warn] 未找到 Rar.exe，退化为 zip（大赛要求 rar，请在装有 WinRAR 的机器上重跑）")
    base = os.path.splitext(out_rar)[0]
    shutil.make_archive(base, "zip", stage)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--school", default=u"XX学校", help="学校名称（按官方模板填）")
    ap.add_argument("--team", default=u"XX团队", help="团队名称")
    ap.add_argument("--out-dir", default=WORKSPACE, help="输出目录（默认工作区根）")
    args = ap.parse_args()

    stage = os.path.join(WORKSPACE, "_submission_stage")
    print("[1/3] 收集材料 ->", stage)
    missing = make_stage(stage)
    for m in missing:
        print("  [missing]", m)
    if missing:
        print("  [error] 关键材料缺失，已停止打包（避免交出不完整的包）")
        return 2

    base = u"赛题四+%s+%s%s" % (TOPIC, args.school, args.team)
    out = os.path.join(args.out_dir, base + u".rar")
    print("[2/3] 压缩 ->", out)
    if not pack(stage, out):
        return 3

    size_mb = os.path.getsize(out) / 1024.0 / 1024.0
    print("[3/3] 完成：%s（%.2f MB）" % (os.path.basename(out), size_mb))
    shutil.rmtree(stage, ignore_errors=True)
    print("提示：提交前把 --school / --team 换成真实信息重跑；视频放进工作区根后会自动带上。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
