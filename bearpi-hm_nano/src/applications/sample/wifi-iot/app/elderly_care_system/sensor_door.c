/*
 * 干簧管门窗磁传感器驱动 (GPIO_13)
 * 原理: 干簧管一端接 GPIO_13，另一端接 GND
 * 磁铁靠近吸合 -> 闭合接地 (电平 0，表示门窗闭合)
 * 门开磁铁离开 -> 触点断开，被内部上拉拉高 (电平 1，表示门窗打开)
 */

#include "sensor_door.h"
#include <stdio.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"

static volatile uint8_t  g_door_is_open = 0;
static volatile uint32_t g_door_open_duration = 0;
static uint8_t           g_debounce_history = 0;

void SensorDoorInit(void)
{
    // GPIO_13 初始化为标准数字 GPIO 输入，并开启内部上拉
    IoTGpioInit(PIN_DOOR_REED);
    IoTGpioSetFunc(PIN_DOOR_REED, IOT_GPIO_FUNC_GPIO_13_GPIO);
    IoTGpioSetDir(PIN_DOOR_REED, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(PIN_DOOR_REED, IOT_GPIO_PULL_UP);

    g_door_is_open = 0;
    g_door_open_duration = 0;
    g_debounce_history = 0;

    printf("[DOOR] Reed switch door/window sensor initialized on GPIO_13.\n");
}

uint8_t SensorDoorIsOpen(void)
{
    IotGpioValue val = IOT_GPIO_VALUE0;
    IoTGpioGetInputVal(PIN_DOOR_REED, &val);

    // 软件滑动滤波
    g_debounce_history = ((g_debounce_history << 1) | (val == IOT_GPIO_VALUE1 ? 1 : 0)) & 0x07;
    if (g_debounce_history == 0x07) {
        g_door_is_open = 1; // 连续 3 次检测均为高电平 -> 确认开门
    } else if (g_debounce_history == 0x00) {
        g_door_is_open = 0; // 连续 3 次检测均为低电平 -> 确认关门
    }
    return g_door_is_open;
}

uint32_t SensorDoorGetOpenDuration(void)
{
    return g_door_open_duration;
}

void SensorDoorTick1s(void)
{
    if (SensorDoorIsOpen()) {
        g_door_open_duration++;
    } else {
        g_door_open_duration = 0;
    }
}
