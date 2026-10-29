# -*- coding: utf-8 -*-
"""告警规则引擎：三档告警 + 三条融合规则。

与固件侧保持同一套判定口径，网关再算一遍的理由：
1) 硬件未到货时，可用模拟器把算法先验证完；
2) 设备重启/断线后，网关仍能基于最后一张快照给出结论。

三条融合规则（对应设计文档 3.3）：
    ① 外出抑制：门磁 开→关 且随后无活动 ⇒ 疑似外出，不报 L2
    ② 时段加权：夜间阈值收紧，卫生间权重最高（直接给 L3）
    ③ 冷却去重：同类型告警在活动期内不重复建单
"""
from __future__ import annotations

from gateway import util
from gateway.config import BOARD_EVENT_TYPES, DIRECT_EVENT_TYPES

def _note(msg):
    """规则引擎的诊断日志（带时间戳，方便对着网关窗口看）。"""
    print("[%s] [rules] %s" % (util.to_iso(util.now())[11:19], msg), flush=True)


ZONE_LABEL = {
    "living": "客厅",
    "bedroom": "卧室",
    "bathroom": "卫生间",
    "kitchen": "厨房",
    "entry": "入户",
    "unknown": "未知区域",
    None: "未知区域",
}


# 板端事件没有携带等级时的兜底档位：火情类一律 L3，其余按风险给 L2/L1
BOARD_LEVEL = {
    "TEMP_FIRE": "L3", "TEMP_RISE": "L3",
    "TEMP_HIGH": "L2", "TEMP_LOW": "L2", "HUM_HIGH": "L2", "HUM_LOW": "L2",
    "DOOR_OPEN": "L1", "TEMP_WARM": "L1",
    "HUM_WARN_HIGH": "L1", "HUM_WARN_LOW": "L1",
}

BOARD_TEXT = {
    "TEMP_FIRE": "室内温度异常偏高，疑似火情，请立即确认",
    "TEMP_RISE": "室内温度短时快速上升，疑似火情，请立即确认",
    "TEMP_HIGH": "室内温度过高",
    "TEMP_LOW": "室内温度过低",
    "HUM_HIGH": "室内湿度过高",
    "HUM_LOW": "室内湿度过低",
    "DOOR_OPEN": "门窗长时间未关",
    "TEMP_WARM": "室内偏热",
    "HUM_WARN_HIGH": "室内偏潮",
    "HUM_WARN_LOW": "室内偏干",
}


def zone_text(zone):
    return ZONE_LABEL.get(zone, zone or "未知区域")


def is_night(hour, th):
    return hour >= th.night_start_hour or hour < th.night_end_hour


def is_sleep(hour, th):
    """睡眠时段：夜间睡眠 + 可选午睡。人在睡觉，长时间不动是正常的。"""
    start = getattr(th, "sleep_start_hour", 23)
    end = getattr(th, "sleep_end_hour", 7)
    if hour >= start or hour < end:
        return True
    if getattr(th, "nap_enabled", 1):
        nap_start = getattr(th, "nap_start_hour", 13)
        nap_end = getattr(th, "nap_end_hour", 15)
        if nap_start <= hour < nap_end:
            return True
    return False


def eff(th, name, fallback):
    """实际生效阈值 = 基础值 + 地区气候偏移。

    温度类偏移 temp_offset，湿度类偏移 hum_offset，
    这样"炎热地区/潮湿地区"不用各写一套规则。
    """
    base = getattr(th, name, fallback)
    if name.startswith("temp"):
        return base + getattr(th, "temp_offset", 0.0)
    if name.startswith("hum"):
        return base + getattr(th, "hum_offset", 0.0)
    return base


def in_daylight(hour):
    return 8 <= hour < 18


class Engine(object):
    def __init__(self, store):
        self.store = store
        # 连续性跟踪：设备重启 / 网关重启 / 长时间断档后，
        # “连续多久没活动”不能拿断档前的旧时间戳算，否则一启动就误报。
        self._watch = {}          # device_id -> 本次连续观察起点（第一张实时帧的时间）
        # 本进程里“亲眼看到门磁由关变开”的设备：这种门开启时间是自己观测的，
        # 不该再按“连续观察起点”截断；重启前就在库里的旧门磁状态才需要截断。
        self._door_learned = set()
        self._warm_noted = set()  # 已经提示过“预热中”的设备

    # ------------------------------------------------------------------
    # 快照入口：可能产出 0..N 条告警
    # ------------------------------------------------------------------
    def on_snapshot(self, snap):
        th = self.store.thresholds()
        device_id = snap.get("deviceId")
        ts_text = snap.get("ts")
        ts = util.parse_ts(ts_text)
        if ts is None:
            return []

        # --- 连续性判定：区分“实时帧”和“回放 / 补传帧”
        srv = util.now()
        tol = float(getattr(th, "realtime_tolerance_sec", 120))
        warm_sec = float(getattr(th, "startup_warmup_sec", 180))
        realtime = abs((ts - srv).total_seconds()) <= tol
        watch = self._watch.get(device_id)
        if realtime and watch is None:
            watch = ts
            self._watch[device_id] = watch
        warming = bool(realtime and watch is not None
                       and (ts - watch).total_seconds() < warm_sec)
        if warming and device_id not in self._warm_noted:
            self._warm_noted.add(device_id)
            _note("%s 首次实时上报，%d 秒内不判时间累积型告警（避免重启后误报）"
                  % (device_id, int(warm_sec)))

        dev = self.store.get_device(device_id) or {}
        created = []

        pir = bool(snap.get("pirDetected"))
        door = snap.get("doorOpen")
        # 设备状态是从 sqlite 读回来的：door_open 存的是 1/0，
        # 直接写 `is True` 永远不成立，会让开→关跳变和门磁持续开启全部失灵。
        prev_door = dev.get("door_open")
        if prev_door is not None:
            prev_door = bool(prev_door)
        # 设备自报的 lastMotionTs 是“最后活动时间”的权威值（真机由固件算好），
        # 优先用它；只有帧里没带这个字段（老固件/精简帧）才退回库里缓存。
        last_motion = snap.get("lastMotionTs") or dev.get("last_motion_ts")
        # 补传 / 回放帧（ts 明显早于现在）代表“过去某一刻的现场”，
        # 那一刻以帧里自带的 lastMotionTs 为准；不能用档案里“最新的活动时间”
        # 去顶替它，否则历史注入（演示脚本）永远算不出“已经无活动多久”。
        replay_motion = None
        if (not realtime) and snap.get("lastMotionTs") is not None:
            replay_motion = snap.get("lastMotionTs")
            last_motion = replay_motion
        last_zone = dev.get("last_zone")
        away_since = dev.get("away_since")
        door_since = dev.get("door_open_since")
        source = snap.get("source", "real")

        # --- 活动与门磁状态机
        if pir:
            last_motion = ts_text
            last_zone = snap.get("lastMotionZone") or last_zone
            away_since = None
            self.store.add_activity(ts_text, last_zone, "motion", 1, source)
        if door is not None:
            door = bool(door)
            if door:
                away_since = None
                if prev_door is not True:
                    door_since = ts_text
                    self._door_learned.add(device_id)
            else:
                if prev_door is True:
                    # 开→关的关键歧义：可能是“出门”（外出）也可能是“回家”。
                    # 同一帧里 PIR 也在动 = 人刚进门把门带上 → 回家，清掉外出态；
                    # 只有“门关且没检测到活动”才按疑似外出处理。
                    if pir:
                        away_since = None
                    else:
                        away_since = ts_text
                    self.store.add_activity(ts_text, "entry", "door", 1, source)
                door_since = None

        # --- ① 无活动判定（受外出模式抑制；门开着不叠加，见下）
        if last_motion and not warming and not door:
            last_dt = util.parse_ts(last_motion)
            if last_dt is not None:
                # 断档 / 重启后的静默计时从“本次连续观察起点”算
                last_dt = self._silence(device_id, ts, last_dt)
                delta_min = (ts - last_dt).total_seconds() / 60.0
                # 时钟回拨/前跳时 delta 会变成负数或超大值，直接跳过本轮判定
                if delta_min < 0 or delta_min > 7 * 24 * 60:
                    delta_min = -1
                night = is_night(ts.hour, th)
                away = away_since is not None
                zone = last_zone or "unknown"

                # 睡眠时段：不动是正常的，只有“超长静默”才需要确认
                if (not away and delta_min > 0 and is_sleep(ts.hour, th)
                        and (last_zone or "unknown") != "bathroom"):
                    gap = getattr(th, "sleep_gap_min", 360)
                    if delta_min >= gap:
                        msg = "睡眠时段已连续 %d 小时无活动，请确认情况" % int(delta_min / 60)
                        alarm = self._emit("SLEEP_GAP", "L2", msg, ts_text, device_id,
                                           zone_text(last_zone or "bedroom"), source, snap)
                        if alarm:
                            created.append(alarm)
                elif not away and delta_min > 0:
                    if night:
                        limit = th.night_bathroom_min if zone == "bathroom" else th.night_no_motion_min
                        level = "L3" if zone == "bathroom" else "L2"
                    else:
                        limit = th.no_motion_min
                        level = "L2"
                    if delta_min >= limit:
                        msg = "%s连续 %d 分钟未检测到活动" % (zone_text(zone), int(delta_min))
                        if night and zone == "bathroom":
                            msg = "夜间卫生间连续 %d 分钟未检测到活动" % int(delta_min)
                        alarm = self._emit("NO_MOTION", level, msg, ts_text, device_id,
                                           zone, source, snap)
                        if alarm:
                            created.append(alarm)
                elif away:
                    away_dt = util.parse_ts(away_since)
                    away_dt = self._silence(device_id, ts, away_dt)
                    if away_dt is not None:
                        away_min = (ts - away_dt).total_seconds() / 60.0
                        if away_min >= th.away_max_min:
                            msg = "疑似外出已 %d 分钟，仍未检测到返回" % int(away_min)
                            alarm = self._emit("AWAY_LONG", "L1", msg, ts_text, device_id,
                                               "entry", source, snap)
                            if alarm:
                                created.append(alarm)

        # --- 门窗持续开启
        # 门磁有两种来源：
        #  a) 本进程亲眼看到“关→开”（例如演示脚本注入 12 分钟前开门）：直接按它的时间算；
        #  b) 库里的旧状态（网关重启前门就开着）：按连续观察起点截断，避免一启动就报。
        if door and door_since:
            since = util.parse_ts(door_since)
            if device_id not in self._door_learned:
                since = self._silence(device_id, ts, since)
            if since is not None:
                open_min = (ts - since).total_seconds() / 60.0
                if open_min >= th.door_open_max_min:
                    msg = "门窗已持续开启 %d 分钟" % int(open_min)
                    alarm = self._emit("DOOR_OPEN", "L1", msg, ts_text, device_id,
                                       "entry", source, snap)
                    if alarm:
                        created.append(alarm)

        # --- 环境：温度（火情 / 过高 / 偏热 / 过低）
        temp = snap.get("temperature")
        if temp is not None:
            temp = float(temp)
            fire_at = eff(th, "temp_fire", 55.0)
            rise_need = getattr(th, "temp_rise_delta", 8.0)
            rise_win = getattr(th, "temp_rise_window", 180)
            rise = self._temp_rise(device_id, ts, temp, rise_win)

            if temp >= fire_at:
                created.append(self._emit(
                    "TEMP_FIRE", "L3",
                    "疑似火情：室内温度 %.1f℃，已超过火情阈值 %.1f℃" % (temp, fire_at),
                    ts_text, device_id, "unknown", source, snap))
            elif rise is not None and rise >= rise_need:
                created.append(self._emit(
                    "TEMP_FIRE", "L3",
                    "疑似火情：%d 分钟内升温 %.1f℃（当前 %.1f℃）"
                    % (int(rise_win / 60), rise, temp),
                    ts_text, device_id, "unknown", source, snap))
            elif temp >= eff(th, "temp_high", 35.0):
                created.append(self._emit(
                    "TEMP_HIGH", "L2",
                    "室内温度 %.1f℃，高于安全阈值 %.1f℃，请检查取暖设备"
                    % (temp, eff(th, "temp_high", 35.0)),
                    ts_text, device_id, "unknown", source, snap))
            elif temp >= eff(th, "temp_warm", 28.0):
                created.append(self._emit(
                    "TEMP_WARM", "L1",
                    "室内温度 %.1f℃，偏热，建议通风" % temp,
                    ts_text, device_id, "unknown", source, snap))
            elif temp < eff(th, "temp_low", 16.0):
                created.append(self._emit(
                    "TEMP_LOW", "L2",
                    "室内温度 %.1f℃，低于 %.1f℃，注意保暖" % (temp, eff(th, "temp_low", 16.0)),
                    ts_text, device_id, "unknown", source, snap))

        # --- 湿度：过高 / 偏高 / 过低 / 偏低
        hum = snap.get("humidity")
        if hum is not None:
            hum = float(hum)
            if hum >= eff(th, "hum_high", 80.0):
                created.append(self._emit(
                    "HUM_HIGH", "L2",
                    "室内湿度 %.0f%%，高于 %.0f%%，注意防潮" % (hum, eff(th, "hum_high", 80.0)),
                    ts_text, device_id, "unknown", source, snap))
            elif hum >= eff(th, "hum_warn_high", 70.0):
                created.append(self._emit(
                    "HUM_WARN_HIGH", "L1",
                    "室内湿度 %.0f%%，偏潮" % hum,
                    ts_text, device_id, "unknown", source, snap))
            elif hum <= eff(th, "hum_low", 25.0):
                created.append(self._emit(
                    "HUM_LOW", "L2",
                    "室内湿度 %.0f%%，低于 %.0f%%，注意补水" % (hum, eff(th, "hum_low", 25.0)),
                    ts_text, device_id, "unknown", source, snap))
            elif hum <= eff(th, "hum_warn_low", 35.0):
                created.append(self._emit(
                    "HUM_WARN_LOW", "L1",
                    "室内湿度 %.0f%%，偏干" % hum,
                    ts_text, device_id, "unknown", source, snap))

        # --- 光照：夜间异常强光 / 白天采光不足
        lux = snap.get("lux")
        if lux is not None:
            lux = float(lux)
            if is_night(ts.hour, th) and lux >= getattr(th, "lux_night_high", 200.0):
                created.append(self._emit(
                    "LUX_NIGHT_HIGH", "L1",
                    "夜间光照 %.0f lx，明显偏亮，可能忘关灯" % lux,
                    ts_text, device_id, "unknown", source, snap))
            elif in_daylight(ts.hour) and lux <= getattr(th, "lux_day_low", 20.0):
                created.append(self._emit(
                    "LUX_DAY_LOW", "L1",
                    "白天光照仅 %.0f lx，采光不足" % lux,
                    ts_text, device_id, "unknown", source, snap))

        # --- 回写设备状态（供离线判断与首页展示）
        # last_seen 必须是“网关收到数据的时刻”，不能是设备上报的 ts：
        # 补传历史帧时 ts 可能是几小时前，用它会立刻把设备判成离线。
        self.store.upsert_device(
            device_id,
            online=1,
            last_seen=util.iso_now(),
            battery=snap.get("battery"),
            power_state=snap.get("powerState"),
            rssi=snap.get("rssi"),
            fw_version=snap.get("fwVersion"),
            # 回放帧不把档案里的“最新活动”往回退，避免真机补传后判重出错
            last_motion_ts=(dev.get("last_motion_ts")
                            if replay_motion is not None and dev.get("last_motion_ts")
                            else last_motion),
            last_zone=last_zone,
            door_open=None if door is None else (1 if door else 0),
            door_open_since=door_since,
            away_since=away_since,
        )

        return [a for a in created if a]

    # ------------------------------------------------------------------
    # 设备直报事件（求助 / 燃气 / 烟雾），不经阈值
    # ------------------------------------------------------------------
    def on_event(self, event):
        etype = (event.get("type") or "").upper()
        ts_text = event.get("ts") or util.iso_now()
        device_id = event.get("deviceId")
        source = event.get("source", "real")
        payload = event.get("payload") or {}

        if etype in BOARD_EVENT_TYPES:
            return self.on_board_event(etype, ts_text, device_id, source, payload)
        if etype not in DIRECT_EVENT_TYPES:
            return None

        if etype == "SOS":
            message = "紧急求助：老人按下随身求助按钮"
            level = "L3"
        elif etype == "GAS":
            message = "燃气泄漏报警，请立即确认"
            level = "L3"
        else:
            message = "烟雾报警，请立即确认"
            level = "L3"

        note = payload.get("message")
        if note:
            message = "%s（%s）" % (message, note)

        return self._emit(etype, level, message, ts_text, device_id,
                          payload.get("zone"), source, payload)

    def on_board_event(self, etype, ts_text, device_id, source, payload):
        """板端本地阈值判定结果入库。

        板端判完为什么还要走网关？
          - 板端只是“单指标 + 时间窗”的快速反应，断网时靠它自己响；
          - 告警台账、家属确认、外出抑制、冷却去重都在网关，这里才是唯一入口；
          - 所以板端事件也必须经过 _emit 的去重，否则板端每变一次就建一条单。
        等级优先采信板端上报的 payload.level，没有就按类型给默认档。
        """
        level = (payload.get("level") or "").upper()
        if level not in ("L1", "L2", "L3"):
            level = BOARD_LEVEL.get(etype, "L2")
        message = payload.get("message") or BOARD_TEXT.get(etype, etype)
        return self._emit(etype, level, message, ts_text, device_id,
                          payload.get("zone"), source, payload)

    # ------------------------------------------------------------------
    def _temp_rise(self, device_id, ts, temp, window_sec):
        """窗口内相对最低温的升幅，用于区分“着火”与“正常升温”。"""
        try:
            rows = self.store.list_snapshots(limit=30)
        except Exception:
            return None
        best = None
        for row in rows:
            if row.get("deviceId") != device_id:
                continue
            value = row.get("temperature")
            if value is None:
                continue
            row_ts = util.parse_ts(row.get("ts"))
            if row_ts is None:
                continue
            delta = (ts - row_ts).total_seconds()
            if 0 < delta <= window_sec:
                rise = temp - float(value)
                best = rise if best is None else max(best, rise)
        return best

    def _silence(self, device_id, ts, raw_dt):
        """时间累积类判定的起点。

        规则：
        1) 设备一直在连续上报（上次上报离本帧不超过 60 秒）→ 相信设备自报的
           “已经多久没动”，这是固件的权威结论，网关不擅自压缩。
        2) 真的断档了（设备重启、掉线补传、网关停机很久）→ 静默计时从“本次连续
           观察起点”重新算，避免一启动就拿历史时间戳直接报 L2/L3。
        3) 回放帧（ts 不晚于观察起点）保持原值，保证演示脚本的历史注入成立。
        """
        if raw_dt is None:
            return raw_dt
        watch = self._watch.get(device_id)
        last_seen = None
        dev = self.store.get_device(device_id) or {}
        if dev.get("last_seen"):
            last_seen = util.parse_ts(dev.get("last_seen"))
        continuous = (last_seen is not None and ts is not None
                      and 0 <= (ts - last_seen).total_seconds() <= 60)
        if continuous:
            return raw_dt
        if watch is None or ts <= watch:
            return raw_dt
        return raw_dt if raw_dt > watch else watch

    def _emit(self, alarm_type, level, message, ts_text, device_id, zone, source, snapshot):
        """③ 冷却去重：同类型告警处于活动期时不重复建单。"""
        if self.store.active_alarm(alarm_type, device_id):
            return None
        # 证据：触发这一刻的传感器读数。APP 详情页用它们做“证据链”，
        # 让家属/评委能复核“当时到底是多少度、门是不是开着”，而不是只看到一句结论。
        evidence = None
        if snapshot:
            evidence = util.dump({
                "temperature": snapshot.get("temperature"),
                "humidity": snapshot.get("humidity"),
                "lux": snapshot.get("lux"),
                "pirDetected": snapshot.get("pirDetected"),
                "doorOpen": snapshot.get("doorOpen"),
                "online": snapshot.get("online"),
            })
        return self.store.create_alarm(
            alarm_type=alarm_type, level=level, message=message, ts=ts_text,
            device_id=device_id, zone=zone, source=source, snapshot=snapshot,
            evidence=evidence)
