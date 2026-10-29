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
    GA12-N20 直流减速电机的相关API接口
    左轮：IB:GPIO5, IA: GPIO6
    右轮：IB:GPIO9, IA: GPIO10
*/
#include <stdio.h>
#include <unistd.h>

#include "iot_gpio_ex.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_watchdog.h"
#include "iot_pwm.h"

#define IOT_PWM_PORT_PWM0   0
#define IOT_PWM_PORT_PWM1   1
#define IOT_PWM_PORT_PWM2   2
#define IOT_PWM_PORT_PWM3   3
#define IOT_FREQ            65535
#define IOT_DUTY            50

// PWM取值：分频系数[1, 65535] PWM value: frequency division coefficient [1, 65535]
void LeftWheelForword(void)
{
    IoTPwmStart(IOT_PWM_PORT_PWM2, IOT_DUTY, IOT_FREQ);
}

void LeftWheelBackword(void)
{
    IoTPwmStart(IOT_PWM_PORT_PWM3, IOT_DUTY, IOT_FREQ);
}

void LeftWheelStop(void)
{
    IoTPwmStop(IOT_PWM_PORT_PWM2);
    IoTPwmStop(IOT_PWM_PORT_PWM3);
}

void RightWheelForword(void)
{
    IoTPwmStart(IOT_PWM_PORT_PWM1, IOT_DUTY, IOT_FREQ);
}

void RightWheelBackword(void)
{
    IoTPwmStart(IOT_PWM_PORT_PWM0, IOT_DUTY, IOT_FREQ);
}

void RightWheelStop(void)
{
    IoTPwmStop(IOT_PWM_PORT_PWM0);
    IoTPwmStop(IOT_PWM_PORT_PWM1);
}

void GA12N20Init(void)
{
    // 左电机GPIO5,GPIO6初始化 Initialization of left motor GPIO5 and GPIO6
    IoTGpioInit(IOT_IO_NAME_GPIO_5);
    IoTGpioInit(IOT_IO_NAME_GPIO_6);
    // 右电机GPIO9, GPIO10初始化 Right motor GPIO9, GPIO10 initialization
    IoTGpioInit(IOT_IO_NAME_GPIO_9);
    IoTGpioInit(IOT_IO_NAME_GPIO_10);

    // 设置GPIO5的管脚复用关系为PWM2输出 Set the pin multiplexing relationship of GPIO5 to PWM2 output
    IoSetFunc(IOT_IO_NAME_GPIO_5, IOT_IO_FUNC_GPIO_5_PWM2_OUT);
    // 设置GPIO6的管脚复用关系为PWM3输出 Set the pin multiplexing relationship of GPIO6 to PWM3 output
    IoSetFunc(IOT_IO_NAME_GPIO_6, IOT_IO_FUNC_GPIO_6_PWM3_OUT);
    // 设置GPIO9的管脚复用关系为PWM0输出 Set the pin multiplexing relationship of GPIO9 to PWM0 output
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_PWM0_OUT);
    // 设置GPIO10的管脚复用关系为PWM01输出 Set the pin multiplexing relationship of GPIO10 to PWM01 output
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_PWM1_OUT);

    // GPIO5方向设置为输出 GPIO5 direction set to output
    IoTGpioSetDir(IOT_IO_NAME_GPIO_5, IOT_GPIO_DIR_OUT);
    // GPIO6方向设置为输出 GPIO6 direction set to output
    IoTGpioSetDir(IOT_IO_NAME_GPIO_6, IOT_GPIO_DIR_OUT);
    // GPIO9方向设置为输出 GPIO9 direction set to output
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);
    // GPIO10方向设置为输出 GPIO10 direction set to output
    IoTGpioSetDir(IOT_IO_NAME_GPIO_10, IOT_GPIO_DIR_OUT);
    // 初始化PWM2 Initialize PWM2
    IoTPwmInit(IOT_PWM_PORT_PWM2);
    // 初始化PWM3 Initialize PWM3
    IoTPwmInit(IOT_PWM_PORT_PWM3);
    // 初始化PWM0 Initialize PWM0
    IoTPwmInit(IOT_PWM_PORT_PWM0);
    // 初始化PWM1 Initialize PWM1
    IoTPwmInit(IOT_PWM_PORT_PWM1);
    // 先使两个电机处于停止状态 motors stop
    RightWheelStop();
    LeftWheelStop();
}

void GA12N205Task(void)
{
    // 初始化电机模块 Initialize the motor module
    GA12N20Init();
    // 实现左电机向前转动 Realize the forward rotation of the left motor
    LeftWheelForword();
    // 实现右电机向前转动 The right motor rotates forward
    RightWheelForword();
}

void GA12N20SampleEntry(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "GA12N205Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 5; // 堆栈大小为1024*5,stack size 1024*5
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)GA12N205Task, NULL, &attr) == NULL) {
        printf("[GA12N205Task] Failed to create Hcsr04SampleTask!\n");
    }
}

APP_FEATURE_INIT(GA12N20SampleEntry);