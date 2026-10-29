# -*- coding: utf-8 -*-
"""公共工具：时间、JSON、ID 生成。

全系统统一使用带时区的 ISO 8601 字符串（例如 2026-11-02T14:03:00+08:00），
避免跨时区与夏令时歧义。这里集中处理解析与格式化，其它模块不要各写一套。
"""
from __future__ import annotations

import json
import re
from datetime import date, datetime, timedelta, timezone

TZ = timezone(timedelta(hours=8))
ISO_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}")


def now() -> datetime:
    """当前时间（东八区）。"""
    return datetime.now(TZ)


def to_iso(dt: datetime) -> str:
    """datetime -> 秒级 ISO 字符串，保留时区偏移。"""
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=TZ)
    return dt.astimezone(TZ).replace(microsecond=0).isoformat()


def iso_now() -> str:
    return to_iso(now())


def parse_ts(value: str | None) -> datetime | None:
    """宽松解析 ISO 时间；无法解析返回 None，不抛异常。"""
    if not value:
        return None
    text = value.strip().replace("Z", "+00:00")
    # 兼容不带秒的写法 2026-11-02T14:03
    if re.match(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}$", text):
        text += ":00"
    try:
        dt = datetime.fromisoformat(text)
    except ValueError:
        return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=TZ)
    return dt.astimezone(TZ)


def day_of(value: str | None) -> str:
    """取日期部分 YYYY-MM-DD；解析失败时用今天。"""
    dt = parse_ts(value)
    return (dt or now()).date().isoformat()


def hour_of(value: str) -> int:
    dt = parse_ts(value)
    return (dt or now()).hour


def dump(obj) -> str:
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":"))


def load(text: str, default=None):
    try:
        return json.loads(text)
    except (TypeError, ValueError):
        return default


def as_bool(value, default=None):
    """把 0/1/"true"/"false"/None 统一成布尔值。"""
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    text = str(value).strip().lower()
    if text in ("1", "true", "yes", "on"):
        return True
    if text in ("0", "false", "no", "off"):
        return False
    return default


def as_float(value, default=None):
    try:
        if value is None or value == "":
            return default
        return float(value)
    except (TypeError, ValueError):
        return default


def as_int(value, default=None):
    try:
        if value is None or value == "":
            return default
        return int(value)
    except (TypeError, ValueError):
        return default


def week_key(dt: datetime) -> str:
    """ISO 周：2026-W45。"""
    year, week, _ = dt.isocalendar()
    return "%d-W%02d" % (year, week)


def week_range(key: str) -> tuple[date, date]:
    """返回该 ISO 周对应的周一与周日。"""
    m = re.match(r"^(\d{4})-W(\d{1,2})$", key.strip().upper())
    if not m:
        d = now().date()
        start = d - timedelta(days=d.weekday())
        return start, start + timedelta(days=6)
    year, week = int(m.group(1)), int(m.group(2))
    start = date.fromisocalendar(year, week, 1)
    return start, start + timedelta(days=6)


def pct_delta(current: float | None, previous: float | None) -> float | None:
    """环比百分比；分母为 0 或缺失时返回 None（界面显示“暂无对比”，不显示 0%）。"""
    if current is None or previous in (None, 0):
        return None
    return round((current - previous) / previous * 100.0, 1)


def minutes_ago(minutes: int) -> str:
    """N 分钟前的时间戳（用于升级/冷却判断）。"""
    return to_iso(now() - timedelta(minutes=minutes))
