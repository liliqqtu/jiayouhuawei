/*
 * 老人紧急求助 SOS 与现场消警按键驱动
 * F1 (GPIO_11): 紧急求助 SOS 触发
 * F2 (GPIO_12): 现场消警复位按键
 */

#include "emergency_sos.h"
#include <stdio.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "alarm_beeper.h"

static volatile uint8_t g_sos_state = 0;
static uint8_t g_f1_history = 0xFF;
static uint8_t g_f2_history = 0xFF;

void EmergencySosInit(void)
{
    // 初始化 F1 按键 (GPIO_11) 为数字输入，开内部上拉
    IoTGpioInit(PIN_KEY_SOS);
    IoTGpioSetFunc(PIN_KEY_SOS, IOT_GPIO_FUNC_GPIO_11_GPIO);
    IoTGpioSetDir(PIN_KEY_SOS, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_KEY_SOS, IOT_GPIO_PULL_UP);

    // 初始化 F2 按键 (GPIO_12) 为数字输入，开内部上拉
    IoTGpioInit(PIN_KEY_RESET);
    IoTGpioSetFunc(PIN_KEY_RESET, IOT_GPIO_FUNC_GPIO_12_GPIO);
    IoTGpioSetDir(PIN_KEY_RESET, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_KEY_RESET, IOT_GPIO_PULL_UP);

    g_sos_state = 0;
    g_f1_history = 0xFF;
    g_f2_history = 0xFF;

    printf("[SOS] Emergency SOS (F1/GPIO_11) and Reset (F2/GPIO_12) initialized.\n");
}

uint8_t EmergencySosIsActive(void)
{
    return g_sos_state;
}

void EmergencySosTrigger(void)
{
    g_sos_state = 1;
    printf("!!! [SOS ALARM TRIGGERED] Elder pressed SOS emergency button! !!!\n");
}

void EmergencySosClear(void)
{
    if (g_sos_state) {
        g_sos_state = 0;
        AlarmBeeperBeepShort();
        printf("[SOS] Alarm cleared and reset locally by caregiver/elder.\n");
    }
}

void EmergencySosPoll(void)
{
    IotGpioValue val_f1 = IOT_GPIO_VALUE1;
    IotGpioValue val_f2 = IOT_GPIO_VALUE1;

    IoTGpioGetInputVal(PIN_KEY_SOS, &val_f1);
    IoTGpioGetInputVal(PIN_KEY_RESET, &val_f2);

    // F1 去抖采样 (按下接地 = 0)
    g_f1_history = ((g_f1_history << 1) | (val_f1 == IOT_GPIO_VALUE0 ? 0 : 1));
    if (g_f1_history == 0x00 && !g_sos_state) {
        EmergencySosTrigger();
    }

    // F2 去抖采样 (按下接地 = 0)
    g_f2_history = ((g_f2_history << 1) | (val_f2 == IOT_GPIO_VALUE0 ? 0 : 1));
    if (g_f2_history == 0x00) {
        EmergencySosClear();
        AlarmBeeperMute();
    }
}
