/**
 * @file    env.h
 * @brief   环境传感器：SHT30（温湿度）、BH1750（光照），走 I2C0。
 *
 * 关键约定：读不到就返回 -1，由调用方把字段填 null。
 * **绝不允许**在读取失败时填 0 或默认值 —— 那会把"没有数据"伪装成"25℃、50%"。
 */

#ifndef __ENV_H__
#define __ENV_H__

#include "hi_types_base.h"

#define ENV_SHT30_ADDR   0x44
#define ENV_BH1750_ADDR  0x23

/** 初始化 I2C0（E53 扩展座的 SDA/SCL） */
int Env_Init(void);

/** 读温湿度；返回 0 成功，-1 失败（*temp / *hum 不变） */
int Env_ReadTempHum(float *temp, float *hum);

/** 读光照；返回 0 成功，-1 失败 */
int Env_ReadLux(float *lux);

/** 该模块是否存在（Init 时探测，不存在则不再重试，省 CPU） */
int Env_HasTempHum(void);
int Env_HasLux(void);

#endif /* __ENV_H__ */
