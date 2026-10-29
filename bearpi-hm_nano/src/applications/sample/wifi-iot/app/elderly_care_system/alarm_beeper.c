/*
 * 本地声光联动报警驱动 (E53_IS1 蜂鸣器 + 板载 LED)
 */

#include "alarm_beeper.h"
#include <stdio.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_pwm.h"
#include "cmsis_os2.h"

static alert_level_t g_current_level = ALERT_LEVEL_NORMAL;
static alert_type_t  g_current_type  = ALERT_TYPE_NONE;
static uint8_t       g_is_muted      = 0;
static uint32_t      g_pattern_tick  = 0;
static uint8_t       g_beeper_active = 0;

static void BeeperOn(void)
{
    if (!g_beeper_active) {
        IoTPwmStart(PWM_PORT_BEEPER, 50, 4000); // 4000Hz, 50% duty
        g_beeper_active = 1;
    }
}

static void BeeperOff(void)
{
    if (g_beeper_active) {
        IoTPwmStop(PWM_PORT_BEEPER);
        g_beeper_active = 0;
    }
}

static void LedOn(void)
{
    IoTGpioSetOutputVal(PIN_LED_SYS, IOT_GPIO_VALUE1);
}

static void LedOff(void)
{
    IoTGpioSetOutputVal(PIN_LED_SYS, IOT_GPIO_VALUE0);
}

void AlarmBeeperInit(void)
{
    // 初始化蜂鸣器引脚 GPIO_8 为 PWM1
    IoTGpioInit(PIN_BEEPER);
    IoTGpioSetFunc(PIN_BEEPER, IOT_GPIO_FUNC_GPIO_8_PWM1_OUT);
    IoTGpioSetDir(PIN_BEEPER, IOT_GPIO_DIR_OUT);
    IoTPwmInit(PWM_PORT_BEEPER);
    BeeperOff();

    // 初始化板载 LED 引脚 GPIO_2
    IoTGpioInit(PIN_LED_SYS);
    IoTGpioSetFunc(PIN_LED_SYS, IOT_GPIO_FUNC_GPIO_2_GPIO);
    IoTGpioSetDir(PIN_LED_SYS, IOT_GPIO_DIR_OUT);
    LedOff();

    g_current_level = ALERT_LEVEL_NORMAL;
    g_current_type  = ALERT_TYPE_NONE;
    g_is_muted      = 0;
    g_pattern_tick  = 0;

    // 自检开机提示音 (短鸣 100ms)
    AlarmBeeperBeepShort();
    printf("[ALARM] Beeper and LED hardware initialized successfully.\n");
}

void AlarmBeeperSetMode(alert_level_t level, alert_type_t type)
{
    if (g_current_level != level || g_current_type != type) {
        g_current_level = level;
        g_current_type = type;
        g_pattern_tick = 0;
        // 如果出现新的严重报警，解除静音锁
        if (level == ALERT_LEVEL_CRITICAL) {
            g_is_muted = 0;
        }
    }
}

void AlarmBeeperMute(void)
{
    g_is_muted = 1;
    BeeperOff();
    printf("[ALARM] Alarm sound muted locally.\n");
}

void AlarmBeeperBeepShort(void)
{
    BeeperOn();
    LedOn();
    osDelay(10); // 约 100ms
    BeeperOff();
    LedOff();
}

void AlarmBeeperTick(uint32_t tick_step_ms)
{
    g_pattern_tick += tick_step_ms;

    if (g_current_level == ALERT_LEVEL_NORMAL) {
        // 正常安全模式: 2000ms 一个周期，LED 短闪 100ms (心跳指示)
        uint32_t cycle = g_pattern_tick % 2000;
        if (cycle < 100) {
            LedOn();
        } else {
            LedOff();
        }
        BeeperOff();
    }
    else if (g_current_level == ALERT_LEVEL_WARNING) {
        // 预警模式 (如门长开): 1500ms 周期，LED 双闪，蜂鸣器在前半段短鸣两声
        uint32_t cycle = g_pattern_tick % 1500;
        if (cycle < 120 || (cycle >= 240 && cycle < 360)) {
            LedOn();
            if (!g_is_muted) BeeperOn();
            else BeeperOff();
        } else {
            LedOff();
            BeeperOff();
        }
    }
    else if (g_current_level == ALERT_LEVEL_CRITICAL) {
        // 严重告警模式 (SOS 或 摔倒/超时无活动): 400ms 急促周期，急促连环啸叫爆闪
        uint32_t cycle = g_pattern_tick % 400;
        if (cycle < 200) {
            LedOn();
            if (!g_is_muted) BeeperOn();
            else BeeperOff();
        } else {
            LedOff();
            BeeperOff();
        }
    }
}
