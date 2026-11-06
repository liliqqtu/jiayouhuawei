/*
 * 老人紧急求助 SOS 与现场消警按键驱动
 * 支持双轨触发：
 * 1. F1 (GPIO_11): 主板物理微动按键
 * 2. 433MHz 无线遥控接收模块 (GPIO_14): 随身老人防跌倒无线 SOS 按键
 * 3. F2 (GPIO_12): 现场消警复位按键
 */

#include "emergency_sos.h"
#include <stdio.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "alarm_beeper.h"

static volatile uint8_t      g_sos_state = 0;
static volatile sos_source_t g_sos_source = SOS_SRC_NONE;

static uint8_t g_f1_history = 0xFF;
static uint8_t g_wireless_history = 0xFF;
static uint8_t g_f2_history = 0xFF;

void EmergencySosInit(void)
{
    // 1. 初始化 F1 按键 (GPIO_11) 为数字输入，开内部弱上拉
    IoTGpioInit(PIN_KEY_SOS);
    IoTGpioSetFunc(PIN_KEY_SOS, IOT_GPIO_FUNC_GPIO_11_GPIO);
    IoTGpioSetDir(PIN_KEY_SOS, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_KEY_SOS, IOT_GPIO_PULL_UP);

    // 2. 初始化 433MHz 随身无线求助模块引脚 (GPIO_14 / J1 第 4 孔) 为数字输入，开内部弱上拉
    //    当老人按下随身遥控器时，接收模块内部继电器闭合，将 GPIO_14 拉低接地
    IoTGpioInit(PIN_WIRELESS_SOS);
    IoTGpioSetFunc(PIN_WIRELESS_SOS, IOT_GPIO_FUNC_GPIO_14_GPIO);
    IoTGpioSetDir(PIN_WIRELESS_SOS, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_WIRELESS_SOS, IOT_GPIO_PULL_UP);

    // 3. 初始化 F2 按键 (GPIO_12) 为数字输入，开内部弱上拉
    IoTGpioInit(PIN_KEY_RESET);
    IoTGpioSetFunc(PIN_KEY_RESET, IOT_GPIO_FUNC_GPIO_12_GPIO);
    IoTGpioSetDir(PIN_KEY_RESET, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_KEY_RESET, IOT_GPIO_PULL_UP);

    g_sos_state = 0;
    g_sos_source = SOS_SRC_NONE;
    g_f1_history = 0xFF;
    g_wireless_history = 0xFF;
    g_f2_history = 0xFF;

    printf("[SOS] Emergency SOS Driver Initialized:\n");
    printf("      - Onboard Key F1: GPIO_11\n");
    printf("      - Wireless 433MHz SOS: GPIO_14 (J1 Pin 4)\n");
    printf("      - Reset Key F2: GPIO_12\n");
}

uint8_t EmergencySosIsActive(void)
{
    return g_sos_state;
}

sos_source_t EmergencySosGetSource(void)
{
    return g_sos_source;
}

void EmergencySosTriggerWithSource(sos_source_t source)
{
    g_sos_state = 1;
    g_sos_source = source;
    if (source == SOS_SRC_WIRELESS_REMOTE) {
        printf("\n=======================================================\n");
        printf(">>> [433MHz WIRELESS REMOTE SOS] Triggered by Elder! <<<\n");
        printf("=======================================================\n\n");
    } else {
        printf("\n=======================================================\n");
        printf(">>> [ONBOARD KEY F1 SOS] Triggered locally by Elder! <<<\n");
        printf("=======================================================\n\n");
    }
}

void EmergencySosTrigger(void)
{
    EmergencySosTriggerWithSource(SOS_SRC_KEY_F1);
}

void EmergencySosClear(void)
{
    if (g_sos_state) {
        g_sos_state = 0;
        g_sos_source = SOS_SRC_NONE;
        AlarmBeeperBeepShort();
        printf("[SOS] Alarm cleared and reset locally by caregiver/elder.\n");
    }
}

void EmergencySosPoll(void)
{
    IotGpioValue val_f1 = IOT_GPIO_VALUE1;
    IotGpioValue val_wireless = IOT_GPIO_VALUE1;
    IotGpioValue val_f2 = IOT_GPIO_VALUE1;

    IoTGpioGetInputVal(PIN_KEY_SOS, &val_f1);
    IoTGpioGetInputVal(PIN_WIRELESS_SOS, &val_wireless);
    IoTGpioGetInputVal(PIN_KEY_RESET, &val_f2);

    // 1. F1 板载按键滑动窗口去抖 (按下接地 = 0)
    g_f1_history = ((g_f1_history << 1) | (val_f1 == IOT_GPIO_VALUE0 ? 0 : 1));
    if (g_f1_history == 0x00 && !g_sos_state) {
        EmergencySosTriggerWithSource(SOS_SRC_KEY_F1);
    }

    // 2. 433MHz 随身无线遥控器去抖 (遥控按下继电器导通接地 = 0)
    g_wireless_history = ((g_wireless_history << 1) | (val_wireless == IOT_GPIO_VALUE0 ? 0 : 1));
    if (g_wireless_history == 0x00 && !g_sos_state) {
        EmergencySosTriggerWithSource(SOS_SRC_WIRELESS_REMOTE);
    }

    // 3. F2 消警按键去抖 (按下接地 = 0)
    g_f2_history = ((g_f2_history << 1) | (val_f2 == IOT_GPIO_VALUE0 ? 0 : 1));
    if (g_f2_history == 0x00) {
        EmergencySosClear();
        AlarmBeeperMute();
    }
}
