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
#include "hi_io.h"

#define LED_TASK_GPIO    9
#define IOT_GPIO_KEY     5
#define DELAY_TICKS_30  (30)
#define LED_TASK_STACK_SIZE 512
#define LED_TASK_PRIO   (25)
#define GPIO_HIGH       (1)
#define GPIO_LOW        (0)

enum LedState {
    LED_ON = 0,
    LED_OFF,
    LED_SPARK,
};

enum LedState g_ledState = LED_SPARK;
static int g_count = 100;

static void *LedTask(void)
{
    while (g_count) {
        switch (g_ledState) {
            case LED_ON:
                IoTGpioSetOutputVal(LED_TASK_GPIO, GPIO_LOW);
                osDelay(DELAY_TICKS_30);
                break;
            case LED_OFF:
                IoTGpioSetOutputVal(LED_TASK_GPIO, GPIO_HIGH);
                osDelay(DELAY_TICKS_30);
                break;
            case LED_SPARK:
                IoTGpioSetOutputVal(LED_TASK_GPIO, GPIO_LOW);
                osDelay(DELAY_TICKS_30);
                IoTGpioSetOutputVal(LED_TASK_GPIO, GPIO_HIGH);
                osDelay(DELAY_TICKS_30);
                break;
            default:
                osDelay(DELAY_TICKS_30);
                break;
        }
    }

    return NULL;
}

static void OnButtonPressed(void)
{
    enum LedState nextState = LED_SPARK;
    switch (g_ledState) {
        case LED_ON:
            nextState = LED_OFF;
            break;
        case LED_OFF:
            nextState = LED_ON;
            break;
        case LED_SPARK:
            nextState = LED_OFF;
            break;
        default:
            break;
    }

    g_ledState = nextState;
    g_count--;
}

static void LedExampleEntry(void)
{
    osThreadAttr_t attr;

    IoTGpioInit(LED_TASK_GPIO);
    IoTGpioSetDir(LED_TASK_GPIO, IOT_GPIO_DIR_OUT);
 
    IoTGpioInit(IOT_GPIO_KEY);
    hi_io_set_func(IOT_GPIO_KEY, GPIO_LOW);
    IoTGpioSetDir(IOT_GPIO_KEY, IOT_GPIO_DIR_IN);
    hi_io_set_pull(IOT_GPIO_KEY, GPIO_HIGH);

    IoTGpioRegisterIsrFunc(IOT_GPIO_KEY, IOT_INT_TYPE_EDGE,
        IOT_GPIO_EDGE_FALL_LEVEL_LOW,
        OnButtonPressed, NULL);

    attr.name = "LedTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = LED_TASK_STACK_SIZE;
    attr.priority = LED_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)LedTask, NULL, &attr) == NULL) {
        printf("[LedExample] Falied to create LedTask!\n");
    }
}
SYS_RUN(LedExampleEntry);