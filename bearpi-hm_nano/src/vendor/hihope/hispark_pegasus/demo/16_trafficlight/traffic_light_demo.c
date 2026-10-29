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
#include "iot_watchdog.h"
#include "iot_pwm.h"
#include "hi_io.h"

static int g_ledStates[3] = {0, 0, 0};
static int g_currentBright = 0;
static int g_beepState = 0;

#define     STACK_SIZE             (1024)
#define     PWM_DUTY_50            (50)
#define     PWM_FREQ_4K            (4000)
#define     LIGHT_NUM              (3)
#define     DELAY_100MS            (100 * 1000)
#define     DELAY_200MS            (200 * 1000)
#define     DELAY_500MS            (500 * 1000)


#define     IOT_GPIO_IDX_10    10  // Red
#define     IOT_GPIO_IDX_11    11  // Green
#define     IOT_GPIO_IDX_12    12  // Yellow
#define     IOT_GPIO_IDX_8     8   // Switch
#define     IOT_GPIO_IDX_9     9   // Beep
#define     IOT_PWM_PORT_PWM0  0

static void *TrafficLightTask(void)
{
    printf("TrafficLightTask start!\r\n");
    static int g_blinkCnt = 5;
    static int g_beepCnt = 1000;

    unsigned int  pins[LIGHT_NUM] = {IOT_GPIO_IDX_10, IOT_GPIO_IDX_12, IOT_GPIO_IDX_11};
    for (int i = 0; i < g_blinkCnt; i++) {
        for (unsigned int j = 0; j < LIGHT_NUM; j++) {
            IoTGpioSetOutputVal(pins[j], IOT_GPIO_VALUE1);
            usleep(DELAY_200MS);

            IoTGpioSetOutputVal(pins[j], IOT_GPIO_VALUE0);
            usleep(DELAY_100MS);
        }
    }

    while (g_beepCnt--) {
        for (unsigned int j = 0; j < LIGHT_NUM; j++) {
            IoTGpioSetOutputVal(pins[j], g_ledStates[j]);
        }
        if (g_beepState) {
            IoTPwmStart(IOT_PWM_PORT_PWM0, PWM_DUTY_50, PWM_FREQ_4K);
        } else {
            IoTPwmStop(IOT_PWM_PORT_PWM0);
        }
        usleep(DELAY_500MS);
    }

    return NULL;
}

static void OnButtonPressed(void)
{
    for (int i = 0; i < LIGHT_NUM; i++) {
        if (i == g_currentBright) {
            g_ledStates[i] = 1;
        } else {
            g_ledStates[i] = 0;
        }
    }
    g_currentBright++;
    if (g_currentBright == LIGHT_NUM) {
        g_currentBright = 0;
    }

    g_beepState = !g_beepState;
}

static void StartTrafficLightTask(void)
{
    osThreadAttr_t attr;

    IoTGpioInit(IOT_GPIO_IDX_10);
    IoTGpioSetDir(IOT_GPIO_IDX_10, IOT_GPIO_DIR_OUT);

    IoTGpioInit(IOT_GPIO_IDX_11);
    IoTGpioSetDir(IOT_GPIO_IDX_11, IOT_GPIO_DIR_OUT);

    IoTGpioInit(IOT_GPIO_IDX_12);
    IoTGpioSetDir(IOT_GPIO_IDX_12, IOT_GPIO_DIR_OUT);

    IoTGpioInit(IOT_GPIO_IDX_8);
    hi_io_set_func(IOT_GPIO_IDX_8, HI_IO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(IOT_GPIO_IDX_8, IOT_GPIO_DIR_IN);
    hi_io_set_pull(IOT_GPIO_IDX_8, HI_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(IOT_GPIO_IDX_8, IOT_INT_TYPE_EDGE,
        IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnButtonPressed, NULL);

    IoTGpioInit(IOT_GPIO_IDX_9);
    hi_io_set_func(IOT_GPIO_IDX_9, HI_IO_FUNC_GPIO_9_PWM0_OUT);
    IoTGpioSetDir(IOT_GPIO_IDX_9, IOT_GPIO_DIR_OUT);
    IoTPwmInit(IOT_PWM_PORT_PWM0);

    IoTWatchDogDisable();

    attr.name = "TrafficLightTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)TrafficLightTask, NULL, &attr) == NULL) {
        printf("[LedExample] Falied to create TrafficLightTask!\n");
    }
}

APP_FEATURE_INIT(StartTrafficLightTask);