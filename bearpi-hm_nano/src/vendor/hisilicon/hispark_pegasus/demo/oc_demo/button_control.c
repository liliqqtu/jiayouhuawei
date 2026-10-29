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
#include "hi_timer.h"

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio_ex.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "hi_errno.h"
#include "pca9555.h"
#include "app_demo_iot.h"

static volatile int g_buttonState = 0;

void OnFuncKeyPressed(char *arg)
{
    (void) arg;
    g_buttonState = 1;
}

void FuncKeyInit(void)
{
    // 使能GPIO11的中断功能, OnFuncKeyPressed 为中断的回调函数
    IoTGpioRegisterIsrFunc(IOT_IO_NAME_GPIO_11, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnFuncKeyPressed, NULL);

    // S3:IO0_2,S4:IO0_3,S5:IO0_4 0001 1100 => 0x1c 将IO0_2,IO0_3,IO0_4方向设置为输入，1为输入，0位输出
    SetPCA9555GpioValue(PCA9555_PART0_IODIR, 0x1c);
}

void GetFunKeyState(void)
{
    uint8_t ext_io_state = 0;
    uint8_t ext_io_state_d = 0;
    uint8_t status;

    while (1) {
        if (g_buttonState == 1) {
            uint8_t diff;
            status = PCA9555I2CReadByte(&ext_io_state);
            if (status != IOT_SUCCESS) {
                printf("i2c error!\r\n");
                ext_io_state = 0;
                ext_io_state_d = 0;
                g_buttonState = 0;
                continue;
            }

            diff = ext_io_state ^ ext_io_state_d;
            if (diff == 0) {
                printf("diff = 0! state:%0X, %0X\r\n", ext_io_state, ext_io_state_d);
            }
            if ((diff & 0x04) && ((ext_io_state & 0x04) == 0)) {
                printf("button1 pressed,\r\n");
                RedLight();
            } else if ((diff & 0x08) && ((ext_io_state & 0x08) == 0)) {
                printf("button2 pressed \r\n");
                RedOff();
            } else if ((diff & 0x10) && ((ext_io_state & 0x10) == 0)) {
                printf("button3 pressed \r\n");
            }
            status = PCA9555I2CReadByte(&ext_io_state);
            ext_io_state_d = ext_io_state;
            g_buttonState = 0;
        }
        usleep(20); // 20us
    }
}

static void ButtonControl(void)
{
    printf("ButtonControl\r\n");
    // IO扩展芯片初始化
    PCA9555Init();
    // 配置IO扩展芯片的part1的所有管脚为输出
    SetPCA9555GpioValue(PCA9555_PART1_IODIR, 0x00);
    // 配置左右三色车灯全灭
    SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, LED_OFF);

    // 按键中断初始化
    FuncKeyInit();

    // 获取实时的按键状态
    GetFunKeyState();
}

static void ButtonControlEntry(void)
{
    osThreadAttr_t attr;
    attr.name = "LedCntrolDemo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024; /* 堆栈大小为1024 */
    attr.priority = osPriorityNormal;
    // 报错
    if (osThreadNew((osThreadFunc_t)ButtonControl, NULL, &attr) == NULL) {
        printf("[LedExample] Failed to create LedTask!\n");
    }
}

APP_FEATURE_INIT(ButtonControlEntry);