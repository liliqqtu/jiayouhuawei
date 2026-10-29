#ifndef SENSOR_DOOR_H
#define SENSOR_DOOR_H

#include <stdint.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化干簧管门窗磁传感器 (GPIO_13)
void SensorDoorInit(void);

// 读取门窗状态 (0: 门窗闭合, 1: 门窗打开)
uint8_t SensorDoorIsOpen(void);

// 获取门窗当前已打开的持续时长 (秒)
uint32_t SensorDoorGetOpenDuration(void);

// 门窗状态更新时间驱动 (每秒调用)
void SensorDoorTick1s(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_DOOR_H
