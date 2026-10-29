/*
 * E53_IS1 人体红外热释电传感器驱动 (GPIO_7)
 */

#include "sensor_infrared.h"
#include <stdio.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "cmsis_os2.h"

static volatile uint32_t g_seconds_since_motion = 0;
static volatile uint32_t g_motion_count_today = 0;
static volatile uint8_t  g_motion_active_cached = 0;

// 中断回调函数：人走过时硬件输出上升沿
static void OnInfraredIsr(char *arg)
{
    (void)arg;
    g_seconds_since_motion = 0;
    g_motion_active_cached = 1;
    g_motion_count_today++;
}

void SensorInfraredInit(void)
{
    // GPIO_7 初始化为普通 GPIO 输入模式
    IoTGpioInit(PIN_INFRARED);
    IoTGpioSetFunc(PIN_INFRARED, IOT_GPIO_FUNC_GPIO_7_GPIO);
    IoTGpioSetDir(PIN_INFRARED, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_INFRARED, IOT_GPIO_PULL_UP);

    // 注册上升沿中断 (检测到人体拉高)
    IoTGpioRegisterIsrFunc(PIN_INFRARED, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_RISE_LEVEL_HIGH, OnInfraredIsr, NULL);

    g_seconds_since_motion = 0;
    g_motion_count_today = 0;
    g_motion_active_cached = 0;

    printf("[INFRARED] E53_IS1 PIR sensor initialized on GPIO_7.\n");
}

uint8_t SensorInfraredGetRealtime(void)
{
    IotGpioValue val = IOT_GPIO_VALUE0;
    IoTGpioGetInputVal(PIN_INFRARED, &val);
    if (val == IOT_GPIO_VALUE1) {
        g_motion_active_cached = 1;
        g_seconds_since_motion = 0;
        return 1;
    }
    return g_motion_active_cached;
}

uint32_t SensorInfraredGetSecondsSinceLastMotion(void)
{
    return g_seconds_since_motion;
}

uint32_t SensorInfraredGetTodayCount(void)
{
    return g_motion_count_today;
}

void SensorInfraredTriggerMotion(void)
{
    g_seconds_since_motion = 0;
    g_motion_active_cached = 1;
    g_motion_count_today++;
    printf("[INFRARED] Manual motion triggered!\n");
}

void SensorInfraredTick1s(void)
{
    g_seconds_since_motion++;

    // 如果传感器电平已恢复为 0，则清除瞬时活跃标记
    IotGpioValue val = IOT_GPIO_VALUE0;
    IoTGpioGetInputVal(PIN_INFRARED, &val);
    if (val == IOT_GPIO_VALUE0) {
        g_motion_active_cached = 0;
    }
}
