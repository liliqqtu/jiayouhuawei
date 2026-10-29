/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
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

#include "iot_gpio_ex.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"

#define LED_LOOP    10
#define DELYA_MS    1000

static void LedExampleEntry(void)
{
    // LED3的GPIO初始化 GPIO initialization of LED3
    IoTGpioInit(IOT_IO_NAME_GPIO_9);
    // 设置GPIO9的管脚复用关系为GPIO Set the pin reuse relationship of GPIO9 to GPIO
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_GPIO);
    // GPIO方向设置为输出 GPIO direction set to output
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);

    for (int i = 0; i < LED_LOOP; i++) {
        /*
         * 设置GPIO09输出高电平点亮红色交通灯LED3
         * Set GPIO09 output high level to turn on red traffic light LED 3
         */
        IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
        TaskMsleep(DELYA_MS);
        /*
         * 设置GPIO09输出低电平熄灭红色交通灯LED3
         * Set GPIO09 output low level to turn off red traffic light LED 3
         */
        IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
        TaskMsleep(DELYA_MS);
    }
}

static void LedControlTask(void)
{
    osThreadAttr_t attr;

    attr.name = "LedCntrolDemo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024; // 堆栈大小为1024 stack size 1024
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)LedExampleEntry, NULL, &attr) == NULL) {
        printf("[LedExample] Failed to create LedTask!\n");
    }
}

APP_FEATURE_INIT(LedControlTask);