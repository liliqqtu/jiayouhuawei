#ifndef ALARM_BEEPER_H
#define ALARM_BEEPER_H

#include <stdint.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化声光外设 (E53_IS1 蜂鸣器 + 板载 LED)
void AlarmBeeperInit(void);

// 设置声光报警模式
void AlarmBeeperSetMode(alert_level_t level, alert_type_t type);

// 声光状态机心跳驱动 (在主循环中以 100ms~500ms 周期调用)
void AlarmBeeperTick(uint32_t tick_step_ms);

// 本地静音/消警
void AlarmBeeperMute(void);

// 短促蜂鸣一声 (用于按键反馈或自检通过)
void AlarmBeeperBeepShort(void);

#ifdef __cplusplus
}
#endif

#endif // ALARM_BEEPER_H
