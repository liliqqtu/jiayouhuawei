/*
 * Copyright (C) 2023 HiHope Open Source Organization .
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_pwm.h"
#include "iot_errno.h"
#include "hi_io.h"

#define RED_LED_PIN_NAME 10
#define RED_LED_PIN_FUNCTION WIFI_IOT_IO_FUNC_GPIO_10_GPIO

#define IOT_PWM_DUTY_MAX  100
#define PWM_FREQ_DIVITION 64000
#define DELAY_US 250000
#define STACK_SIZE (4096)
#define PWM_PORT_NUM  (1)
#define DOUBLE  (2)

static void PWMLedDemoTask(void)
{
    static int g_count = 100;

    // 炫彩灯板的红灯
    hi_io_set_func(RED_LED_PIN_NAME, HI_IO_FUNC_GPIO_10_PWM1_OUT);
    IoTPwmInit(PWM_PORT_NUM);

    while (g_count) {
        // use PWM control RED LED brightness
        for (int i = 1; i < IOT_PWM_DUTY_MAX; i *= DOUBLE) {
            IoTPwmStart(PWM_PORT_NUM, i, PWM_FREQ_DIVITION);
            usleep(DELAY_US);
            IoTPwmStop(PWM_PORT_NUM);
        }
        g_count--;
    }
}

static void PWMLedDemo(void)
{
    osThreadAttr_t attr;

    // set Red LED pin to GPIO function
    IoTGpioInit(RED_LED_PIN_NAME);

    attr.name = "PWMLedDemoTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(PWMLedDemoTask, NULL, &attr) == NULL) {
        printf("[ColorfulLightDemo] Falied to create PWMLedDemoTask!\n");
    }
}
APP_FEATURE_INIT(PWMLedDemo);
