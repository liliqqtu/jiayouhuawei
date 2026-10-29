/**
 * @file    biz.c
 * @brief   告警判定实现（纯逻辑）
 *
 * 与《软件开发实施文档》3.3 的规则保持一致，但板端只做两档：
 *   L2 疑似长时间无活动 / L3 紧急求助
 * 更细的“夜间 + 卫生间加权”放在网关做，板端保持简单 —— 这是刻意的取舍：
 * 板子越简单越不容易死机，演示时本地声光才靠得住。
 */

#include <stdio.h>

#include "config.h"
#include "biz.h"

/* SOS 持续时间：板端无法得知 APP 是否已处置，亮灯 5 分钟后自动解除，
 * 避免“灯一直亮着”影响现场演示。真实产品应由下行指令控制。 */
#define SOS_AUTO_CLEAR_MS   (5 * 60 * 1000)

static BizState g_biz;
static AlmStatus g_status = ALM_NONE;

void Biz_Init(hi_u32 now)
{
    g_biz.last_motion_tick = now;   /* 上电视为“刚有活动”，避免开机误报 */
    g_biz.alarm_active = 0;
    g_biz.alarm_type = EVT_NONE;
    g_biz.alarm_tick = now;
    g_biz.seq = 0;
    g_biz.motion_flag = 0;
    printf("[biz] init, no_motion_threshold=%d ms\r\n", CFG_NO_MOTION_MS);
}

void Biz_OnMotion(hi_u32 now)
{
    g_biz.last_motion_tick = now;
    g_biz.motion_flag = 1;

    /* 人动了，说明“无活动”判断不成立 -> 立即解除该告警 */
    if (g_biz.alarm_active && g_biz.alarm_type == EVT_NO_MOTION) {
        g_biz.alarm_active = 0;
        g_biz.alarm_type = EVT_NONE;
        g_status = ALM_CLOSED;
        printf("[biz] motion detected, NO_MOTION cleared\r\n");
    }
}

EvtType Biz_OnSos(hi_u32 now)
{
    /* 求助优先级最高：即使已有其它告警，也要升级为 SOS */
    if (g_biz.alarm_active && g_biz.alarm_type == EVT_SOS) {
        return EVT_NONE;    /* 已经在求助状态，不重复触发 */
    }
    g_biz.alarm_active = 1;
    g_biz.alarm_type = EVT_SOS;
    g_biz.alarm_tick = now;
    g_status = ALM_ACTIVE;
    printf("[biz] SOS triggered\r\n");
    return EVT_SOS;
}

EvtType Biz_Poll(hi_u32 now)
{
    if (g_biz.alarm_active && g_biz.alarm_type == EVT_SOS) {
        /* 求助超时自动解除（仅板端状态，APP 台账不受影响） */
        if ((now - g_biz.alarm_tick) > SOS_AUTO_CLEAR_MS) {
            g_biz.alarm_active = 0;
            g_biz.alarm_type = EVT_NONE;
            g_status = ALM_CLOSED;
            printf("[biz] SOS auto cleared\r\n");
        }
        return EVT_NONE;
    }

    if (g_biz.alarm_active) {
        return EVT_NONE;    /* 已有告警未恢复，不重复建单（去重） */
    }

    if ((now - g_biz.last_motion_tick) >= CFG_NO_MOTION_MS) {
        g_biz.alarm_active = 1;
        g_biz.alarm_type = EVT_NO_MOTION;
        g_biz.alarm_tick = now;
        g_status = ALM_ACTIVE;
        printf("[biz] NO_MOTION after %u s\r\n",
               (unsigned int)((now - g_biz.last_motion_tick) / 1000));
        return EVT_NO_MOTION;
    }
    return EVT_NONE;
}

hi_u32 Biz_NextSeq(void)
{
    g_biz.seq++;
    if (g_biz.seq == 0) {
        g_biz.seq = 1;      /* 回绕保护 */
    }
    return g_biz.seq;
}

hi_u8 Biz_AlarmActive(void)
{
    return g_biz.alarm_active;
}

EvtType Biz_AlarmType(void)
{
    return g_biz.alarm_type;
}

hi_u32 Biz_LastMotionAgoSec(hi_u32 now)
{
    return (now - g_biz.last_motion_tick) / 1000;
}

void Biz_ClearAlarm(void)
{
    g_biz.alarm_active = 0;
    g_biz.alarm_type = EVT_NONE;
}

AlmStatus Biz_AlarmStatus(void)
{
    return g_status;
}

const char *Biz_StatusText(void)
{
    switch (g_status) {
        case ALM_ACTIVE:       return "active";
        case ALM_ACKNOWLEDGED: return "acknowledged";
        case ALM_CLOSED:       return "closed";
        default:               return "none";
    }
}

void Biz_MarkReported(void)
{
    if (g_status == ALM_ACTIVE) {
        g_status = ALM_ACKNOWLEDGED;
    }
}
