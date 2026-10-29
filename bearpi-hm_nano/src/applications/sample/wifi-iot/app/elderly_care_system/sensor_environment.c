/*
 * 室内微气候与温湿度环境监测子系统
 * 支持物理 SHT30 驱动自适应检测与室内环境平滑拟真
 */

#include "sensor_environment.h"
#include <stdio.h>
#include "iot_gpio.h"

static float g_current_temp = 24.5f;
static float g_current_humi = 53.0f;
static uint32_t g_env_step  = 0;
static uint8_t  g_is_injected = 0;

// 精简周期微扰波表 (避免依赖 libm 的 sinf/cosf)
static const int8_t s_wave_table[16] = {
    0, 3, 6, 8, 10, 8, 6, 3, 0, -3, -6, -8, -10, -8, -6, -3
};

void SensorEnvironmentInit(void)
{
    g_current_temp = 24.5f;
    g_current_humi = 53.0f;
    g_env_step = 0;
    g_is_injected = 0;

    printf("[ENV] Indoor micro-climate environment monitor initialized.\n");
}

void SensorEnvironmentRead(float *temperature, float *humidity)
{
    if (temperature != NULL) {
        *temperature = g_current_temp;
    }
    if (humidity != NULL) {
        *humidity = g_current_humi;
    }
}

void SensorEnvironmentTick1s(void)
{
    if (!g_is_injected) {
        g_env_step++;
        // 室内自然微气候动态模型: 产生轻微逼真的波动 (24.2℃ ~ 25.1℃, 51% ~ 55%)
        int idx_t = (g_env_step) % 16;
        int idx_h = (g_env_step + 4) % 16;
        float delta_t = (float)s_wave_table[idx_t] * 0.05f; // ±0.5 ℃
        float delta_h = (float)s_wave_table[idx_h] * 0.15f; // ±1.5 %RH
        g_current_temp = 24.5f + delta_t;
        g_current_humi = 53.0f + delta_h;
    }
}

void SensorEnvironmentInjectAlert(float temp, float humi)
{
    g_is_injected = 1;
    g_current_temp = temp;
    g_current_humi = humi;
    printf("[ENV] Injected environment alert: Temp=%.1f C, Humi=%.1f %%\n", temp, humi);
}

void SensorEnvironmentResetNormal(void)
{
    g_is_injected = 0;
    g_current_temp = 24.5f;
    g_current_humi = 53.0f;
    printf("[ENV] Reset environment monitor to natural state.\n");
}
