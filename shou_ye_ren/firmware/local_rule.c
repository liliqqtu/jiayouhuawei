/**
 * @file    local_rule.c
 * @brief   板端本地阈值判定实现（纯逻辑，可单独测试）
 *
 * 三档划分（与 gateway/rules.py 同一口径）：
 *   L1 提醒：偏热（≥28℃）、偏潮（≥70%）、偏干（<35%）、门窗久开（≥10 分钟）
 *   L2 告警：温度过高（≥35℃）、过低（<16℃）、湿度过高（≥80%）、过低（<25%）
 *   L3 紧急：绝对高温（≥55℃）或 180 秒内升温 ≥8℃（疑似火情）
 *
 * 防抖的做法（现场能不能看清告警，全靠这两个时间窗）：
 *   升级：新等级持续 CFG_ENV_CONFIRM_MS 才提交；L3 例外，立即提交（火情不能等）
 *   降级/解除：恢复条件持续 CFG_ENV_CLEAR_MS 才提交
 * 这样 36℃ 在阈值上下抖动时不会“响一下停一下”。
 *
 * 已知取舍：板端不判“无活动超时”和“按键求助”，那两条在 biz.c 里，
 * 因为它们依赖的是时间差和按键状态，与本模块的传感器阈值互不干扰。
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "local_rule.h"

/* 温升历史环形缓冲区：窗口 180 秒 / 采样 2 秒 = 90 条，多留 2 条防下标越界 */
#define RISE_N  ((int)(CFG_T_RISE_MS / CFG_ENV_SAMPLE_MS) + 2)

static float   s_r_temp[RISE_N];
static hi_u32  s_r_tick[RISE_N];
static int     s_r_idx = 0;
static int     s_r_cnt = 0;
static hi_u32  s_last_tick = 0;   /**< 最近一次成功采样时刻，作为温升窗口的“现在” */

/* 最近一次采样（ok=0 表示这次没读到，判定里不采用，上报时写 null） */
static int     s_env_ok = 0;
static float   s_temp = 0.0f;
static float   s_hum = 0.0f;

/* 门磁 */
static int     s_door_valid = 0;
static int     s_door_open = 0;
static hi_u32  s_door_since = 0;

/* 候选（新鲜算出来的）与已提交（正在生效的） */
static LocLevel s_cand_level = LOC_NONE;
static LocKind  s_cand_kind = LOC_KIND_NONE;
static LocLevel s_level = LOC_NONE;
static LocKind  s_kind = LOC_KIND_NONE;

/* 待提交项的时间窗记录 */
static LocLevel s_pend_level = LOC_NONE;
static LocKind  s_pend_kind = LOC_KIND_NONE;
static hi_u32   s_pend_tick = 0;

static hi_u32   s_change_seq = 0;
static float    s_commit_rise = 0.0f;  /* 提交“短时升温”那一刻的升幅，供消息使用 */
static char     s_text[112];

/* ------------------------------------------------------------------ */
/* 小工具                                                              */
/* ------------------------------------------------------------------ */
/** 固定一位小数的格式化：不依赖 printf 的 %f（Hi3861 的 printf 对浮点支持不稳） */
static void fmt1(char *out, int size, float v)
{
    int neg = (v < 0.0f);
    float a = neg ? -v : v;
    int all = (int)(a * 10.0f + 0.5f);
    snprintf(out, size, "%s%d.%d", neg ? "-" : "", all / 10, all % 10);
}

void Loc_Init(hi_u32 now)
{
    int i;
    for (i = 0; i < RISE_N; i++) {
        s_r_temp[i] = 0.0f;
        s_r_tick[i] = 0;
    }
    s_r_idx = 0;
    s_r_cnt = 0;

    s_env_ok = 0;
    s_temp = 0.0f;
    s_hum = 0.0f;

    s_door_valid = 0;
    s_door_open = 0;
    s_door_since = now;

    s_cand_level = LOC_NONE;
    s_cand_kind = LOC_KIND_NONE;
    s_level = LOC_NONE;
    s_kind = LOC_KIND_NONE;

    s_pend_level = LOC_NONE;
    s_pend_kind = LOC_KIND_NONE;
    s_pend_tick = now;

    s_change_seq = 0;
    printf("[rule] local thresholds: t_warn=28 t_high=35 t_fire=55 rise=8/180s\r\n");
}

void Loc_SampleEnv(hi_u32 now, int ok, float temp, float hum)
{
    if (!ok) {
        /* 读不到就不参与判定；不拿上一次的值冒充新值 */
        s_env_ok = 0;
        return;
    }

    s_env_ok = 1;
    s_temp = temp;
    s_hum = hum;
    s_last_tick = now;

    s_r_temp[s_r_idx] = temp;
    s_r_tick[s_r_idx] = now;
    s_r_idx = (s_r_idx + 1) % RISE_N;
    if (s_r_cnt < RISE_N) {
        s_r_cnt++;
    }
}

void Loc_SampleDoor(hi_u32 now, int has_door, int door_open)
{
    if (!has_door) {
        s_door_valid = 0;
        s_door_open = 0;
        return;
    }

    if (door_open && !s_door_open) {
        s_door_since = now;     /* 刚打开：重新计时 */
    }
    s_door_valid = 1;
    s_door_open = door_open ? 1 : 0;
}

/** 窗口内相对最低温的升幅；历史不足 2 个点时返回 0 */
float Loc_RiseC(void)
{
    int i;
    int found = 0;
    int span = 0;
    float min_t = 0.0f;

    if (s_r_cnt < 2 || s_last_tick == 0) {
        return 0.0f;        /* 历史太短，宁可判 0 也不虚报“升温” */
    }

    for (i = 0; i < s_r_cnt; i++) {
        if (s_r_tick[i] == 0) {
            continue;
        }
        /* 只统计最近 CFG_T_RISE_MS 内的采样；环形区翻转后老数据会被时间窗挡掉 */
        if ((s_last_tick - s_r_tick[i]) > CFG_T_RISE_MS) {
            continue;
        }
        if (!found || s_r_temp[i] < min_t) {
            min_t = s_r_temp[i];
            found = 1;
            span = (int)(s_last_tick - s_r_tick[i]);
        }
    }
    if (!found || span < CFG_ENV_SAMPLE_MS * 2) {
        return 0.0f;        /* 时间跨度不足两个采样周期，不算升温 */
    }
    return s_temp - min_t;
}

/* ------------------------------------------------------------------ */
/* 候选等级计算                                                        */
/* ------------------------------------------------------------------ */
/** 同级时先出现的优先：调用顺序即优先级（火情 > 温度 > 湿度 > 门磁 > 提醒） */
static void consider(LocKind kind, LocLevel level)
{
    if (level > s_cand_level) {
        s_cand_level = level;
        s_cand_kind = kind;
    }
}

static void compute_cand(hi_u32 now)
{
    s_cand_level = LOC_NONE;
    s_cand_kind = LOC_KIND_NONE;

    if (s_env_ok) {
        float rise = Loc_RiseC();
        if (s_temp >= CFG_T_FIRE) {
            consider(LOC_KIND_TEMP_FIRE, LOC_L3);
        }
        if (rise >= CFG_T_RISE) {
            consider(LOC_KIND_TEMP_RISE, LOC_L3);
        }
        if (s_temp >= CFG_T_HIGH) {
            consider(LOC_KIND_TEMP_HIGH, LOC_L2);
        }
        if (s_hum >= CFG_H_HIGH) {
            consider(LOC_KIND_HUM_HIGH, LOC_L2);
        }
        if (s_temp < CFG_T_LOW) {
            consider(LOC_KIND_TEMP_LOW, LOC_L2);
        }
        if (s_hum < CFG_H_LOW) {
            consider(LOC_KIND_HUM_LOW, LOC_L2);
        }
        if (s_temp >= CFG_T_WARN) {
            consider(LOC_KIND_TEMP_WARN, LOC_L1);
        }
        if (s_hum >= CFG_H_WARN_HIGH) {
            consider(LOC_KIND_HUM_WARN_HIGH, LOC_L1);
        }
        if (s_hum < CFG_H_WARN_LOW) {
            consider(LOC_KIND_HUM_WARN_LOW, LOC_L1);
        }
    }

    if (s_door_valid && s_door_open && (now - s_door_since) >= CFG_DOOR_OPEN_MS) {
        consider(LOC_KIND_DOOR_OPEN, LOC_L1);
    }
}

/* ------------------------------------------------------------------ */
/* 状态机                                                              */
/* ------------------------------------------------------------------ */
LocLevel Loc_Eval(hi_u32 now)
{
    hi_u32 hold = 0;

    compute_cand(now);

    /* 与已提交项一致：没有变化，清掉待提交记录 */
    if (s_cand_level == s_level && s_cand_kind == s_kind) {
        s_pend_level = LOC_NONE;
        s_pend_kind = LOC_KIND_NONE;
        return s_level;
    }

    /* 待提交项变了 -> 重新计时（防止在两个等级之间来回跳） */
    if (s_cand_level != s_pend_level || s_cand_kind != s_pend_kind) {
        s_pend_level = s_cand_level;
        s_pend_kind = s_cand_kind;
        s_pend_tick = now;
    }

    if (s_cand_level > s_level) {
        /* 升级：火情立即报，其余要稳定一段时间 */
        hold = (s_cand_level == LOC_L3) ? 0 : CFG_ENV_CONFIRM_MS;
    } else if (s_cand_level < s_level) {
        /* 降级/解除：确认恢复了才撤，避免阈值边缘反复 */
        hold = CFG_ENV_CLEAR_MS;
    } else {
        /* 同等级换原因（比如 温度过高 -> 湿度过高）：也要稳定一下再改说法 */
        hold = CFG_ENV_CONFIRM_MS;
    }

    if ((now - s_pend_tick) >= hold) {
        s_commit_rise = Loc_RiseC();
        s_level = s_cand_level;
        s_kind = s_cand_kind;
        s_change_seq++;
        s_pend_level = LOC_NONE;
        s_pend_kind = LOC_KIND_NONE;
        printf("[rule] level -> %s (%s)\r\n", Loc_LevelText(), Loc_KindAlarmType());
    }
    return s_level;
}

LocLevel Loc_Level(void)
{
    return s_level;
}

LocKind Loc_Kind(void)
{
    return s_kind;
}

hi_u32 Loc_ChangeSeq(void)
{
    return s_change_seq;
}

void Loc_LatestEnv(float *temp, float *hum, int *ok)
{
    if (temp) {
        *temp = s_temp;
    }
    if (hum) {
        *hum = s_hum;
    }
    if (ok) {
        *ok = s_env_ok;
    }
}

const char *Loc_LevelText(void)
{
    switch (s_level) {
        case LOC_L1: return "L1";
        case LOC_L2: return "L2";
        case LOC_L3: return "L3";
        default:     return "none";
    }
}

const char *Loc_KindAlarmType(void)
{
    switch (s_kind) {
        case LOC_KIND_TEMP_FIRE:     return "TEMP_FIRE";
        case LOC_KIND_TEMP_RISE:     return "TEMP_RISE";
        case LOC_KIND_TEMP_HIGH:     return "TEMP_HIGH";
        case LOC_KIND_HUM_HIGH:      return "HUM_HIGH";
        case LOC_KIND_TEMP_LOW:      return "TEMP_LOW";
        case LOC_KIND_HUM_LOW:       return "HUM_LOW";
        case LOC_KIND_DOOR_OPEN:     return "DOOR_OPEN";
        case LOC_KIND_TEMP_WARN:     return "TEMP_WARM";
        case LOC_KIND_HUM_WARN_HIGH: return "HUM_WARN_HIGH";
        case LOC_KIND_HUM_WARN_LOW:  return "HUM_WARN_LOW";
        default:                     return "NONE";
    }
}

const char *Loc_KindText(void)
{
    char t[16];
    char h[16];
    char r[16];

    fmt1(t, sizeof(t), s_temp);
    fmt1(h, sizeof(h), s_hum);
    fmt1(r, sizeof(r), s_commit_rise);

    switch (s_kind) {
        case LOC_KIND_TEMP_FIRE:
            snprintf(s_text, sizeof(s_text),
                     "疑似火情：室内温度 %s℃，已超过火情阈值 55.0℃", t);
            break;
        case LOC_KIND_TEMP_RISE:
            snprintf(s_text, sizeof(s_text),
                     "疑似火情：180 秒内升温 %s℃，当前 %s℃", r, t);
            break;
        case LOC_KIND_TEMP_HIGH:
            snprintf(s_text, sizeof(s_text),
                     "室内温度 %s℃，高于安全阈值 35.0℃，请检查取暖设备", t);
            break;
        case LOC_KIND_HUM_HIGH:
            snprintf(s_text, sizeof(s_text), "室内湿度 %s%%，高于 80%%，注意防潮", h);
            break;
        case LOC_KIND_TEMP_LOW:
            snprintf(s_text, sizeof(s_text), "室内温度 %s℃，低于 16.0℃，注意保暖", t);
            break;
        case LOC_KIND_HUM_LOW:
            snprintf(s_text, sizeof(s_text), "室内湿度 %s%%，低于 25%%，注意补水", h);
            break;
        case LOC_KIND_DOOR_OPEN:
            snprintf(s_text, sizeof(s_text), "门窗已持续开启超过 10 分钟");
            break;
        case LOC_KIND_TEMP_WARN:
            snprintf(s_text, sizeof(s_text), "室内温度 %s℃，偏热，建议通风", t);
            break;
        case LOC_KIND_HUM_WARN_HIGH:
            snprintf(s_text, sizeof(s_text), "室内湿度 %s%%，偏潮", h);
            break;
        case LOC_KIND_HUM_WARN_LOW:
            snprintf(s_text, sizeof(s_text), "室内湿度 %s%%，偏干", h);
            break;
        default:
            snprintf(s_text, sizeof(s_text), "本地判定：状态已恢复");
            break;
    }
    return s_text;
}
