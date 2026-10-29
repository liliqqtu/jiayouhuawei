/**
 * @file    app_main.c
 * @brief   应用入口：采集判定与上报分成两个任务，互不阻塞。
 *
 * 任务划分：
 *   SensorTask  每 CFG_SAMPLE_MS 一次：
 *                PIR / 按键 / 门磁 -> 状态机 -> **板端本地阈值判定** -> 本地声光
 *                环境传感器（I2C）按 CFG_ENV_SAMPLE_MS 采样，由本任务独占总线
 *   UploadTask  每 CFG_UPLOAD_MS 一次：读缓存好的环境值 -> 拼 JSON -> 上报
 *                每 CFG_ACK_POLL_MS 问一次网关“我的告警还有人处理吗”（下行确认）
 *
 * 四条硬约束：
 *   1. 断网时采集、本地阈值判定、本地声光照常运行（SensorTask 完全不碰网络）；
 *   2. 读不到的传感器一律写 null，绝不用 0 或默认值填充；
 *   3. 按键求助的数据 source=test，真实传感器数据 source=hardware，界面据此标注；
 *   4. 板端判定只是“本地快速反应”，告警台账与融合规则仍以网关为准：
 *      板端判出来的等级会作为事件补报给网关，由网关再套外出抑制/夜间加权/冷却去重。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_time.h"

#include "config.h"
#include "drv.h"
#include "env.h"
#include "biz.h"
#include "net.h"
#include "sntp.h"
#include "log.h"
#include "local_rule.h"

#define JSON_SIZE 768

static char g_json[JSON_SIZE];

/* 门磁模拟状态：仅 CFG_DOOR_MODE==2 时使用（长按按键切换） */
static int g_door_sim = 0;

/* 最近一次门磁状态：SensorTask 写，UploadTask 读（两个任务共享，volatile 够用） */
static volatile int g_door_open = 0;

/* 下行确认结果：UploadTask 写，SensorTask 读
 *   g_ack_pending > 0  -> 网关说“这条告警还没人处理”，LED 闪烁
 *   g_ack_acked   > 0  -> 网关说“已有人全部处理完”，LED 常亮
 * 两个都是 0 表示“还没问到过”，此时按“未处理”处理，宁可多闪也不假装已处理。 */
static volatile int g_ack_pending = 0;
static volatile int g_ack_acked = 0;

static hi_u32 now_ms(void)
{
    return hi_get_milli_seconds();
}

static void last_motion_iso(char *buf, int size)
{
    unsigned int ago = Biz_LastMotionAgoSec(now_ms());
    unsigned int now = Sntp_NowSec();
    unsigned int lm = (now > ago) ? (now - ago) : 0;
    Sntp_IsoOfSec(lm, buf, size);
}

/**
 * 数值或 null：读取失败时输出 null，不伪造。
 *
 * 注意：这里用整数运算拼“一位小数”，不用 %f。
 * Hi3861 的 printf 默认不链接浮点格式化，%f 在板子上会输出乱码，
 * 拼出来的 JSON 就坏了（网关解析失败，而且日志里看不出异常）。
 */
static void num_or_null(char *out, int size, int ok, float value, int decimals)
{
    (void)decimals;     /* 统一上报一位小数，温度/湿度/光照不需要更高精度 */
    if (!ok) {
        snprintf(out, size, "null");
        return;
    }
    long scaled = (long)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    long whole = scaled / 10;
    long frac = scaled % 10;
    if (frac < 0) {
        frac = -frac;
    }
    snprintf(out, size, "%ld.%ld", whole, frac);
}

static const char *frame_source(void)
{
    /* 只要这一帧含模拟字段，就必须整体标 test，不冒充硬件 */
    if (CFG_DOOR_MODE == 2) {
        return "test";
    }
    return "hardware";
}

/**
 * 拼一帧快照。
 * 注意：环境值不再在这里读 I2C，而是用 SensorTask 缓存好的最近一次读数
 * （I2C 总线不能被两个任务同时访问）。
 */
static void build_snapshot(char *buf, int size, int pir)
{
    char ts[32];
    char lm[32];
    char t[24];
    char h[24];
    char lux[24];
    float tv = 0.0f;
    float hv = 0.0f;
    float lv = 0.0f;
    int ok_th = 0;
    int ok_lux = 0;
    int has_door = (CFG_DOOR_MODE != 0);

    Sntp_ToIso(ts, sizeof(ts));
    last_motion_iso(lm, sizeof(lm));

    Loc_LatestEnv(&tv, &hv, &ok_th);
    ok_lux = (Env_ReadLux(&lv) == 0);   /* 光照只在联网任务里读，仍归本任务独享 I2C */

    num_or_null(t, sizeof(t), ok_th, tv, 1);
    num_or_null(h, sizeof(h), ok_th, hv, 1);
    num_or_null(lux, sizeof(lux), ok_lux, lv, 1);

    snprintf(buf, size,
        "{\"deviceId\":\"%s\",\"ts\":\"%s\",\"seq\":%u,"
        "\"pirDetected\":%s,\"lastMotionTs\":\"%s\","
        "\"lastMotionZone\":\"unknown\","
        "\"temperature\":%s,\"humidity\":%s,\"lux\":%s,"
        "\"doorOpen\":%s,\"battery\":null,\"powerState\":\"mains\","
        "\"rssi\":null,\"online\":true,\"source\":\"%s\",\"fwVersion\":\"%s\"}",
        CFG_DEVICE_ID, ts, Biz_NextSeq(),
        pir ? "true" : "false", lm,
        t, h, lux,
        has_door ? (g_door_open ? "true" : "false") : "null",
        frame_source(), CFG_FW_VERSION);
}

static void build_event(EvtType type, const char *ts, char *buf, int size)
{
    const char *name = (type == EVT_SOS) ? "SOS" : "NO_MOTION";
    /* 按键求助属于演示输入 -> test；由 PIR 推导的无活动 -> hardware */
    const char *src = (type == EVT_SOS) ? "test" : "hardware";

    snprintf(buf, size,
        "{\"deviceId\":\"%s\",\"ts\":\"%s\",\"type\":\"%s\",\"source\":\"%s\","
        "\"payload\":{\"zone\":\"unknown\",\"status\":\"%s\"}}",
        CFG_DEVICE_ID, ts, name, src, Biz_StatusText());
}

/**
 * 拼一条“板端本地判定”事件。
 * 类型名与 gateway/rules.py 的告警类型同名，网关不用改代码就能认。
 * 板端判定用的是真实传感器值，所以 source=hardware。
 */
static void build_local_event(const char *type, const char *level, const char *text,
                              int has_value, float value, char *buf, int size)
{
    char ts[32];
    char val[24];

    Sntp_ToIso(ts, sizeof(ts));
    num_or_null(val, sizeof(val), has_value, value, 1);

    snprintf(buf, size,
        "{\"deviceId\":\"%s\",\"ts\":\"%s\",\"type\":\"%s\",\"source\":\"hardware\","
        "\"payload\":{\"zone\":\"unknown\",\"level\":\"%s\",\"value\":%s,\"message\":\"%s\"}}",
        CFG_DEVICE_ID, ts, type, level, val, text);
}

/** 从 JSON 文本里取一个整数字段（例如 "pendingCount":1）；
 *  取不到返回 -1 —— 负值时调用方保持原状态，绝不瞎改。 */
static int json_int(const char *json, const char *key)
{
    const char *p = strstr(json, key);
    if (p == NULL) {
        return -1;
    }
    p += strlen(key);
    while (*p == ' ') {
        p++;
    }
    return atoi(p);
}

/* ------------------------------------------------------------------ */
/* 下行确认：问网关“我这条告警还有人待处理吗”                            */
/* ------------------------------------------------------------------ */
static void poll_ack(void)
{
    char path[96];
    char resp[192];

    snprintf(path, sizeof(path), "/api/v1/alarms/active?deviceId=%s", CFG_DEVICE_ID);
    memset(resp, 0, sizeof(resp));
    if (Net_GetJson(path, resp, sizeof(resp)) != 0) {
        return;     /* 连不上就保持原状态：没人确认过，继续闪 */
    }

    int pending = json_int(resp, "\"pendingCount\":");
    int acked = json_int(resp, "\"acknowledgedCount\":");
    if (pending < 0) {
        return;     /* 响应格式不对，同样不动状态 */
    }

    g_ack_pending = pending;
    /* 只有“确实有告警、且全部处理完了”才算已接手；一条告警都没有时不该常亮 */
    g_ack_acked = (pending == 0 && acked > 0) ? 1 : 0;

    LOGI("ack", "gateway says pending=%d acked=%d", pending, acked);
}

/* ------------------------------------------------------------------ */
/* 本地声光                                                            */
/* ------------------------------------------------------------------ */
/** 综合等级：无活动/SOS 这类状态机告警 + 本地阈值判定，取更严重的那个 */
static LocLevel board_level(void)
{
    LocLevel loc = Loc_Level();
    if (Biz_AlarmActive()) {
        LocLevel biz = (Biz_AlarmType() == EVT_SOS) ? LOC_L3 : LOC_L2;
        return (biz > loc) ? biz : loc;
    }
    return loc;
}

/**
 * LED 三态（单色灯，不假装能亮绿）：
 *   无告警           -> 灭
 *   有待确认告警     -> 按等级闪：L1 800ms / L2 400ms / L3 150ms
 *   已被家属处理     -> 常亮（“有人接手了”）
 * 蜂鸣器：仅 CFG_BUZZER_EN=1 且等级为 L3 时响，没接就是纯光告警。
 */
static void led_apply(hi_u32 now, LocLevel level, int pending)
{
    static hi_u32 tick = 0;
    static hi_u8 on = 0;

    if (level == LOC_NONE) {
        if (on) {
            on = 0;
            Drv_LedSet(0);
        }
        Drv_BuzzerSet(0);
        return;
    }

    if (!pending) {
        /* 已有人接手：常亮，不再闪、不再响，避免越帮越乱 */
        if (!on) {
            on = 1;
            Drv_LedSet(1);
        }
        Drv_BuzzerSet(0);
        return;
    }

    hi_u32 period = (level == LOC_L3) ? 150 : ((level == LOC_L2) ? 400 : 800);
    if ((now - tick) >= period) {
        tick = now;
        on = (hi_u8)!on;
        Drv_LedSet(on);
    }
    Drv_BuzzerSet((level == LOC_L3 && on) ? 1 : 0);
}

/** 本地告警事件要不要带数值：门磁没有“数值” */
static int local_has_value(void)
{
    return (Loc_Kind() != LOC_KIND_DOOR_OPEN) ? 1 : 0;
}

/**
 * 本地告警事件带哪个数值：温度类带温度、湿度类带湿度。
 * 之前这里一律写“升温幅度”，会在 APP 详情页显示成“湿度 0.0%”这种怪数字。
 */
static float local_value(void)
{
    float t = 0.0f;
    float h = 0.0f;
    int ok = 0;

    Loc_LatestEnv(&t, &h, &ok);
    if (!ok) {
        return 0.0f;
    }
    switch (Loc_Kind()) {
        case LOC_KIND_TEMP_FIRE:
        case LOC_KIND_TEMP_RISE:
        case LOC_KIND_TEMP_HIGH:
        case LOC_KIND_TEMP_LOW:
        case LOC_KIND_TEMP_WARN:
            return t;
        case LOC_KIND_HUM_HIGH:
        case LOC_KIND_HUM_LOW:
        case LOC_KIND_HUM_WARN_HIGH:
        case LOC_KIND_HUM_WARN_LOW:
            return h;
        default:
            return 0.0f;
    }
}

/* ------------------------------------------------------------------ */
/* 任务 1：采集 + 判定 + 本地告警（不依赖网络）                          */
/* ------------------------------------------------------------------ */
static void SensorTask(void *arg)
{
    (void)arg;
    LOGI("sensor", "task start, sample=%dms env=%dms", CFG_SAMPLE_MS, CFG_ENV_SAMPLE_MS);

    hi_u32 key_tick = 0;
    hi_u8 key_prev = 0;
    hi_u32 env_tick = now_ms();
    hi_u32 local_seq = 0;

    while (1) {
        hi_u32 now = now_ms();

        /* PIR：刷新最后活动时间 */
        if (Drv_PirTriggered()) {
            Biz_OnMotion(now);
        }

        /* 按键：短按求助；长按 2 秒在“门磁模拟”模式下切换门磁 */
        hi_u8 down = Drv_KeyDown();
        if (down && !key_prev) {
            key_tick = now;
        }
        if (!down && key_prev) {
            hi_u32 held = now - key_tick;
            if (CFG_DOOR_MODE == 2 && held >= 2000) {
                g_door_sim = !g_door_sim;
                g_door_open = g_door_sim;
                g_ack_acked = 0;            /* 新情况：重新按“待确认”显示 */
                LOGI("door", "simulated door toggled to %d (source=test)", g_door_sim);
            } else if (held >= 30) {
                LOGI("key", "SOS pressed, held=%lums", (unsigned long)held);
                Biz_OnSos(now);
                g_ack_acked = 0;            /* 新的求助：先闪，等网关说已处理 */
            }
        }
        key_prev = down;

        /* 门磁状态（接了才更新；没接时 doorOpen 上报 null） */
        if (CFG_DOOR_MODE == 1) {
            g_door_open = (Drv_DoorOpen() > 0) ? 1 : 0;
        }

        /* 环境采样：I2C 由本任务独占，UploadTask 只读缓存 */
        if ((now - env_tick) >= CFG_ENV_SAMPLE_MS) {
            float tv = 0.0f;
            float hv = 0.0f;
            int ok = (Env_ReadTempHum(&tv, &hv) == 0);
            env_tick = now;
            Loc_SampleEnv(now, ok, tv, hv);
            Loc_SampleDoor(now, (CFG_DOOR_MODE != 0), g_door_open);
        }

        /* 板端阈值判定（带确认/解除时间窗） */
        Loc_Eval(now);

        /* 判定结果变化时补报事件：网关据此入库，同时也是一次“板端已识别”的证据 */
        if (Loc_ChangeSeq() != local_seq) {
            local_seq = Loc_ChangeSeq();
            if (Loc_Level() != LOC_NONE) {
                char ts[32];
                char body[JSON_SIZE];
                int rise_t10 = (int)(Loc_RiseC() * 10.0f + 0.5f);
                Sntp_ToIso(ts, sizeof(ts));
                build_local_event(Loc_KindAlarmType(), Loc_LevelText(), Loc_KindText(),
                                  local_has_value(), local_value(),
                                  body, sizeof(body));
                LOGW("local", "level=%s kind=%s rise=%d.%dC -> %s",
                     Loc_LevelText(), Loc_KindAlarmType(),
                     rise_t10 / 10, rise_t10 % 10, Loc_KindText());
                printf("%s%s\r\n", JSON_MARK, body);
                if (Net_IsOnline()) {
                    if (Net_PostJson("/api/v1/events", body) != 0) {
                        Net_PushCache(body);
                    }
                } else {
                    Net_PushCache(body);
                }
            } else {
                LOGI("local", "level cleared, back to normal");
            }
        }

        /* 无活动 / 求助状态机（原有逻辑） */
        EvtType evt = Biz_Poll(now);
        if (evt != EVT_NONE) {
            char ts[32];
            char body[JSON_SIZE];
            Sntp_ToIso(ts, sizeof(ts));
            build_event(evt, ts, body, sizeof(body));
            LOGW("biz", "alarm %s status=%s",
                 evt == EVT_SOS ? "SOS" : "NO_MOTION", Biz_StatusText());
            g_ack_acked = 0;                /* 新告警，重新按待确认显示 */

            /* 串口备用通道：无论网络是否可用都输出 */
            printf("%s%s\r\n", JSON_MARK, body);

            if (Net_IsOnline()) {
                if (Net_PostJson("/api/v1/events", body) == 0) {
                    Biz_MarkReported();
                } else {
                    Net_PushCache(body);
                }
            } else {
                Net_PushCache(body);
            }
        }

        /* 本地声光：等级 + 是否已被网关确认 */
        {
            int pending = (g_ack_pending > 0) || (g_ack_acked == 0);
            led_apply(now, board_level(), pending);
        }

        osDelay(CFG_SAMPLE_MS);
    }
}

/* ------------------------------------------------------------------ */
/* 任务 2：联网 + 上报 + 补传 + 下行确认                                 */
/* ------------------------------------------------------------------ */
static void UploadTask(void *arg)
{
    (void)arg;
    LOGI("net", "task start, gateway=%s:%d", CFG_GATEWAY_IP, CFG_GATEWAY_PORT);

    while (Net_Init() != 0) {
        LOGW("net", "connect failed, retry in 5s");
        osDelay(5000);
    }
    if (Sntp_Sync() == 0) {
        char ts[32];
        Sntp_ToIso(ts, sizeof(ts));
        LOGI("sntp", "time synced: %s", ts);
    } else {
        LOGW("sntp", "sync failed, timestamps may be wrong");
    }

    hi_u32 ack_tick = now_ms();

    while (1) {
        char tbuf[24];
        char hbuf[24];
        float tv = 0.0f;
        float hv = 0.0f;
        int ok_th = 0;
        int pir = Drv_PirTriggered();

        Loc_LatestEnv(&tv, &hv, &ok_th);
        num_or_null(tbuf, sizeof(tbuf), ok_th, tv, 1);
        num_or_null(hbuf, sizeof(hbuf), ok_th, hv, 1);

        build_snapshot(g_json, sizeof(g_json), pir);

        /* 串口备用通道（网关可据此做 USB 桥接降级） */
        printf("%s%s\r\n", JSON_MARK, g_json);

        if (Net_PostJson("/api/v1/snapshot", g_json) == 0) {
            Net_FlushCache();
        } else {
            Net_PushCache(g_json);
        }

        LOGI("upload", "pir=%d temp=%s hum=%s door=%d level=%s",
             pir, tbuf, hbuf, g_door_open, Loc_LevelText());

        /* 下行确认：问一次“还有没有人没处理” */
        if ((now_ms() - ack_tick) >= CFG_ACK_POLL_MS) {
            ack_tick = now_ms();
            poll_ack();
        }

        osDelay(CFG_UPLOAD_MS);
    }
}

/* ------------------------------------------------------------------ */
void AppMain(void)
{
    Drv_Init();
    Env_Init();                 /* 没有模块也能跑，只是字段为 null */
    Biz_Init(now_ms());
    Loc_Init(now_ms());         /* 本地阈值判定：上电时刻作为时间基准 */

    LOGI("app", "===== ShadowLamp %s boot =====", CFG_FW_VERSION);
    LOGI("app", "device=%s gateway=%s:%d door_mode=%d",
         CFG_DEVICE_ID, CFG_GATEWAY_IP, CFG_GATEWAY_PORT, CFG_DOOR_MODE);
    if (CFG_DOOR_MODE == 2) {
        LOGW("app", "door is SIMULATED (long press 2s), frames marked source=test");
    }
    if (CFG_BUZZER_EN == 0) {
        LOGW("app", "no buzzer: local alarm is LED only");
    }

    osThreadAttr_t attr;
    memset(&attr, 0, sizeof(attr));

    attr.name = "SensorTask";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)SensorTask, NULL, &attr) == NULL) {
        LOGE("app", -1, "create SensorTask failed");
    }

    attr.name = "UploadTask";
    attr.stack_size = 8192;
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)UploadTask, NULL, &attr) == NULL) {
        LOGE("app", -2, "create UploadTask failed");
    }
}

SYS_RUN(AppMain);
