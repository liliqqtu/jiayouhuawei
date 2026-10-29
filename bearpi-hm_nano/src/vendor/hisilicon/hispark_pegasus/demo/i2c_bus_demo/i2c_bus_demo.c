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

#include "ssd1306_fonts.h"
#include "ssd1306.h"
#include "cw2015_control.h"
#include "gyro.h"
#include "hi_time.h"
#include "iot_watchdog.h"
#include "pca9555.h"
#include "nfc_control.h"

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio_ex.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "hi_errno.h"


int g_buttonState = 0;

void OnFuncKeyPressed(char *arg)
{
    (void) arg;
    g_buttonState = 1;
}

void PressToRestore(void)
{
    uint8_t ext_io_state = 0;
    IotGpioValue value = 0;
    uint8_t intLowFlag = 0;
    uint32_t cTick = 0;
    uint8_t status;
    status = IoTGpioGetInputVal(IOT_IO_NAME_GPIO_11, &value);
    if (status != IOT_SUCCESS) {
        printf("status = %d\r\n", status);
    }
    if (value == 1) {
        intLowFlag = 0;
    } else {
        if (intLowFlag == 0) {
            cTick = hi_get_milli_seconds();
            intLowFlag = 1;
        } else {
            if ((hi_get_milli_seconds() - cTick) > 2) { // 2ms
                status = PCA9555I2CReadByte(&ext_io_state);
                intLowFlag = 0;
            }
        }
    }
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
                printf("failed\r\n");
            }
            diff = ext_io_state ^ ext_io_state_d;
            if ((diff & 0x04) && ((ext_io_state & 0x04) == 0)) {
                    printf("S3 prassed\r\n");
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_5, IOT_GPIO_VALUE0);
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE0);
            } else if ((diff & 0x08) && ((ext_io_state & 0x08) == 0)) {
                    printf("S4 prassed\r\n");
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_5, IOT_GPIO_VALUE0);
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE1);
            } else if ((diff & 0x10) && ((ext_io_state & 0x10) == 0)) {
                    printf("S5 prassed\r\n");
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_5, IOT_GPIO_VALUE1);
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
                    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE0);
            }
            ext_io_state_d = ext_io_state;
            g_buttonState = 0;
        } else {
            PressToRestore();
        }
    }
}

// 电量检测模块Task
void CW2015Task(void)
{
    static char line[32] = {0}; // 32
    float voltage = 0.0;

    while (1) {
        voltage = GetVoltage();
        // 将获取到的电源格式化为字符串
        ssd1306_SetCursor(0, 15); // 15
        int ret = snprintf(line, sizeof(line), "voltage: %.2f", voltage);
        if (ret != 13) { // 13
            printf("failed\r\n");
        }
        ssd1306_DrawString(line, Font_7x10, White);
        ssd1306_UpdateScreen();
        TaskMsleep(10); // 10ms
    }
}

// NFC模块task
void NFCTask(void)
{
    NFCInit(); // 初始化NFC模块
}

void LSM6DSTask(void)
{
    GetLSM6DS();
}

// 功能按键task
void FunKeyTask(void)
{
    // 获取实时的按键状态
    GetFunKeyState();
}

void TrafficLightInit(void)
{
    // LED3的GPIO初始化
    IoTGpioInit(IOT_IO_NAME_GPIO_9);
    // 设置GPIO9的管脚复用关系为GPIO
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_GPIO);
    // GPIO方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);

    // LED5的GPIO初始化
    IoTGpioInit(IOT_IO_NAME_GPIO_10);
    // 设置GPIO9的管脚复用关系为GPIO
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_GPIO);
    // GPIO方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_10, IOT_GPIO_DIR_OUT);

    // LED4的GPIO初始化
    IoTGpioInit(IOT_IO_NAME_GPIO_5);
    // 设置GPIO5的管脚复用关系为GPIO
    IoSetFunc(IOT_IO_NAME_GPIO_5, IOT_IO_FUNC_GPIO_5_GPIO);
    // GPIO方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_5, IOT_GPIO_DIR_OUT);

    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_5, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE0);
}

void I2CBusInit(void)
{
    PCA9555Init(); // 初始化PCA9555 扩展IO芯片模块
    // 配置IO扩展芯片的part1的所有管脚为输出
    SetPCA9555GpioValue(PCA9555_PART1_IODIR, 0x00);
    // 配置左右三色车灯全灭
    SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, LED_OFF);
    CW2015Init();  // 电量检测模块
    LSM6DS_Init();  // 陀螺仪模块初始化
    ssd1306_Init(); // 初始化 SSD1306 OLED模块
    ssd1306_Fill(Black);
    TrafficLightInit(); // 交通灯初始化

    // 使能GPIO11的中断功能, OnFuncKeyPressed 为中断的回调函数
    IoTGpioRegisterIsrFunc(IOT_IO_NAME_GPIO_11, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnFuncKeyPressed, NULL);

    // S3:IO0_2,S4:IO0_3,S5:IO0_4 0001 1100 => 0x1c 将IO0_2,IO0_3,IO0_4方向设置为输入，1为输入，0位输出
    SetPCA9555GpioValue(PCA9555_PART0_IODIR, 0x1c);
}

void I2CBusExampleEntry(void)
{
    osThreadAttr_t attr;

    I2CBusInit();
    IoTWatchDogDisable();

    attr.name = "NFCTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 5*1024
    attr.priority = osPriorityNormal;

    // NFC
    if (osThreadNew((osThreadFunc_t)NFCTask, NULL, &attr) == NULL) {
        printf("[NFCTask] Failed to create NFCTask!\n");
    }

    // 电量监测
    attr.name = "CW2015Task";
    if (osThreadNew((osThreadFunc_t)CW2015Task, NULL, &attr) == NULL) {
        printf("[CW2015Task] Failed to create CW2015Task!\n");
    }

    // 功能按键
    attr.name = "FunKeyTask";
    if (osThreadNew((osThreadFunc_t)FunKeyTask, NULL, &attr) == NULL) {
        printf("[FunKeyTask] Failed to create FunKeyTask!\n");
    }

    // 陀螺仪
    attr.name = "LSM6DSTask";
    if (osThreadNew((osThreadFunc_t)LSM6DSTask, NULL, &attr) == NULL) {
        printf("[LSM6DSTask] Failed to create LSM6DSTask!\n");
    }
}

APP_FEATURE_INIT(I2CBusExampleEntry);
