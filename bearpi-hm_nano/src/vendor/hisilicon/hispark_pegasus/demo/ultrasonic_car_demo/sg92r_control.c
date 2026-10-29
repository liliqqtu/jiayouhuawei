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
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_gpio_ex.h"
#include "hi_time.h"
#include "sg92r_control.h"

#define  COUNT   10

void SetAngle(unsigned int duty)
{
    unsigned int time = 20000;

    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_2, IOT_GPIO_VALUE1);
    hi_udelay(duty);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_2, IOT_GPIO_VALUE0);
    hi_udelay(time - duty);
}

/* The steering gear is centered
1、依据角度与脉冲的关系，设置高电平时间为1500微秒
2、不断地发送信号，控制舵机居中
*/
void RegressMiddle(unsigned int a)
{
    unsigned int angle = a;
    for (int i = 0; i < COUNT; i++) {
        SetAngle(angle);
    }
}

/* Turn 90 degrees to the right of the steering gear
1、依据角度与脉冲的关系，设置高电平时间为500微秒
2、不断地发送信号，控制舵机向右旋转90度
*/
/*  Steering gear turn right */
void EngineTurnRight(unsigned int a)
{
    unsigned int angle = a;
    for (int i = 0; i < COUNT; i++) {
        SetAngle(angle);
    }
}

/* Turn 90 degrees to the left of the steering gear
1、依据角度与脉冲的关系，设置高电平时间为2500微秒
2、不断地发送信号，控制舵机向左旋转90度
*/
/* Steering gear turn left */
void EngineTurnLeft(unsigned int a)
{
    unsigned int angle = a;
    for (int i = 0; i < COUNT; i++) {
        SetAngle(angle);
    }
}

void S92RInit(void)
{
    // PWM舵机对应的GPIO是GPIO02
    // 初始化 GPIO02
    IoTGpioInit(IOT_IO_NAME_GPIO_2);
    // 设置GPIO02的管脚复用关系为GPIO
    IoSetFunc(IOT_IO_NAME_GPIO_2, IOT_IO_FUNC_GPIO_2_GPIO);
    // 设置GPIO02的方向为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_2, IOT_GPIO_DIR_OUT);
}
