/*
 * Copyright (C) 2021 HiHope Open Source Organization .
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
 *
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "hi_pwm.h"
#include "hi_io.h"

#define     STACK_SIZE             (1024)
#define     PWM_DUTY_50            (50)
#define     PWM_FREQ_4K            (4000)

static int g_beepCount = 1000;  // beep test max 1000 times
static int g_beepState = 0;
#define     CLK_160M                160000000
#define     IOT_GPIO_IDX_9          9
#define     IOT_GPIO_IDX_8          8
#define     IOT_GPIO_PWM_FUNCTION   5
#define     IOT_PWM_PORT_PWM0       0
#define     IO_FUNC_GPIO_8_GPIO     0
#define     IOT_IO_PULL_UP          1

static void *PWMBeerTask(void)
{
    printf("PWMBeerTask start!\r\n");

    while (g_beepCount) {
        if (g_beepState) {
            IoTPwmStart(IOT_PWM_PORT_PWM0, PWM_DUTY_50, PWM_FREQ_4K);
        } else {
            IoTPwmStop(IOT_PWM_PORT_PWM0);
        }
    }

    return NULL;
}

static void OnButtonPressed(void)
{
    g_beepState = !g_beepState;
    g_beepCount--;
}

static void StartPWMBeerTask(void)
{
    osThreadAttr_t attr;

    IoTGpioInit(IOT_GPIO_IDX_9);
    hi_io_set_func(IOT_GPIO_IDX_9, IOT_GPIO_PWM_FUNCTION);
    IoTGpioSetDir(IOT_GPIO_IDX_9, IOT_GPIO_DIR_OUT);
    IoTPwmInit(IOT_PWM_PORT_PWM0);

    hi_io_set_func(IOT_GPIO_IDX_8, IO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(IOT_GPIO_IDX_8, IOT_GPIO_DIR_IN);
    hi_io_set_pull(IOT_GPIO_IDX_8, IOT_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(IOT_GPIO_IDX_8, IOT_INT_TYPE_EDGE,
        IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnButtonPressed, NULL);

    IoTWatchDogDisable();

    attr.name = "PWMBeerTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)PWMBeerTask, NULL, &attr) == NULL) {
        printf("[StartPWMBeerTask] Falied to create PWMBeerTask!\n");
    }
}
APP_FEATURE_INIT(StartPWMBeerTask);