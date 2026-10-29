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

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio_ex.h"
#include "iot_gpio.h"
#include "pca9555.h"

static void TraColorLampControl(void)
{
    PCA9555Init();
    SetPCA9555GpioValue(PCA9555_PART1_IODIR, PCA9555_OUTPUT);
    SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, LED_OFF);

    /*
     * 控制左三色车灯跑马灯，绿、蓝、红、白：每隔一秒一次亮
     * 绿灯：IO1_3 ==> 0000 1000 ==> 0x08
     * 蓝灯：IO1_4 ==> 0001 0000 ==> 0x10
     * 红灯：IO1_5 ==>  0010 0000 ==> 0x20
     * 白灯：三灯全亮 ==>  0011 1000 ==> 0x38
     * Control the left tricolor running lights, green, blue, red and white: light up once every second
     * Green light: IO1_ 3 ==> 0000 1000 ==> 0x08
     * Blue light: IO1_4 ==> 0001 0000 ==> 0x10
     * Red light: IO1_5 ==>  0010 0000 ==> 0x20
     * White light: all three lights are on==>0011 1000==>0x38
     */
    while (1) {
        /*
         * 设置绿灯：IO1_3 输出高电平点亮左车绿灯
         * Set green light: IO1_ 3 Output high level to turn on the left green light
         */
        SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, GREEN_LED);
        /*
         * 延时函数毫秒（设置高电平持续时间）
         * Delay function milliseconds (set high level duration)
         */
        TaskMsleep(DELAY_MS);
        /*
         * 设置 蓝灯：IO1_4 输出高电平点亮左车蓝灯
         * Set blue light: IO1_ 4 Output high to turn on the left car blue light
         */
        SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, BLUE_LED);
        /*
         * 延时函数毫秒（设置高电平持续时间）
         * Delay function milliseconds (set high level duration)
         */
        TaskMsleep(DELAY_MS);
        /*
         * 设置红灯：IO1_3 输出高电平点亮左车红灯
         * Set red light: IO1_ 3 Output high level to turn on the left vehicle red light
         */
        SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, RED_LED);
        /*
         * 延时函数毫秒（设置高电平持续时间）
         * Delay function milliseconds (set high level duration)
         */
        TaskMsleep(DELAY_MS);
        /*
         * 设置 IO1_3 IO1_4 IO1_5 都输出高电平，左车亮白灯
         * Set IO1_ 3 IO1_ 4 IO1_ 5 output high level, left vehicle white light
         */
        SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, WHITE_LED);
        TaskMsleep(DELAY_MS);
    }
}

static void TraColorLampControlEntry(void)
{
    osThreadAttr_t attr;
    attr.name = "LedCntrolDemo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024; /* 堆栈大小为1024 stack size 1024 */
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)TraColorLampControl, NULL, &attr) == NULL) {
        printf("[LedExample] Failed to create LedTask!\n");
    }
}

APP_FEATURE_INIT(TraColorLampControlEntry);