# -*- coding: utf-8 -*-
"""固件静态一致性检查（不烧板子也能在电脑上跑）。

为什么要有这个脚本：
  南向固件在这台机器上没有 Hi3861 源码树，编不了。没法编译就只能靠“人眼”检查，
  而人眼最容易漏的恰恰是跨文件的约定：
    - 新增的 .c 忘了加进 BUILD.gn（本次就真的漏了 env.c，等于光照/温湿度全是 null）；
    - 板端判出来的事件类型名和网关不认识的名字对不上，事件全被丢弃；
    - 板端阈值和网关阈值各改各的，同一间屋子两个标准；
    - 拼快照的字段少一个，APP 那个格子就永远是“—”。
  这些都能用文本比对查出来，所以做成脚本，改完固件跑一遍即可。

用法：
    python scripts/check_firmware.py
    python scripts/check_firmware.py -v      # 打印更多细节

退出码：0 = 全部通过；1 = 有 FAIL。
"""
import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                 # software/
FW = os.path.join(ROOT, "firmware")
sys.path.insert(0, ROOT)

from gateway.config import BOARD_EVENT_TYPES, Thresholds   # noqa: E402

VERBOSE = False
FAILS = []
CHECKS = 0


def note(msg):
    if VERBOSE:
        print("    " + msg)


def check(name, ok, detail=""):
    global CHECKS
    CHECKS += 1
    print(("  [PASS] " if ok else "  [FAIL] ") + name)
    if not ok:
        if detail:
            print("         " + detail)
        FAILS.append(name)
    return ok


def read(path):
    return io.open(path, encoding="utf-8").read()


def c_files():
    return sorted(f for f in os.listdir(FW) if f.endswith(".c"))


def h_files():
    return sorted(f for f in os.listdir(FW) if f.endswith(".h"))


# ----------------------------------------------------------------------
def check_build_gn():
    print("\n[1] BUILD.gn 覆盖所有源文件")
    text = read(os.path.join(FW, "BUILD.gn"))
    listed = set(re.findall(r'"([\w.]+\.c)"', text))
    actual = set(c_files())
    missing = actual - listed          # 写了却没编译 -> 相当于没这个文件
    ghost = listed - actual            # 列了但文件不存在 -> 构建直接失败
    check("所有 .c 都已加入 sources", not missing, "漏了：%s" % ", ".join(sorted(missing)))
    check("sources 里的文件都存在", not ghost, "不存在：%s" % ", ".join(sorted(ghost)))
    note("sources = %s" % ", ".join(sorted(listed)))


def check_include_guard():
    print("\n[2] 头文件 include guard")
    for name in h_files():
        text = read(os.path.join(FW, name))
        m = re.search(r"#ifndef\s+(\w+)", text)
        d = re.search(r"#define\s+(\w+)", text)
        ok = bool(m and d and m.group(1) == d.group(1))
        check("%s 有配对 include guard" % name, ok, "缺 #ifndef/#define 或两者不一致")
        if ok:
            tail = text.rstrip().splitlines()[-1].strip()
            check("%s 结尾有 #endif" % name, tail.startswith("#endif"),
                  "最后一行是：%s" % tail)


def strip_comments(text):
    """去掉注释但保留行号（块注释换成等量空行），否则注释里提到 %f 会误报。"""
    def repl(m):
        return "\n" * m.group(0).count("\n")
    text = re.sub(r"/\*.*?\*/", repl, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def check_float_printf():
    print("\n[3] 不使用浮点 printf（Hi3861 的 printf 默认不支持 %f）")
    bad = []
    pattern = re.compile(r"%[-+ 0-9.*]*[fFeEgG]")
    for name in c_files():
        for lineno, line in enumerate(strip_comments(read(os.path.join(FW, name))).splitlines(), 1):
            if pattern.search(line):
                bad.append("%s:%d  %s" % (name, lineno, line.strip()))
    check("没有 %f/%e/%g 格式符", not bad, "\n         ".join(bad))


def check_thresholds():
    print("\n[4] 板端阈值 == 网关阈值（同一条规则不能有两个标准）")
    text = read(os.path.join(FW, "config.h"))
    values = {}
    for name, num in re.findall(r"#define\s+(CFG_[A-Z_]+)\s+\(?(-?\d+(?:\.\d+)?)f?\)?", text):
        values.setdefault(name, float(num))

    pairs = [
        ("CFG_T_WARN", "temp_warm"),
        ("CFG_T_HIGH", "temp_high"),
        ("CFG_T_FIRE", "temp_fire"),
        ("CFG_T_RISE", "temp_rise_delta"),
        ("CFG_T_LOW", "temp_low"),
        ("CFG_H_HIGH", "hum_high"),
        ("CFG_H_WARN_HIGH", "hum_warn_high"),
        ("CFG_H_LOW", "hum_low"),
        ("CFG_H_WARN_LOW", "hum_warn_low"),
    ]
    defaults = Thresholds()
    for macro, field in pairs:
        if macro not in values:
            check("%s 在 config.h 中定义" % macro, False, "没找到该宏")
            continue
        expect = float(getattr(defaults, field))
        check("%s(%g) == %s(%g)" % (macro, values[macro], field, expect),
              abs(values[macro] - expect) < 1e-6,
              "板端 %g / 网关 %g，改一处必须同步另一处" % (values[macro], expect))

    # 升温窗口：CFG_T_RISE_MS 毫秒 vs temp_rise_window 秒
    m = re.search(r"#define\s+CFG_T_RISE_MS\s+\((\d+)\s*\*\s*1000\)", text)
    ok = bool(m) and int(m.group(1)) == defaults.temp_rise_window
    check("CFG_T_RISE_MS == temp_rise_window(%ds)" % defaults.temp_rise_window, ok,
          "config.h 里没找到 (N * 1000) 形式的定义")

    # 门磁持续开启
    m = re.search(r"#define\s+CFG_DOOR_OPEN_MS\s+\((\d+)\s*\*\s*60\s*\*\s*1000\)", text)
    ok = bool(m) and int(m.group(1)) == defaults.door_open_max_min
    check("CFG_DOOR_OPEN_MS == door_open_max_min(%d分钟)" % defaults.door_open_max_min, ok,
          "config.h 里没找到门磁超时定义")


def check_event_types():
    print("\n[5] 板端事件类型 == 网关认识的类型")
    text = read(os.path.join(FW, "local_rule.c"))
    board_types = set(re.findall(r'return\s+"([A-Z_]{3,})"', text)) - {"NONE"}
    gw = set(BOARD_EVENT_TYPES)
    unknown = board_types - gw
    unused = gw - board_types
    check("板端上报的类型网关都认识", not unknown,
          "网关不认识：%s（会被直接丢弃）" % ", ".join(sorted(unknown)))
    check("网关登记的类型板端都会产生", not unused,
          "板端从不产生：%s（登记多余，不算错但要清理）" % ", ".join(sorted(unused)))
    note("板端类型：%s" % ", ".join(sorted(board_types)))

    # 网关侧必须给每种类型配默认等级与文案，否则告警列表显示英文类型名
    rules = read(os.path.join(ROOT, "gateway", "rules.py"))
    for block in ("BOARD_LEVEL", "BOARD_TEXT"):
        body = rules.split(block + " = {", 1)[-1].split("}", 1)[0]
        missing = [t for t in board_types if '"%s"' % t not in body]
        check("rules.py 的 %s 覆盖全部板端类型" % block, not missing,
              "缺：%s" % ", ".join(sorted(missing)))


SNAPSHOT_FIELDS = [
    "deviceId", "ts", "seq", "pirDetected", "lastMotionTs", "lastMotionZone",
    "temperature", "humidity", "lux", "doorOpen", "battery", "powerState",
    "rssi", "online", "source", "fwVersion",
]


def check_snapshot_fields():
    print("\n[6] 快照字段与数据契约一致（少一个字段 = APP 那个格子永远是“—”）")
    text = read(os.path.join(FW, "app_main.c"))
    block = text.split("static void build_snapshot", 1)[-1].split("static void build_event", 1)[0]
    keys = re.findall(r'\\"(\w+)\\":', block)
    check("快照字段数量为 %d" % len(SNAPSHOT_FIELDS),
          len(keys) == len(SNAPSHOT_FIELDS), "板端拼了 %d 个：%s" % (len(keys), ", ".join(keys)))
    missing = [f for f in SNAPSHOT_FIELDS if f not in keys]
    extra = [k for k in keys if k not in SNAPSHOT_FIELDS]
    check("十六个字段一个不少", not missing, "缺：%s" % ", ".join(missing))
    check("没有契约外的新字段", not extra,
          "多出：%s（网关会原样存，但 APP 不显示，确认后同步改契约文档）" % ", ".join(extra))


def check_declarations():
    print("\n[7] 跨模块调用都有声明（否则 Hi3861 上会因隐式声明各按各的猜）")
    headers = []
    for name in h_files():
        headers.append(read(os.path.join(FW, name)))
    hdr_text = "\n".join(headers)

    for name in c_files():
        text = read(os.path.join(FW, name))
        body = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        calls = set(re.findall(r"\b((?:Biz|Drv|Env|Loc|Net|Sntp)_[A-Za-z0-9_]+)\s*\(", body))
        own_defs = set(re.findall(r"^\s*(?:[a-zA-Z_][\w ]*?)\b(\w+)\s*\([^;]*\)\s*\{",
                                  body, flags=re.M))
        missing = sorted(c for c in calls
                         if c not in own_defs and not re.search(r"\b%s\s*\(" % c, hdr_text))
        check("%s 的跨模块调用都有声明" % name, not missing,
              "没有声明：%s" % ", ".join(missing))


def check_ack_contract():
    print("\n[8] 下行确认的两个整数字段，网关节点的键名与固件一致")
    app = read(os.path.join(FW, "app_main.c"))
    api = read(os.path.join(ROOT, "gateway", "api.py"))
    for key in ("pendingCount", "acknowledgedCount"):
        in_fw = ('\\"%s\\":' % key) in app
        in_gw = ('"%s"' % key) in api
        check("%s 两端都有" % key, in_fw and in_gw,
              "固件=%s 网关=%s" % (in_fw, in_gw))

    fw_path = re.search(r'"(/api/v1/[^"?]+\?deviceId=%s)"', app)
    check("固件请求的是 /api/v1/alarms/active", bool(fw_path),
          "没找到固件里的请求路径")
    if fw_path:
        route_ok = "alarms/active" in api
        check("网关注册了 /alarms/active 路由", route_ok, "api.py 的 routes 里没有这条")


def check_pure_logic():
    print("\n[9] local_rule.c 是纯逻辑（不碰硬件/网络，才能单独测试）")
    text = read(os.path.join(FW, "local_rule.c"))
    dirty = [w for w in ("hi_gpio", "hi_io_", "socket(", "wifi_device", "osDelay")
             if w in text]
    check("local_rule.c 没有硬件/网络调用", not dirty, "出现了：%s" % ", ".join(dirty))


def check_net_port():
    """固件里的网关端口必须和网关程序默认端口一致（否则板子上报直接连不上）。"""
    from gateway.config import DEFAULT_PORT

    cfg = read(os.path.join(FW, "config.h"))
    m = re.search(r"#define\s+CFG_GATEWAY_PORT\s+(\d+)", cfg)
    fw_port = int(m.group(1)) if m else -1
    check("网关端口一致（firmware CFG_GATEWAY_PORT=%s / gateway DEFAULT_PORT=%s）"
          % (fw_port, DEFAULT_PORT), fw_port == DEFAULT_PORT,
          "改 firmware/config.h 的 CFG_GATEWAY_PORT 或网关 --port，二者必须相同")

    # 固件只用 HTTP 通道时没有 UDP 端口宏；有就必须和文档/脚本里的 9998 一致
    m2 = re.search(r"#define\s+CFG_GATEWAY_UDP_PORT\s+(\d+)", cfg)
    if m2:
        check("UDP 端口一致（%s / 约定 9998）" % m2.group(1), int(m2.group(1)) == 9998)


def main():
    global VERBOSE
    parser = argparse.ArgumentParser(description="固件静态一致性检查")
    parser.add_argument("-v", "--verbose", action="store_true", help="打印细节")
    args = parser.parse_args()
    VERBOSE = args.verbose

    print("固件静态检查：%s" % FW)
    check_build_gn()
    check_include_guard()
    check_float_printf()
    check_thresholds()
    check_event_types()
    check_snapshot_fields()
    check_declarations()
    check_ack_contract()
    check_pure_logic()
    check_net_port()

    print("\n共 %d 项，失败 %d 项" % (CHECKS, len(FAILS)))
    if FAILS:
        print("失败项：")
        for name in FAILS:
            print("  - " + name)
        return 1
    print("全部通过。注意：这只是静态检查，真机行为仍需烧录后按实验指导书 T01 验证。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
