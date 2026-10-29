#ifndef SENSOR_ENVIRONMENT_H
#define SENSOR_ENVIRONMENT_H

#include <stdint.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化环境传感器子系统 (支持物理 I2C SHT30 与微气候拟真双模)
void SensorEnvironmentInit(void);

// 读取当前室内温湿度
void SensorEnvironmentRead(float *temperature, float *humidity);

// 环境子系统时间驱动 (每秒调用，产生真实微气候波动)
void SensorEnvironmentTick1s(void);

// 异常环境测试注入接口 (用于模拟高温火险/严寒)
void SensorEnvironmentInjectAlert(float temp, float humi);

// 恢复正常环境模拟
void SensorEnvironmentResetNormal(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_ENVIRONMENT_H
