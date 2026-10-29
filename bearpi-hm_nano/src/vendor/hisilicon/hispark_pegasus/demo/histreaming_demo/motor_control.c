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
#include "iot_pwm.h"
#include "iot_gpio.h"
#include "motor_control.h"

#define IOT_PWM_PORT_PWM0   0
#define IOT_PWM_PORT_PWM1   1
#define IOT_PWM_PORT_PWM2   2
#define IOT_PWM_PORT_PWM3   3
#define IOT_FREQ            65535
#define IOT_DUTY1            10
#define IOT_DUTY2            30

void GA12N20Init(void)
{
    // 左电机GPIO5,GPIO6初始化
    IoTGpioInit(IOT_IO_NAME_GPIO_5);
    IoTGpioInit(IOT_IO_NAME_GPIO_6);
    // 右电机GPIO9, GPIO10初始化
    IoTGpioInit(IOT_IO_NAME_GPIO_9);
    IoTGpioInit(IOT_IO_NAME_GPIO_10);

    // 设置GPIO5的管脚复用关系为PWM2输出
    IoSetFunc(IOT_IO_NAME_GPIO_5, IOT_IO_FUNC_GPIO_5_PWM2_OUT);
    // 设置GPIO6的管脚复用关系为PWM3输出
    IoSetFunc(IOT_IO_NAME_GPIO_6, IOT_IO_FUNC_GPIO_6_PWM3_OUT);
    // 设置GPIO9的管脚复用关系为PWM0输出
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_PWM0_OUT);
    // 设置GPIO10的管脚复用关系为PWM01输出
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_PWM1_OUT);

    // GPIO5方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_5, IOT_GPIO_DIR_OUT);
    // GPIO6方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_6, IOT_GPIO_DIR_OUT);
    // GPIO9方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);
    // GPIO10方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_10, IOT_GPIO_DIR_OUT);
    // 初始化PWM2
    IoTPwmInit(IOT_PWM_PORT_PWM2);
    // 初始化PWM3
    IoTPwmInit(IOT_PWM_PORT_PWM3);
    // 初始化PWM0
    IoTPwmInit(IOT_PWM_PORT_PWM0);
    // 初始化PWM1
    IoTPwmInit(IOT_PWM_PORT_PWM1);
}

// PWM取值：[1, 65535]，占空比[0-99]
void car_backward(void)
{
    car_stop();
    IoTPwmStart(IOT_PWM_PORT_PWM0, IOT_DUTY1, IOT_FREQ);
    IoTPwmStart(IOT_PWM_PORT_PWM3, IOT_DUTY1, IOT_FREQ);
}

void car_forward(void)
{
    car_stop();
    IoTPwmStart(IOT_PWM_PORT_PWM2, IOT_DUTY1, IOT_FREQ);
    IoTPwmStart(IOT_PWM_PORT_PWM1, IOT_DUTY1, IOT_FREQ);
}

void car_left(void)
{
    car_stop();
    IoTPwmStart(IOT_PWM_PORT_PWM1, IOT_DUTY2, IOT_FREQ);
}

void car_right(void)
{
    car_stop();
    IoTPwmStart(IOT_PWM_PORT_PWM2, IOT_DUTY2, IOT_FREQ);
}

void car_stop(void)
{
    IoTPwmStop(IOT_PWM_PORT_PWM0);
    IoTPwmStop(IOT_PWM_PORT_PWM1);
    IoTPwmStop(IOT_PWM_PORT_PWM2);
    IoTPwmStop(IOT_PWM_PORT_PWM3);
}