/**
 * @file    biz.h
 * @brief   业务状态层：告警判定的唯一权威。
 *
 * 这一层**不碰硬件、不碰网络**，只接收“现在几点、有没有人动、按键是否按下”，
 * 输出“该不该报警”。好处是：可以在 PC 上用 gcc 单独跑测试，不用烧板子。
 */

#ifndef __BIZ_H__
#define __BIZ_H__

#include "hi_types_base.h"

typedef enum {
    EVT_NONE = 0,
    EVT_NO_MOTION,   /**< 疑似长时间无活动（仅描述信号状态） */
    EVT_SOS          /**< 紧急求助（最高优先级） */
} EvtType;

/**
 * 板端告警状态机：active -> acknowledged -> closed
 *   active       刚触发，本地声光已启动
 *   acknowledged 已成功上报到网关（对应 HTTP 200）
 *   closed       条件恢复（有人活动 / 求助超时）后归档
 * 注意：APP 侧的“确认/处置”是另一条线，由网关台账负责，两者不要混。
 */
typedef enum {
    ALM_NONE = 0,
    ALM_ACTIVE,
    ALM_ACKNOWLEDGED,
    ALM_CLOSED
} AlmStatus;

typedef struct {
    hi_u32   last_motion_tick;   /**< 最后一次检测到活动的时刻（ms） */
    hi_u8    alarm_active;       /**< 当前是否有未恢复的告警 */
    EvtType  alarm_type;
    hi_u32   alarm_tick;
    hi_u32   seq;                /**< 上报序号，上电从 1 开始 */
    hi_u8    motion_flag;        /**< 本轮轮询是否检测到新活动 */
} BizState;

void        Biz_Init(hi_u32 now);

/** 检测到人体活动：刷新最后活动时间，并自动解除“无活动”告警 */
void        Biz_OnMotion(hi_u32 now);

/** 按下求助按钮：立即产生 SOS（不被其它告警覆盖） */
EvtType     Biz_OnSos(hi_u32 now);

/** 周期调用：判断是否超时，返回本轮需要新建的告警事件 */
EvtType     Biz_Poll(hi_u32 now);

/** 取下一个上报序号（调用即自增） */
hi_u32      Biz_NextSeq(void);

hi_u8       Biz_AlarmActive(void);
EvtType     Biz_AlarmType(void);
hi_u32      Biz_LastMotionAgoSec(hi_u32 now);
void        Biz_ClearAlarm(void);

AlmStatus   Biz_AlarmStatus(void);
const char *Biz_StatusText(void);
/** 上报成功后调用：active -> acknowledged */
void        Biz_MarkReported(void);

#endif /* __BIZ_H__ */
