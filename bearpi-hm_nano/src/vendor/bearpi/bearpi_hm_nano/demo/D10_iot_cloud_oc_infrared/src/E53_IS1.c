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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "iot_errno.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_pwm.h"
#include "E53_IS1.h"

/***************************************************************
 * 函数名称: E53IS1IoInit
 * 说    明: E53_SC2 GPIO初始化
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
static void E53IS1IoInit(void)
{
    IoTGpioInit(WIFI_IOT_IO_NAME_GPIO_8);                                      // 初始化GPIO
    IoTGpioSetFunc(WIFI_IOT_IO_NAME_GPIO_8, WIFI_IOT_IO_FUNC_GPIO_8_PWM1_OUT); // 设置GPIO_8引脚复用功能为PWM
    IoTGpioSetDir(WIFI_IOT_IO_NAME_GPIO_8, IOT_GPIO_DIR_OUT);                  // 设置GPIO_8引脚为输出模式
    IoTPwmInit(WIFI_IOT_PWM_PORT_PWM1);                  // 初始化PWM1端口

    IoTGpioInit(WIFI_IOT_IO_NAME_GPIO_7);
    IoTGpioSetFunc(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoTGpioSetDir(WIFI_IOT_IO_NAME_GPIO_7, IOT_GPIO_DIR_IN); // 设置GPIO_7为输入模式
    IoTGpioSetPull(WIFI_IOT_IO_NAME_GPIO_7, IOT_GPIO_PULL_UP);
}

/***************************************************************
 * 函数名称: E53IS1Init
 * 说    明: 初始化E53_IS1
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void E53IS1Init(void)
{
    E53IS1IoInit();
}
/***************************************************************
 * 函数名称: E53IS1ReadData
 * 说    明: 读取数据
 * 参    数: func 人体感应传感器回调函数
 * 返 回 值: 无
 ***************************************************************/
int E53IS1ReadData(E53IS1CallbackFunc func)
{
    uint32_t ret;
    ret = IoTGpioRegisterIsrFunc(WIFI_IOT_IO_NAME_GPIO_7, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_RISE_LEVEL_HIGH, func, NULL);
    if (ret != 0) {
        return -1;
    }
    return 0;
}
/***************************************************************
 * 函数名称: BeepStatusSet
 * 说    明: 蜂鸣器报警与否
 * 参    数: status,ENUM枚举的数据
 *									OFF,蜂鸣器
 *									ON,开蜂鸣器
 * 返 回 值: 无
 ***************************************************************/
void BeepStatusSet(E53IS1Status status)
{
    if (status == ON) {
        IoTPwmStart(WIFI_IOT_PWM_PORT_PWM1, PWM_DUTY, PWM_FREQ); // 输出PWM波
    }
    if (status == OFF) {
        IoTPwmStop(WIFI_IOT_PWM_PORT_PWM1);
    }
}
