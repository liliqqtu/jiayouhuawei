#ifndef EMERGENCY_SOS_H
#define EMERGENCY_SOS_H

#include <stdint.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 求助触发源
typedef enum {
    SOS_SRC_NONE = 0,
    SOS_SRC_KEY_F1,          // 主控板载 F1 按键
    SOS_SRC_WIRELESS_REMOTE, // 433MHz 随身无线求助按键
    SOS_SRC_REMOTE_CMD       // 局域网/北向远程触发
} sos_source_t;

// 初始化紧急求助按键与 433MHz 无线遥控接收接口
// F1 (GPIO_11): 主机紧急求助 SOS 键
// F2 (GPIO_12): 现场消警复位按键
// WIRELESS (GPIO_14): 433MHz 随身无线遥控开关常开触点接入端
void EmergencySosInit(void);

// 查询当前是否处于 SOS 告警激活状态
uint8_t EmergencySosIsActive(void);

// 获取本次求助的具体触发来源 (0:无, 1:板载按键, 2:无线遥控)
sos_source_t EmergencySosGetSource(void);

// 软件触发或清除 SOS
void EmergencySosTriggerWithSource(sos_source_t source);
void EmergencySosTrigger(void);
void EmergencySosClear(void);

// 轮询去抖驱动 (每 50ms 周期调用)
void EmergencySosPoll(void);

#ifdef __cplusplus
}
#endif

#endif // EMERGENCY_SOS_H
