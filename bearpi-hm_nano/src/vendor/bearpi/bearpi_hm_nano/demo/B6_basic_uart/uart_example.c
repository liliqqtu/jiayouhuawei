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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "iot_errno.h"
#include "iot_uart.h"
#include "ohos_init.h"

#define UART_TASK_STACK_SIZE (1024 * 8)
#define UART_TASK_PRIO 25
#define UART_BUFF_SIZE 1000
#define WIFI_IOT_UART_IDX_1 1
#define TASK_DELAY_1S 1000000

static const char *data = "Hello, BearPi!\r\n";

/**
 * @brief uart task send data through uart1 and receive data through uart1
 *
 */
static void UartTask(void)
{
    uint8_t uart_buff[UART_BUFF_SIZE] = { 0 };
    uint8_t *uart_buff_ptr = uart_buff;
    uint8_t ret;

    IotUartAttribute uart_attr = {

        // baud_rate: 9600
        .baudRate = 9600,

        // data_bits: 8bits
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    // Initialize uart driver
    ret = IoTUartInit(WIFI_IOT_UART_IDX_1, &uart_attr);
    if (ret != IOT_SUCCESS) {
        printf("Failed to init uart! Err code = %d\n", ret);
        return;
    }

    while (1) {
        printf("=======================================\r\n");
        printf("*************UART_example**************\r\n");
        printf("=======================================\r\n");

        // send data through uart1
        IoTUartWrite(WIFI_IOT_UART_IDX_1, (unsigned char *)data, strlen(data));

        // receive data through uart1
        IoTUartRead(WIFI_IOT_UART_IDX_1, uart_buff_ptr, UART_BUFF_SIZE);

        printf("Uart1 read data:%s\n", uart_buff_ptr);
        usleep(TASK_DELAY_1S);
    }
}

/**
 * @brief Main Entry of the UART Example
 *
 */
static void UartExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "UartTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = UART_TASK_STACK_SIZE;
    attr.priority = UART_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)UartTask, NULL, &attr) == NULL) {
        printf("Failed to create UartTask!\n");
    }
}

APP_FEATURE_INIT(UartExampleEntry);