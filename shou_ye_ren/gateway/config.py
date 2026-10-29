# -*- coding: utf-8 -*-
"""网关配置与可下发阈值。

兼容性：本工程面向 Python 3.8（与 DevEco Device Tool 要求的 3.7.4~3.8 一致），
不使用 3.9+ 的语法与运行期泛型。

阈值全部是“项目演示参数”，不是医疗或行业标准——算法、界面、设计文档中
必须保持同一口径，避免把演示阈值包装成权威判断。
"""
from __future__ import annotations

from dataclasses import asdict, dataclass, fields

# 服务运行参数
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8780
DEFAULT_DB = "data/gateway.db"

# 设备心跳超过该秒数即视为离线（固件 1 秒一帧，留出 3 倍冗余）
OFFLINE_SECONDS = 10

# 补传数据允许的乱序窗口（秒）：超出窗口只入库，不参与实时判定，
# 避免“用一小时前的快照触发现场告警”这种荒谬结果。
BACKFILL_WINDOW_SECONDS = 300

# 设备端可直接上报、由网关直接建单的事件类型（无需再算阈值）
DIRECT_EVENT_TYPES = ("SOS", "GAS", "SMOKE")

# 板端本地阈值判定上报的事件类型，名字与 firmware/local_rule.c 的
# Loc_KindAlarmType() 一一对应。板端只做“单指标 + 时间窗”，属于快反；
# 真正建单仍要过网关的冷却去重，避免同一件事被板端反复报成十几条。
BOARD_EVENT_TYPES = (
    "TEMP_FIRE", "TEMP_RISE", "TEMP_HIGH", "TEMP_LOW",
    "HUM_HIGH", "HUM_LOW", "DOOR_OPEN",
    "TEMP_WARM", "HUM_WARN_HIGH", "HUM_WARN_LOW",
)

# 紧急危急生命安全告警：零冷却间隔，绝不抑制！只要事件触发，随时立即报警建单！
ZERO_COOLDOWN_TYPES = (
    "SOS", "FALL", "FALL_DOWN", "GAS", "SMOKE", "TEMP_FIRE", "TEMP_RISE", "NO_MOTION"
)


@dataclass
class Thresholds:
    """可由 APP 远程下发的判定参数，单位为分钟 / 摄氏度 / %RH。"""

    no_motion_min: int = 5          # 白天连续无活动告警阈值
    night_no_motion_min: int = 3    # 夜间连续无活动告警阈值
    night_bathroom_min: int = 2     # 夜间卫生间单独收紧（最高风险组合）
    away_max_min: int = 120         # 疑似外出最长容忍时长
    door_open_max_min: int = 10     # 门窗持续开启提醒
    cooldown_env_min: int = 15      # 环境类告警（温湿度、光照）处置关闭后的静音冷却窗口（分钟）
    cooldown_door_min: int = 10     # 门窗持续开启告警处置关闭后的冷却窗口（分钟）
    escalate_min: int = 10          # 待确认超时自动升级
    temp_warm: float = 28.0         # 偏热提醒
    temp_high: float = 35.0         # 过高
    temp_fire: float = 55.0         # 疑似火情（绝对高温）
    temp_rise_delta: float = 8.0    # 疑似火情（短时升温幅度，℃）
    temp_rise_window: int = 180     # 升温统计窗口（秒）
    temp_low: float = 16.0
    hum_high: float = 80.0          # 过高
    hum_warn_high: float = 70.0     # 偏高提醒
    hum_low: float = 25.0           # 过低
    hum_warn_low: float = 35.0      # 偏低提醒
    lux_night_high: float = 200.0   # 夜间异常强光
    lux_day_low: float = 20.0       # 白天采光不足
    night_start_hour: int = 22      # 夜间起点（含）
    night_end_hour: int = 6         # 夜间终点（不含）
    sleep_start_hour: int = 23      # 夜间睡眠起点（含）
    sleep_end_hour: int = 7         # 夜间睡眠终点（不含）
    sleep_gap_min: int = 360        # 睡眠期超长静默才算异常（分钟）
    startup_warmup_sec: int = 180   # 重启/断档后，时间累积型告警的预热窗口（秒）
    realtime_tolerance_sec: int = 120  # 快照时间与服务器时间差在此范围内算“实时帧”
    nap_enabled: int = 1            # 是否启用午睡时段（1 开 / 0 关）
    nap_start_hour: int = 13        # 午睡起点（含）
    nap_end_hour: int = 15          # 午睡终点（不含）
    # 地区气候偏移：基础阈值 + 偏移 = 实际生效阈值
    temp_offset: float = 0.0        # 例如炎热地区 +3
    hum_offset: float = 0.0         # 例如潮湿地区 +10

    @classmethod
    def from_dict(cls, data):
        """只接收已知字段，未知键忽略；类型按默认值推断。

        注意：类上使用了 `from __future__ import annotations`，字段的 .type 是字符串，
        不能用来判断类型，因此这里用默认值本身推断。
        """
        known = {f.name for f in fields(cls)}
        defaults = cls()
        clean = {}
        for key, value in (data or {}).items():
            if key not in known:
                continue
            default = getattr(defaults, key)
            try:
                clean[key] = int(value) if isinstance(default, int) else float(value)
            except (TypeError, ValueError):
                continue
        return cls(**clean)

    @classmethod
    def merge(cls, data):
        """在默认值基础上覆盖，供 /config/threshold 局部更新使用。"""
        base = cls().to_dict()
        if data:
            base.update(cls.from_dict(data).to_dict())
        return cls.from_dict(base)

    def to_dict(self):
        return asdict(self)


def check_python_version(minimum=(3, 8)):
    """早失败：版本过低时给出可读提示，而不是一堆语法错误。"""
    import sys

    if sys.version_info < minimum:
        raise SystemExit(
            "需要 Python %d.%d 及以上（当前 %s）。\n"
            "DevEco Device Tool 要求 3.7.4~3.8，请使用同一解释器运行网关。"
            % (minimum[0], minimum[1], sys.version.split()[0])
        )
