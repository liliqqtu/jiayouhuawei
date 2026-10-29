/*
 * Copyright (C) 2021 HiHope Open Source Organization .
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
 *
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_uart.h"

#define IOT_UART_IDX_2  (2)
#define STACK_SIZE   (1024)
#define DELAY_US     (100000)
#define IOT_GPIO_11  (11)
#define IOT_GPIO_12  (12)

unsigned char uartWriteBuff[] = "hello uart!";
unsigned char uartReadBuff[2048] = {0};
int usr_uart_config(void)
{
    // 初始化UART配置，115200，数据bit为8, 停止位1，奇偶校验为NONE，流控为NONE
    IotUartAttribute g_uart_cfg = {115200, 8, 1, IOT_UART_PARITY_NONE, 500, 500, 0};
    int ret = IoTUartInit(IOT_UART_IDX_2, &g_uart_cfg);
    if (ret != 0) {
        printf("uart init fail\r\n");
    }

    return ret;
}

// 1.任务处理函数
static void* UartDemo_Task(void)
{
    static int count = 1000;
    printf("[UartDemo] UartDemo_Task()\n");

    IoTGpioInit(IOT_GPIO_11);  // 使用GPIO，都需要调用该接口
    IoTGpioInit(IOT_GPIO_12);  // 使用GPIO，都需要调用该接口

    printf("UART init...\r\n");
    usr_uart_config();

    int ret = IoTUartWrite(IOT_UART_IDX_2, (unsigned char *)uartWriteBuff, sizeof(uartWriteBuff));
    printf("Uart Write data: ret = %d\r\n", ret);
    while (count--) {
        unsigned int len = IoTUartRead(IOT_UART_IDX_2, uartReadBuff, sizeof(uartReadBuff));
        if (len > 0) {
            printf("Uart read data:%s\r\n", uartReadBuff);
        }
        usleep(DELAY_US);
    }

    return NULL;
}

// 2.任务入口函数
static void UartDemo_Entry(void)
{
    osThreadAttr_t attr = {0};

    printf("[UartDemo] UartDemo_Entry()\n");

    attr.name = "UartDemo_Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;      // 堆栈大小
    attr.priority = osPriorityNormal;  // 优先级

    if (osThreadNew((osThreadFunc_t)UartDemo_Task, NULL, &attr) == NULL) {
        printf("[UartDemo] Falied to create UartDemo_Task!\n");
    }
}

// 3.注册模块
SYS_RUN(UartDemo_Entry);