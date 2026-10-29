#ifndef EMERGENCY_SOS_H
#define EMERGENCY_SOS_H

#include <stdint.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化紧急求助按键 (F1 = SOS 报警, F2 = 消警复位)
void EmergencySosInit(void);

// 查询当前是否处于 SOS 告警激活状态
uint8_t EmergencySosIsActive(void);

// 软件触发或清除 SOS
void EmergencySosTrigger(void);
void EmergencySosClear(void);

// 轮询去抖驱动 (每 50ms~100ms 调用)
void EmergencySosPoll(void);

#ifdef __cplusplus
}
#endif

#endif // EMERGENCY_SOS_H
