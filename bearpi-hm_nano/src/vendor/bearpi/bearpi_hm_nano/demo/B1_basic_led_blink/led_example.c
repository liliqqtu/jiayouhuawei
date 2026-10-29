/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
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
#include "iot_gpio.h"
#include "ohos_init.h"

#define LED_TASK_STACK_SIZE (1024 * 4)
#define LED_TASK_PRIO 25
#define LED_GPIO 2

/**
 * @brief led task output high and low levels to turn on and off LED
 *
 */
static void LedTask(void)
{
    // init gpio of LED
    IoTGpioInit(LED_GPIO);

    // set GPIO_2 is output mode
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);

    while (1) {
        // set GPIO_2 output high levels to turn on LED
        IoTGpioSetOutputVal(LED_GPIO, 1);

        // delay 1s
        sleep(1);

        // set GPIO_2 output low levels to turn off LED
        IoTGpioSetOutputVal(LED_GPIO, 0);

        // delay 1s
        sleep(1);
    }
}

/**
 * @brief Main Entry of the Led Example
 *
 */
static void LedExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "LedTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = LED_TASK_STACK_SIZE;
    attr.priority = LED_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)LedTask, NULL, &attr) == NULL) {
        printf("Failed to create LedTask!\n");
    }
}

APP_FEATURE_INIT(LedExampleEntry);