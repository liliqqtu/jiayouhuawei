/**
 * @file    local_rule.h
 * @brief   板端本地阈值判定（赛题四·功能1：开发板通过基础阈值算法识别异常状态）。
 *
 * 为什么板端还要判一遍？
 *   1) 赛题原文要求“开发板…识别异常状态”，只靠网关判等于把这条功能搬到电脑上了；
 *   2) 断网（Wi-Fi 掉了 / 路由器重启）时，板子必须还能自己响、自己亮，
 *      不能因为网关不在就变成一块哑巴板子。
 *
 * 分工（与设计文档 3.3 一致）：
 *   板端：只做“单指标 + 时间窗”的基础阈值判定，结果是 L1/L2/L3 三档，
 *         驱动本地声光；能上网时把判定结果作为事件补报给网关。
 *   网关：做融合规则（外出抑制、夜间加权、冷却去重），是告警台账的权威。
 *
 * 这一层是纯逻辑：不碰 GPIO、不碰网络、不读时间（now 由调用方传入），
 * 因此可以单独单元测试，烧板子之前就能把状态机跑通。
 */

#ifndef __LOCAL_RULE_H__
#define __LOCAL_RULE_H__

#include "hi_types_base.h"

/** 本地告警等级：数值大小即严重程度，向上比较即可 */
typedef enum {
    LOC_NONE = 0,
    LOC_L1   = 1,   /**< 提醒级：偏热、偏潮、偏干 */
    LOC_L2   = 2,   /**< 告警级：温度过高/过低、湿度过高/过低 */
    LOC_L3   = 3    /**< 紧急级：疑似火情（绝对高温或短时剧烈升温） */
} LocLevel;

/** 本地触发原因：一条告警同一时刻只有一个主因，便于界面与日志解释 */
typedef enum {
    LOC_KIND_NONE = 0,
    LOC_KIND_TEMP_FIRE,       /**< 绝对高温（疑似火情） */
    LOC_KIND_TEMP_RISE,       /**< 短时升温过快（疑似火情） */
    LOC_KIND_TEMP_HIGH,       /**< 温度过高 */
    LOC_KIND_HUM_HIGH,        /**< 湿度过高 */
    LOC_KIND_TEMP_LOW,        /**< 温度过低 */
    LOC_KIND_HUM_LOW,         /**< 湿度过低 */
    LOC_KIND_DOOR_OPEN,       /**< 门窗长时间未关 */
    LOC_KIND_TEMP_WARN,       /**< 偏热 */
    LOC_KIND_HUM_WARN_HIGH,   /**< 偏潮 */
    LOC_KIND_HUM_WARN_LOW     /**< 偏干 */
} LocKind;

/** 上电初始化（now 传入开机时刻，避免用 0 做时间基准） */
void     Loc_Init(hi_u32 now);

/**
 * 记录一次环境读数。
 * @param ok   1=读取成功；0=传感器没插/读失败，此时该轮不参与判定（绝不填默认值）
 */
void     Loc_SampleEnv(hi_u32 now, int ok, float temp, float hum);

/** 记录门磁状态：has_door=0 表示未接（不参与判定） */
void     Loc_SampleDoor(hi_u32 now, int has_door, int door_open);

/**
 * 推进状态机并返回“当前生效”的等级（带确认/解除时间窗，防止抖动）。
 * 每个采样周期调用一次即可。
 */
LocLevel Loc_Eval(hi_u32 now);

LocLevel    Loc_Level(void);
LocKind     Loc_Kind(void);

/** 状态提交次数：每次等级/原因真正变化时 +1，上层据此判断“要不要上报事件” */
hi_u32      Loc_ChangeSeq(void);

/** 映射到网关的告警类型名（与 gateway/rules.py 的 alarm type 同名） */
const char *Loc_KindAlarmType(void);

/** 中文描述，用于事件消息，例如“室内温度 36.2℃，高于安全阈值 35.0℃” */
const char *Loc_KindText(void);

/** 等级转字符串：L1/L2/L3 */
const char *Loc_LevelText(void);

/** 最近一次采样值；*ok=1 表示最近一次读取成功，字段才可以上报 */
void     Loc_LatestEnv(float *temp, float *hum, int *ok);

/** 当前窗口内相对最低温的升幅（℃，用于日志与事件消息；无足够历史时返回 0） */
float    Loc_RiseC(void);

#endif /* __LOCAL_RULE_H__ */
