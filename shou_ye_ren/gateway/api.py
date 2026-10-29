# -*- coding: utf-8 -*-
"""HTTP API 层：路由 + 参数校验 + 业务组装。

接口清单与《需求符合性矩阵》04 表一致；实现上只依赖标准库。
错误统一返回 {code, message}，状态非法迁移返回 409。
"""
from __future__ import annotations

import re
import sys
import traceback
from datetime import date, timedelta

from gateway import util
from gateway.family import FamilyError, FamilyRegistry
from gateway.config import Thresholds
from gateway.ingest import normalize_source
from gateway.rules import Engine, zone_text

START_TIME = util.now()


class ApiError(Exception):
    def __init__(self, status, code, message):
        Exception.__init__(self, message)
        self.status = status
        self.code = code
        self.message = message


def _require(data, key):
    if not isinstance(data, dict) or data.get(key) in (None, ""):
        raise ApiError(400, "BAD_REQUEST", "缺少必填字段：%s" % key)
    return data[key]


def _num(value, default=None):
    return util.as_float(value, default)


class Api(object):
    def __init__(self, store):
        self.store = store
        self.engine = Engine(store)
        # 是否用电脑模拟“本地声光”（演示用；真机由固件驱动 LED + 蜂鸣器）
        self.siren = False
        self.voice = False
        # 家庭组：内存态（网关重启即重置），生产环境应替换为云端账号体系
        self.families = FamilyRegistry()
        self.routes = [
            ("GET", re.compile(r"^/health$"), self.health),
            ("GET", re.compile(r"^/api/v1/snapshot$"), self.get_snapshot),
            ("POST", re.compile(r"^/api/v1/snapshot$"), self.post_snapshot),
            ("GET", re.compile(r"^/api/v1/snapshots$"), self.list_snapshots),
            ("GET", re.compile(r"^/api/v1/alarms$"), self.list_alarms),
            ("GET", re.compile(r"^/api/v1/alarms/active$"), self.active_alarms),
            ("GET", re.compile(r"^/api/v1/alarms/(?P<event_id>[^/]+)$"), self.get_alarm),
            ("POST", re.compile(r"^/api/v1/alarms/(?P<event_id>[^/]+)/ack$"), self.ack_alarm),
            ("POST", re.compile(r"^/api/v1/alarms/(?P<event_id>[^/]+)/close$"), self.close_alarm),
            ("GET", re.compile(r"^/api/v1/reports/daily$"), self.daily_report),
            ("GET", re.compile(r"^/api/v1/reports/weekly$"), self.weekly_report),
            ("GET", re.compile(r"^/api/v1/reports/monthly$"), self.monthly_report),
            ("GET", re.compile(r"^/api/v1/siren$"), self.siren_status),
            ("POST", re.compile(r"^/api/v1/siren$"), self.siren_set),
            ("GET", re.compile(r"^/api/v1/device$"), self.device_status),
            ("POST", re.compile(r"^/api/v1/family/create$"), self.family_create),
            ("POST", re.compile(r"^/api/v1/family/join$"), self.family_join),
            ("GET", re.compile(r"^/api/v1/family$"), self.family_get),
            ("GET", re.compile(r"^/api/v1/family/lookup$"), self.family_lookup),
            ("GET", re.compile(r"^/api/v1/devices$"), self.list_devices),
            ("POST", re.compile(r"^/api/v1/devices$"), self.create_device),
            ("GET", re.compile(r"^/api/v1/config/threshold$"), self.get_thresholds),
            ("POST", re.compile(r"^/api/v1/config/threshold$"), self.set_thresholds),
            ("POST", re.compile(r"^/api/v1/events$"), self.post_event),
        ]

    # ------------------------------------------------------------------
    def dispatch(self, method, path, query, body):
        for verb, pattern, handler in self.routes:
            if verb != method:
                continue
            m = pattern.match(path)
            if m:
                params = dict(m.groupdict())
                params.update({k: v[0] for k, v in query.items()})
                return handler(params, body or {})
        raise ApiError(404, "NOT_FOUND", "未知接口：%s %s" % (method, path))

    # ------------------------------------------------------------- basic
    def health(self, params, body):
        uptime = int((util.now() - START_TIME).total_seconds())
        return {"ok": True, "uptime": uptime, "version": "1.0.0"}

    def device_status(self, params, body):
        status = self.store.device_status(params.get("deviceId") or None)
        if status is None:
            raise ApiError(404, "NO_DEVICE", "尚未收到设备数据")
        return status

    def list_devices(self, params, body):
        """全部设备清单：APP 的"多老人/多设备"切换、看板的设备下拉都用它。

        返回 {"items": [...], "count": n}，字段与 /api/v1/device 完全一致。
        """
        items = self.store.list_devices()
        counts = self.store.pending_counts_by_device()
        for item in items:
            item["pendingAlarms"] = counts.get(item["deviceId"], 0)
        return {"items": items, "count": len(items)}

    def create_device(self, params, body):
        """登记一台设备（设备还没上报过数据时，也能先出现在 APP 的设备列表里）。

        为什么需要这个：网关是"谁上报谁建档"，第二台板子没通气前，APP 里根本
        看不到它，演示时家属会以为"连不上"。这里允许手工登记，登记后设备以
        离线状态出现在列表，等它第一帧数据上来就自动变在线。

        注意：这只是"占位登记"，不伪造任何传感器数据——没有数据就显示"该设备
        还没有数据"，不会编造指标。
        """
        _require(body, "deviceId")
        device_id = str(body.get("deviceId") or "").strip()
        if not device_id:
            raise ApiError(400, "BAD_DEVICE_ID", "deviceId 不能为空")
        if not re.match(r"^[A-Za-z0-9_.\-]{1,32}$", device_id):
            raise ApiError(400, "BAD_DEVICE_ID", "deviceId 只能用字母、数字、下划线、点和横线，最长 32 位")
        fields = {}
        fw = body.get("fwVersion")
        if fw:
            fields["fw_version"] = str(fw)[:32]
        self.store.upsert_device(device_id, **fields)
        sys.stderr.write("[api] registered device %s (manual add, waiting for data)\n" % device_id)
        return self.store.device_status(device_id) or {"deviceId": device_id, "online": False}

    # ---------------------------------------------------------- family（家庭组）
    def family_create(self, params, body):
        """创建家庭组：一位老人一个组，创建者自动成为“户主”。

        内存态：网关重启后组和密码都会重置，这是本期有意为之的取舍
        （局域网演示、不搭云），生产环境应换成云端账号体系。
        """
        _require(body, "familyName")
        try:
            return self.families.create(body.get("familyName"), body.get("ownerName") or "户主本人")
        except FamilyError as exc:
            raise ApiError(exc.status, exc.code, exc.message)

    def family_join(self, params, body):
        """用 6 位密码加入家庭组，加入者标记为“监护人”。"""
        _require(body, "inviteCode")
        try:
            return self.families.join(body.get("inviteCode"), body.get("memberName") or "家人")
        except FamilyError as exc:
            raise ApiError(exc.status, exc.code, exc.message)

    def family_get(self, params, body):
        """查家庭组详情：名称、成员列表（含户主/监护人）、加入密码。"""
        family_id = params.get("familyId") or ""
        if not family_id:
            raise ApiError(400, "BAD_REQUEST", "缺少必填参数：familyId")
        try:
            return self.families.get(family_id)
        except FamilyError as exc:
            raise ApiError(exc.status, exc.code, exc.message)

    def family_lookup(self, params, body):
        """加入前预检 6 位密码：只回组名和人数，不产生成员。"""
        code = params.get("inviteCode") or ""
        if not code:
            raise ApiError(400, "BAD_REQUEST", "缺少必填参数：inviteCode")
        try:
            return self.families.lookup(code)
        except FamilyError as exc:
            raise ApiError(exc.status, exc.code, exc.message)

    # ---------------------------------------------------------- snapshot
    def post_snapshot(self, params, body):
        """HTTP 通道入口（兼容旧固件）。"""
        return self.ingest_snapshot(body, channel="http")

    def ingest_snapshot(self, body, channel="http"):
        """三种通道共用：校验 -> 来源归一化 -> 入库 -> 规则判定。

        UDP / 串口进来的报文与 HTTP 走完全相同的校验，避免"两套规则"。
        """
        _require(body, "deviceId")
        ts_raw = _require(body, "ts")
        ts_dt = util.parse_ts(ts_raw)
        if ts_dt is None:
            raise ApiError(400, "BAD_TIME",
                           "ts 必须是 ISO 8601 字符串，例如 2026-11-02T14:03:00+08:00")

        # 只有"时钟跑到未来"才纠偏；补传的过去时间必须原样保留
        skew = (ts_dt - util.now()).total_seconds()
        time_adjusted = False
        if skew > 300:
            ts = util.iso_now()
            time_adjusted = True
        else:
            ts = ts_raw

        seq = util.as_int(body.get("seq"))
        if seq is None or seq <= 0:
            raise ApiError(400, "BAD_SEQ", "seq 必须是正整数（用于补传排序与去重）")

        source = normalize_source(body.get("source"))
        snap = {
            "deviceId": str(body["deviceId"]),
            "ts": ts,
            "seq": seq,
            "pirDetected": util.as_bool(body.get("pirDetected"), False),
            "lastMotionTs": body.get("lastMotionTs"),
            "lastMotionZone": body.get("lastMotionZone"),
            "temperature": util.as_float(body.get("temperature")),
            "humidity": util.as_float(body.get("humidity")),
            "lux": util.as_float(body.get("lux")),
            "doorOpen": util.as_bool(body.get("doorOpen"), None),
            "battery": util.as_float(body.get("battery")),
            "powerState": body.get("powerState"),
            "rssi": util.as_int(body.get("rssi")),
            "online": util.as_bool(body.get("online"), True),
            "source": source,
            "fwVersion": body.get("fwVersion") or "unknown",
            "timeAdjusted": time_adjusted,
        }
        self.store.save_snapshot(snap)
        alarms = self.engine.on_snapshot(snap)
        self.alert(alarms)
        return {
            "ok": True,
            "seq": seq,
            "channel": channel,
            "source": source,
            "timeAdjusted": time_adjusted,
            "alarms": [a["eventId"] for a in alarms],
        }

    # --------------------------------------------------- 板端下行确认
    def active_alarms(self, params, body):
        """板端每 10 秒问一次：我这条告警还有人待处理吗？

        固件只解析两个整数（板端不做 JSON 解析，省 RAM）：
          pendingCount      还有几条没人确认 -> LED 按等级闪烁
          acknowledgedCount 已处理过几条     -> pending 为 0 时 LED 常亮
        pending 列表一并返回，方便人肉排查“到底卡在哪一条”。
        """
        device_id = params.get("deviceId") or None
        counts = self.store.alarm_counts(device_id)
        pending = self.store.list_alarms(status="pending", limit=20,
                                         device_id=device_id)
        return {
            "deviceId": device_id,
            "pendingCount": counts["pending"],
            "acknowledgedCount": counts["handled"],
            "pending": pending,
            "serverTime": util.iso_now(),
        }

    def get_snapshot(self, params, body):
        # 多设备：带 deviceId 只看这台（APP 切换老人时用），不带则为最近上报的那台
        device_id = params.get("deviceId") or None
        snap, row = self.store.latest_snapshot(device_id)
        if snap is None:
            raise ApiError(404, "NO_DATA", "尚未收到任何快照")
        device = self.store.device_status(device_id) or {}
        snap["staleSeconds"] = device.get("staleSeconds")
        snap["online"] = device.get("online", False)

        # 首页要能“一眼看出有没有待处理的事”，所以把最高等级的待确认告警带上
        pending = self.store.list_alarms(status="pending", limit=10, device_id=device_id)
        order = {"L3": 3, "L2": 2, "L1": 1}
        top = None
        for alarm in pending:
            if top is None or order.get(alarm["level"], 0) > order.get(top["level"], 0):
                top = alarm
        snap["activeAlarm"] = None if top is None else {
            "eventId": top["eventId"],
            "level": top["level"],
            "type": top["type"],
            "message": top["message"],
            "ts": top["ts"],
        }
        return snap

    def list_snapshots(self, params, body):
        limit = util.as_int(params.get("limit"), 200)
        limit = max(1, min(limit, 1000))
        items = self.store.list_snapshots(params.get("from"), params.get("to"), limit,
                                          params.get("deviceId") or None)
        return {"items": items, "total": len(items)}

    # ------------------------------------------------------------- alarm
    def list_alarms(self, params, body):
        th = self.store.thresholds()
        self.store.escalate_stale(th.escalate_min)
        items = self.store.list_alarms(
            status=params.get("status"), from_ts=params.get("from"),
            to_ts=params.get("to"), limit=util.as_int(params.get("limit"), 100),
            device_id=params.get("deviceId") or None)
        return {"items": items, "total": len(items)}

    def get_alarm(self, params, body):
        event_id = params["event_id"]
        alarm = self.store.get_alarm(event_id)
        if alarm is None:
            raise ApiError(404, "NO_ALARM", "告警不存在：%s" % event_id)
        result = dict(alarm)
        row = None
        return result

    def ack_alarm(self, params, body):
        event_id = params["event_id"]
        alarm = self.store.get_alarm(event_id)
        if alarm is None:
            raise ApiError(404, "NO_ALARM", "告警不存在：%s" % event_id)
        if alarm["status"] != "pending":
            raise ApiError(409, "INVALID_STATE", "只有“待确认”的告警可以确认，当前状态：%s" % alarm["status"])
        updated = self.store.update_alarm(
            event_id, status="acknowledged",
            ack_ts=body.get("ackTs") or util.iso_now(),
            ack_operator=body.get("operator") or "family")
        return updated

    def close_alarm(self, params, body):
        event_id = params["event_id"]
        alarm = self.store.get_alarm(event_id)
        if alarm is None:
            raise ApiError(404, "NO_ALARM", "告警不存在：%s" % event_id)
        if alarm["status"] == "closed":
            raise ApiError(409, "INVALID_STATE", "告警已关闭，不允许重复关闭")

        handle_type = body.get("handleType")
        allowed = ("visited", "phone", "false_alarm", "community", "other")
        if handle_type not in allowed:
            raise ApiError(400, "BAD_HANDLE_TYPE", "handleType 必须是 %s 之一" % "/".join(allowed))
        handle_result = (body.get("handleResult") or "").strip()
        if not handle_result:
            raise ApiError(400, "BAD_RESULT", "必须填写处置结果")
        if len(handle_result) > 200:
            raise ApiError(400, "BAD_RESULT", "处置结果不超过 200 字")

        fields = {
            "status": "closed",
            "closed_ts": body.get("closedTs") or util.iso_now(),
            "handle_type": handle_type,
            "handle_result": handle_result,
        }
        if alarm["status"] == "pending":
            fields["ack_ts"] = util.iso_now()
            fields["ack_operator"] = body.get("operator") or "family"
        return self.store.update_alarm(event_id, **fields)

    # ------------------------------------------------------------ siren
    def _siren_thread(self):
        siren = getattr(self, "siren_control", None)
        if siren is None:
            raise ApiError(409, "NO_SIREN", "网关未启用播报：启动时加 --siren")
        return siren

    def siren_status(self, params, body):
        siren = self._siren_thread()
        mode = "off" if siren.muted else ("on" if siren.arm_all else "auto")
        try:
            pending = len(self.store.list_alarms(status="pending", limit=200))
        except Exception:
            pending = 0
        return {"mode": mode, "pending": pending}

    def siren_set(self, params, body):
        siren = self._siren_thread()
        mode = (body.get("mode") or "").strip().lower()
        if mode not in ("on", "off", "auto"):
            raise ApiError(400, "BAD_MODE", "mode 只能是 on / off / auto")
        return siren.set_mode(mode)

    # ------------------------------------------------------------ report
    def daily_report(self, params, body):
        day = params.get("date") or util.now().date().isoformat()
        try:
            target = date.fromisoformat(day)
        except ValueError:
            raise ApiError(400, "BAD_DATE", "date 格式应为 YYYY-MM-DD")

        hourly = self.store.hourly_counts(day)
        total = sum(hourly)
        prev_day = (target - timedelta(days=1)).isoformat()
        yesterday = self.store.day_count(prev_day)
        delta = util.pct_delta(total, yesterday)

        peak = None
        if total:
            peak = max(range(24), key=lambda i: hourly[i])

        baseline_days = [(target - timedelta(days=i)).isoformat() for i in range(1, 8)]
        baseline_values = [self.store.day_count(d) for d in baseline_days]
        valid = [c for c in baseline_values if c > 0]
        baseline = round(sum(valid) / float(len(valid)), 1) if valid else None

        level = "normal"
        note = ""
        if baseline:
            if total < baseline * 0.7:
                # 连续 3 日（含今日）低于基线 70% 才升级提示
                streak = 1
                for d in baseline_days[:2]:
                    if self.store.day_count(d) < baseline * 0.7:
                        streak += 1
                level = "low"
                note = ("连续 %d 日低于 7 日基线 %.0f%%，建议关注饮食与睡眠"
                        % (streak, baseline)) if streak >= 3 else "今日活动低于 7 日基线"
            elif total > baseline * 1.3:
                level = "high"
                note = "今日活动明显高于基线，可留意是否有访客或异常走动"
            else:
                note = "活动量与近 7 日基线持平"

        return {
            "date": day,
            "totalCount": total,
            "hourly": hourly,
            "deltaVsYesterday": delta,
            "peakHour": peak,
            "level": level,
            "note": note,
            "baseline": baseline,
            "alarmCount": self.store.alarm_count(day),
        }

    def weekly_report(self, params, body):
        key = params.get("week") or util.week_key(util.now())
        start, end = util.week_range(key)
        days = [(start + timedelta(days=i)).isoformat() for i in range(7)]

        counts = [self.store.day_count(d) for d in days]
        total = sum(counts)
        prev_start = start - timedelta(days=7)
        prev_total = sum(self.store.day_count((prev_start + timedelta(days=i)).isoformat())
                         for i in range(7))
        temp_avg, hum_avg = self.store.env_average(start.isoformat(), end.isoformat())

        return {
            "week": key,
            "startDate": start.isoformat(),
            "endDate": end.isoformat(),
            "totalCount": total,
            "dailyAvg": round(total / 7.0, 1),
            "dailyCounts": counts,
            "deltaVsLastWeek": util.pct_delta(total, prev_total),
            "alarmCount": sum(self.store.alarm_count(d) for d in days),
            "tempAvg": temp_avg,
            "humAvg": hum_avg,
        }

    def monthly_report(self, params, body):
        month = params.get("month") or util.now().strftime("%Y-%m")
        try:
            year, mon = [int(x) for x in month.split("-")]
            first = date(year, mon, 1)
        except (ValueError, TypeError):
            raise ApiError(400, "BAD_MONTH", "month 格式应为 YYYY-MM")
        next_month = date(year + (mon == 12), 1 if mon == 12 else mon + 1, 1)
        days = (next_month - first).days

        # 日均的分母：当月还没过完时不能拿整月天数去分摊，否则演示当天的日均
        # 会平白低 10% 以上（10/27 看 10 月，31 天里有 4 天还没发生）。
        # days      = 当月总天数（界面要显示“当月天数”）
        # daysUsed  = 实际参与计算的天数（当月=已过天数，历史月=整月）
        today = util.now().date()
        if first <= today < next_month:
            days_used = (today - first).days + 1
        else:
            days_used = days
        days_used = max(1, days_used)

        total = 0
        alarms = 0
        for i in range(days):
            d = (first + timedelta(days=i)).isoformat()
            total += self.store.day_count(d)
            alarms += self.store.alarm_count(d)
        temp_avg, hum_avg = self.store.env_average(
            first.isoformat(), (next_month - timedelta(days=1)).isoformat())
        return {
            "month": month,
            "days": days,
            "daysUsed": days_used,
            "totalCount": total,
            "dailyAvg": round(total / float(days_used), 1),
            "alarmCount": alarms,
            "tempAvg": temp_avg,
            "humAvg": hum_avg,
        }

    # ------------------------------------------------------------ config
    def get_thresholds(self, params, body):
        return self.store.thresholds().to_dict()

    def set_thresholds(self, params, body):
        if not isinstance(body, dict) or not body:
            raise ApiError(400, "BAD_REQUEST", "请求体必须包含要更新的阈值字段")
        current = self.store.thresholds().to_dict()
        merged = dict(current)
        merged.update(body)
        th = Thresholds.from_dict(merged)
        for key in ("no_motion_min", "night_no_motion_min", "night_bathroom_min",
                    "away_max_min", "door_open_max_min", "escalate_min",
                    "sleep_gap_min"):
            if getattr(th, key) <= 0:
                raise ApiError(400, "BAD_CONFIG", "%s 必须为正整数" % key)
        for start_key, end_key in (("sleep_start_hour", "sleep_end_hour"),
                                   ("nap_start_hour", "nap_end_hour")):
            value = getattr(th, start_key)
            if value < 0 or value > 23 or getattr(th, end_key) < 0 or getattr(th, end_key) > 23:
                raise ApiError(400, "BAD_CONFIG", "%s / %s 必须在 0~23 之间"
                               % (start_key, end_key))
        if abs(getattr(th, "temp_offset", 0.0)) > 15:
            raise ApiError(400, "BAD_CONFIG", "temp_offset 超出合理范围（±15）")
        if abs(getattr(th, "hum_offset", 0.0)) > 30:
            raise ApiError(400, "BAD_CONFIG", "hum_offset 超出合理范围（±30）")
        self.store.save_thresholds(th.to_dict())
        return {"ok": True, "thresholds": th.to_dict(), "appliedAt": util.iso_now()}

    def alert(self, alarms):
        """模拟板端本地声光：蜂鸣 + 语音播报。

        真机上由固件驱动 LED 与 BY8301 语音模块；这里用电脑代替，
        方便在没有硬件时演示“告警一响，现场有声有光”的效果。
        """
        if not alarms:
            return
        first = alarms[0]
        if self.siren:
            try:
                import winsound
                winsound.Beep(1200, 350)
                winsound.Beep(900, 350)
            except Exception:
                pass
        if self.voice:
            try:
                import subprocess
                text = str(first.get("message", "")).replace("'", "")
                script = "(New-Object -ComObject SAPI.SpVoice).Speak('%s')" % text
                subprocess.Popen(["powershell", "-NoProfile", "-Command", script],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception:
                pass

    # ------------------------------------------------------------ events
    def post_event(self, params, body):
        _require(body, "type")
        body["source"] = normalize_source(body.get("source"))
        alarm = self.engine.on_event(body)
        if alarm is not None:
            self.alert([alarm])
        if alarm is None:
            return {"ok": True, "created": None,
                    "note": "该类型不直接建单，或同类型告警仍在活动期"}
        return {"ok": True, "created": alarm}
