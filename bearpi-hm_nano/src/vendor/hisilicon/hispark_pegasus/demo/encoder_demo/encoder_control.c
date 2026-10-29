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

/*
 * 电机编码器的相关API接口
 * Relevant API interface of motor encoder
 */

#include <stdio.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_gpio_ex.h"
#include "hi_time.h"

static int encoderLeftACounter = 0;
static int encoderLeftBCounter = 0;
static int rc = 0;
#define FLAG_BIT    2

/*
 * 左电机中断处理函数
 * Left motor interrupt processing function
 */
static void LeftACounterHandler(char *arg)
{
    (void) arg;
    encoderLeftACounter++;
    if (encoderLeftACounter == 1 && encoderLeftBCounter == 0) {
        rc = 1;
    }
    if (rc == 1) {
        printf("A , forward, encoderLeftACounter = %d\r\n", encoderLeftACounter);
    }
}

/*
 * 左电机中断处理函数
 * Left motor interrupt processing function
 */
static void LeftBCounterHandler(char *arg)
{
    (void) arg;
    encoderLeftBCounter++;
    if (encoderLeftACounter == 0 && encoderLeftBCounter == 1) {
        rc = FLAG_BIT;
    }
    if (rc == FLAG_BIT) {
        printf("B, backward, encoderLeftBCounter = %d\r\n", encoderLeftBCounter);
    }
}

void EncoderInit(void)
{
    /*
     * 左侧电机编码器B相的GPIO初始化
     * GPIO initialization of left motor encoder phase B
     */
    IoTGpioInit(IOT_IO_NAME_GPIO_0);
    /*
     * 设置GPIO0的管脚复用关系为GPIO
     * Set the pin reuse relationship of GPIO0 to GPIO
     */
    IoSetFunc(IOT_IO_NAME_GPIO_0, IOT_IO_FUNC_GPIO_0_GPIO);
    /*
     * GPIO0方向设置为输入
     * GPIO0 direction is set as input
     */
    IoTGpioSetDir(IOT_IO_NAME_GPIO_0, IOT_GPIO_DIR_IN);
    /*
     * 设置GPIO0为上拉功能
     * Set GPIO0 as pull-up function
     */
    IoSetPull(IOT_IO_NAME_GPIO_0, IOT_IO_PULL_UP);
    /*
     * 使能GPIO0的中断功能, 上升沿触发中断，LeftBCounterHandler为中断的回调函数
     * Enable the interrupt function of GPIO0, the rising edge triggers
     * the interrupt, and LeftBCounterHandler is the interrupt callback function
     */
    IoTGpioRegisterIsrFunc(IOT_IO_NAME_GPIO_0, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_RISE_LEVEL_HIGH, LeftBCounterHandler, NULL);
    /*
     * 左侧电机编码器A相的GPIO初始化
     * GPIO initialization of left motor encoder phase A
     */
    IoSetFunc(IOT_IO_NAME_GPIO_12, IOT_IO_FUNC_GPIO_12_GPIO);
    /*
     * 设置GPIO12的管脚复用关系为GPIO
     * Set the pin reuse relationship of GPIO12 to GPIO
     */
    IoTGpioSetDir(IOT_IO_NAME_GPIO_12, IOT_GPIO_DIR_IN);
    /*
     * 设置GPIO12为上拉功能
     * Set GPIO12 as pull-up function
     */
    IoSetPull(IOT_IO_NAME_GPIO_12, IOT_IO_PULL_UP);
    /*
     * 使能GPIO1的中断功能, 上升沿触发中断，LeftACounterHandler 为中断的回调函数
     * Enable the interrupt function of GPIO1, the rising edge triggers the interrupt,
     * and LeftACounterHandler is the interrupt callback function
     */
    IoTGpioRegisterIsrFunc(IOT_IO_NAME_GPIO_12, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_RISE_LEVEL_HIGH, LeftACounterHandler, NULL);
}

void EncoderEntry(void)
{
    // 左侧电机编码器初始化
    EncoderInit();
}

static void EncoderControlTask(void)
{
    osThreadAttr_t attr;

    attr.name = "LedCntrolDemo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024; // 堆栈大小为1024，stack size 1024
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)EncoderEntry, NULL, &attr) == NULL) {
        printf("[LedExample] Failed to create LedTask!\n");
    }
}

APP_FEATURE_INIT(EncoderControlTask);