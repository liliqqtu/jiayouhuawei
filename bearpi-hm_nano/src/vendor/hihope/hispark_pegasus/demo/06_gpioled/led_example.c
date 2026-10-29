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

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "iot_gpio.h"

#define LED_TASK_GPIO 9
#define LED_TASK_STACK_SIZE 1024
#define LED_TASK_PRIO   25
#define DELAY_TICKS_50 (50)
#define GPIO_HIGH       (1)
#define GPIO_LOW        (0)

static void* GpioTask(void)
{
    static int count = 100;

    IoTGpioInit(LED_TASK_GPIO);
    IoTGpioSetDir(LED_TASK_GPIO, IOT_GPIO_DIR_OUT);

    while (count--) {
        printf(" LED_SPARK!\n");
        IoTGpioSetOutputVal(LED_TASK_GPIO, GPIO_LOW);
        osDelay(DELAY_TICKS_50);
        IoTGpioSetOutputVal(LED_TASK_GPIO, GPIO_HIGH);
        osDelay(DELAY_TICKS_50);
    }
    return NULL;
}

static void GpioExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "GpioTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = LED_TASK_STACK_SIZE;
    attr.priority = LED_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)GpioTask, NULL, &attr) == NULL) {
        printf("[GpioExample] Falied to create GpioTask!\n");
    }
}
SYS_RUN(GpioExampleEntry);
