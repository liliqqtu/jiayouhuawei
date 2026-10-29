/**
 * @file    drv.c
 * @brief   PIR / 按键 / LED 驱动实现（Hi3861）
 *
 * 依赖：hi_gpio.h、hi_io.h（OpenHarmony 轻量系统 SDK 自带）
 *
 * 引脚说明：
 *   - PIR 走 E53 扩展座的 GPIO11（官方 e53_is1_example 用的就是这个）
 *   - 按键用板载 USER 键，默认 GPIO12
 *   - LED 用板载 LED，默认 GPIO2
 * 若与你手上的板子不一致，只改 config.h 里的三个宏，以及下面的 xxx_FUNC。
 */

#include <stdio.h>
#include <unistd.h>

#include "hi_gpio.h"
#include "hi_io.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "config.h"
#include "drv.h"

/* GPIO 功能复用值：必须与引脚号对应。
 * 例如 GPIO11 对应 HI_IO_FUNC_GPIO_11_GPIO。
 * 如果编译提示这些宏未定义，说明 SDK 版本不同，按 SDK 里 hi_io.h 的枚举改。 */
#define PIR_FUNC   HI_IO_FUNC_GPIO_11_GPIO
#define KEY_FUNC   HI_IO_FUNC_GPIO_12_GPIO
#define LED_FUNC   HI_IO_FUNC_GPIO_2_GPIO

/* LED 有些板子是低电平点亮；如果你的灯逻辑相反，把这里改成 1 */
#define LED_ACTIVE_LOW  0

static hi_u8 g_key_last = 1;        /* 上次稳定电平（1=未按下，按键默认上拉） */
static hi_u8 g_key_stable = 1;
static hi_u32 g_key_change_tick = 0;

void Drv_Init(void)
{
    hi_gpio_init();

    /* PIR：输入 + 下拉（无人时保持低电平；有些模块自带输出驱动，可去掉下拉） */
    hi_io_set_func(CFG_PIR_GPIO, PIR_FUNC);
    hi_gpio_set_dir(CFG_PIR_GPIO, HI_GPIO_DIR_IN);
    hi_io_set_pull(CFG_PIR_GPIO, HI_IO_PULL_DOWN);

    /* 按键：输入 + 上拉，按下为低 */
    hi_io_set_func(CFG_KEY_GPIO, KEY_FUNC);
    hi_gpio_set_dir(CFG_KEY_GPIO, HI_GPIO_DIR_IN);
    hi_io_set_pull(CFG_KEY_GPIO, HI_IO_PULL_UP);

#if (CFG_DOOR_MODE == 1)
    /* 门磁：输入 + 上拉，干簧管闭合时拉低 */
    hi_io_set_func(CFG_DOOR_GPIO, CFG_DOOR_FUNC);
    hi_gpio_set_dir(CFG_DOOR_GPIO, HI_GPIO_DIR_IN);
    hi_io_set_pull(CFG_DOOR_GPIO, HI_IO_PULL_UP);
#endif

    /* LED：输出 */
    hi_io_set_func(CFG_LED_GPIO, LED_FUNC);
    hi_gpio_set_dir(CFG_LED_GPIO, HI_GPIO_DIR_OUT);
    Drv_LedSet(0);

#if (CFG_BUZZER_EN == 1)
    /* 蜂鸣器：输出，上电默认不响 */
    hi_io_set_func(CFG_BUZZER_GPIO, CFG_BUZZER_FUNC);
    hi_gpio_set_dir(CFG_BUZZER_GPIO, HI_GPIO_DIR_OUT);
    Drv_BuzzerSet(0);
#endif

    printf("[drv] init ok (pir=%d key=%d led=%d)\r\n",
           (int)CFG_PIR_GPIO, (int)CFG_KEY_GPIO, (int)CFG_LED_GPIO);
}

hi_u8 Drv_PirTriggered(void)
{
    hi_gpio_value val = HI_GPIO_VALUE0;
    if (hi_gpio_get_input_val(CFG_PIR_GPIO, &val) != HI_ERR_SUCCESS) {
        return 0;
    }
    return (val == HI_GPIO_VALUE1) ? 1 : 0;
}

/**
 * 按键边沿检测 + 去抖
 * 思路：电平变化后先记时间，稳定超过 CFG_KEY_DEBOUNCE_MS 才认；
 *      只有“未按下 -> 按下”这一个方向返回 1，避免长按重复触发。
 */
hi_u8 Drv_KeyPressedEdge(void)
{
    hi_gpio_value val = HI_GPIO_VALUE1;
    if (hi_gpio_get_input_val(CFG_KEY_GPIO, &val) != HI_ERR_SUCCESS) {
        return 0;
    }

    hi_u8 raw = (val == HI_GPIO_VALUE0) ? 0 : 1;   /* 0 = 按下 */
    hi_u32 now = hi_get_milli_seconds();

    if (raw != g_key_last) {
        g_key_last = raw;
        g_key_change_tick = now;
        return 0;
    }

    /* 电平已稳定足够久，且与上次确认值不同 -> 采纳新状态 */
    if (raw != g_key_stable && (now - g_key_change_tick) >= CFG_KEY_DEBOUNCE_MS) {
        g_key_stable = raw;
        if (g_key_stable == 0) {
            return 1;   /* 刚按下 */
        }
    }
    return 0;
}

void Drv_LedSet(hi_u8 on)
{
#if LED_ACTIVE_LOW
    hi_gpio_set_ouput_val(CFG_LED_GPIO, on ? HI_GPIO_VALUE0 : HI_GPIO_VALUE1);
#else
    hi_gpio_set_ouput_val(CFG_LED_GPIO, on ? HI_GPIO_VALUE1 : HI_GPIO_VALUE0);
#endif
}

hi_u8 Drv_KeyDown(void)
{
    hi_gpio_value val = HI_GPIO_VALUE1;
    if (hi_gpio_get_input_val(CFG_KEY_GPIO, &val) != HI_ERR_SUCCESS) {
        return 0;
    }
    return (val == HI_GPIO_VALUE0) ? 1 : 0;
}

hi_s8 Drv_DoorOpen(void)
{
#if (CFG_DOOR_MODE == 1)
    hi_gpio_value val = HI_GPIO_VALUE1;
    if (hi_gpio_get_input_val(CFG_DOOR_GPIO, &val) != HI_ERR_SUCCESS) {
        return -1;
    }
    /* 上拉 + 干簧管闭合拉低：低电平 = 门关闭 */
    return (val == HI_GPIO_VALUE0) ? 0 : 1;
#else
    /* 未接硬件或按键模拟：由上层决定是否写 null / 模拟值 */
    return -1;
#endif
}

/**
 * 蜂鸣器开关。
 * 没接蜂鸣器（CFG_BUZZER_EN=0）时是空实现：代码里可以照常调用，
 * 板子上只是没有声音，不会报错，也不会让“有没有声”变成一句空头承诺。
 */
void Drv_BuzzerSet(hi_u8 on)
{
#if (CFG_BUZZER_EN == 1)
#if (CFG_BUZZER_ACTIVE_LOW == 1)
    hi_gpio_set_ouput_val(CFG_BUZZER_GPIO, on ? HI_GPIO_VALUE0 : HI_GPIO_VALUE1);
#else
    hi_gpio_set_ouput_val(CFG_BUZZER_GPIO, on ? HI_GPIO_VALUE1 : HI_GPIO_VALUE0);
#endif
#else
    (void)on;
#endif
}
