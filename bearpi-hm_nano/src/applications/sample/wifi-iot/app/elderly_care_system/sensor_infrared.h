#ifndef SENSOR_INFRARED_H
#define SENSOR_INFRARED_H

#include <stdint.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 E53_IS1 人体红外感应模块
void SensorInfraredInit(void);

// 查询当前瞬时人体移动状态 (0: 无, 1: 有人移动)
uint8_t SensorInfraredGetRealtime(void);

// 获取距上一次检测到人体移动过去的时间 (秒)
uint32_t SensorInfraredGetSecondsSinceLastMotion(void);

// 获取今日累计检测到的活动次数
uint32_t SensorInfraredGetTodayCount(void);

// 软件模拟/注入活动信号 (用于自动化测试与调试)
void SensorInfraredTriggerMotion(void);

// 主循环更新时间驱动 (每秒调用)
void SensorInfraredTick1s(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_INFRARED_H
