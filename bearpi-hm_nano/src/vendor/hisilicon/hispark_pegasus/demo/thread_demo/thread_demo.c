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
#include "iot_pwm.h"
#include "iot_gpio.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_watchdog.h"

#define STACK_SIZE          1024
#define OS_DELAY            500000
#define IOT_PWM_PORT_PWM2   2       // GPIO5对应的是PWM2
                                    // GPIO5 corresponds to PWM2
#define PWM_CHANGE_TIMES    99      // PWM循环输出的次数
                                    // Number of PWM cycle outputs
#define PWM_FREQ            65535   // PWM分频倍数
                                    // PWM frequency division multiple
#define PWM_DELAY_10US      10      // PWM循环一次延迟的时间
                                    // Time of delay for one PWM cycle


void YellowLedControl(const char *arg)
{
    (void)arg;
    unsigned int i;

    while (1) {
        for (i = 0; i < PWM_CHANGE_TIMES; i++) {
            IoTPwmStart(IOT_PWM_PORT_PWM2, i, PWM_FREQ);
            usleep(PWM_DELAY_10US);
        }
        i = 0;
    }
}

void RedLedControl(const char *arg)
{
    (void)arg;
    while (1) {
        IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
        // 延时函数毫秒（设置高电平持续时间）
        // Delay function milliseconds (set high level duration)
        usleep(OS_DELAY);
        // 设置GPIO09输出低电平熄灭红色交通灯LED3
        // Set GPIO09 output low level to turn off red traffic light LED 3
        IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
        usleep(OS_DELAY);
    }
}

void LedGpioInit(void)
{
    IoTGpioInit(IOT_IO_NAME_GPIO_9);                                // RED LED的GPIO初始化
                                                                    // GPIO initialization of RED LED
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_GPIO);         // 设置GPIO9的管脚复用关系为GPIO
                                                                    // Set the pin reuse relationship of GPIO9 to GPIO
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);            // GPIO9方向设置为输出
                                                                    // GPIO9 direction set to output
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);       // 设置GPIO09输出低电平,熄灭红色交通灯LED
    // Set GPIO09 output low level and turn off the red traffic light LED
    IoTGpioInit(IOT_IO_NAME_GPIO_5);                                // 初始化GPIO5管脚
                                                                    // Initialize GPIO5 pin
    IoSetFunc(IOT_IO_NAME_GPIO_5, IOT_IO_FUNC_GPIO_5_PWM2_OUT);     // 设置GPIO5的管脚复用关系为PWM
    // Set the pin multiplexing relationship of GPIO5 to PWM
    IoTGpioSetDir(IOT_IO_NAME_GPIO_5, IOT_GPIO_DIR_OUT);            // 设置GPIO5的管脚方向为输出
                                                                    // Set the pin direction of GPIO5 as output
    IoTPwmInit(IOT_PWM_PORT_PWM2);                                  // 初始化GPIO5为PWM2
                                                                    // Initialize GPIO5 to PWM2
}

void ThreadExampleEntry(void)
{
    osThreadAttr_t attr;

    LedGpioInit();
    IoTWatchDogDisable();

    attr.name = "RedLedControl";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    // 创建一个线程，并注册一个回调函数RedLedControl，控制红色LED灯每隔1秒钟闪烁一次
    // Create a thread and register a callback function RedLedControl to
    // control the red LED to flash once every 1 second
    if (osThreadNew((osThreadFunc_t)RedLedControl, NULL, &attr) == NULL) {
        printf("[RedLedControl] osThreadNew Falied to create RedLedControl!\n");
    }

    // 创建第二个线程，注册回调函数为GreenLedControl,控制绿色LED灯，实现呼吸灯效果
    // Create the second thread, register the callback function as GreenLedControl,
    // control the green LED light, and achieve the effect of breathing light
    attr.name = "YellowLedControl";
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)YellowLedControl, NULL, &attr) == NULL) {
        printf("[YellowLedControl] osThreadNew Falied to create YellowLedControl!\n");
    }
}

APP_FEATURE_INIT(ThreadExampleEntry);
