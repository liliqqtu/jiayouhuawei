/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_uart.h"

#include "hal_bsp_ssd1306.h"
#include "hal_bsp_nfc.h"
#include "hal_bsp_nfc_to_wifi.h"
#include "hal_bsp_wifi.h"
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_aw2013.h"

#include "sys_config.h"
#include "oled_show_log.h"
#include "oled_show_task.h"
#include "udp_send_task.h"
#include "udp_recv_task.h"
#include "uart_recv_task.h"

#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/api_shell.h"

osThreadId_t oled_show_task_id;
osThreadId_t uart_recv_task_id;
osThreadId_t udp_send_task_id;
osThreadId_t udp_recv_task_id;
tn_pcf8574_io_t pcf8574_io;

system_value_t systemValue = {0}; // 系统全局变量

#define TASK_STACK_SIZE (1024 * 5)

/**
 * @brief  串口初始化
 * @note   与STM32单片机之间的串口通信
 * @retval None
 */
void uart_init(void)
{
    uint32_t ret = 0;
    // 初始化串口
    hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_UART2_TXD);
    hi_io_set_func(HI_IO_NAME_GPIO_12, HI_IO_FUNC_GPIO_12_UART2_RXD);

    hi_uart_attribute uart_param = {
        .baud_rate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = 0,
    };
    ret = hi_uart_init(HI_UART_IDX_2, &uart_param, NULL);
    if (ret != HI_ERR_SUCCESS) {
        printf("hi uart init is faild.\r\n");
    }
}

int nfc_connect_wifi_init(void)
{
    /********************************* NFC碰一碰联网 *********************************/
    uint8_t ndefLen = 0;      // ndef包的长度
    uint8_t ndef_Header = 0;  // ndef消息开始标志位-用不到
    uint32_t result_code = 0; // 函数的返回值
    // 读整个数据的包头部分，读出整个数据的长度
    if (result_code = NT3HReadHeaderNfc(&ndefLen, &ndef_Header) != true) {
        printf("NT3HReadHeaderNfc is failed. result_code = %d\r\n", result_code);
        return -1;
    }
    ndefLen += NDEF_HEADER_SIZE; // 加上头部字节
    if (ndefLen <= NDEF_HEADER_SIZE) {
        printf("ndefLen <= 2\r\n");
        return -1;
    }
    
    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("ndefBuff malloc is Falied!\r\n");
        return -1;
    }

    if (result_code = get_NDEFDataPackage(ndefBuff, ndefLen) != HI_ERR_SUCCESS) {
        printf("get_NDEFDataPackage is failed. result_code = %d\r\n", result_code);
        return -1;
    }

    printf("start print ndefBuff.\r\n");
    for (size_t i = 0; i < ndefLen; i++) {
        printf("0x%x ", ndefBuff[i]);
    }
    printf("\n");

    while (NFC_configuresWiFiNetwork(ndefBuff) != WIFI_SUCCESS) {
        printf("nfc connect wifi is failed!\r\n");
        oled_consle_log("wifi no.");
        sleep(1);
        SSD1306_CLS(); // 清屏
    }
    oled_consle_log("wifi yes.");
    return 0;
}

int udp_init(void)
{
    uint32_t result_code = 0; // 函数的返回值
    /********************************** 创建UDP服务端 **********************************/
    printf("wifi IP: %s", WiFi_GetLocalIP());
    // 创建socket
    if ((systemValue.udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        printf("create socket failed!\r\n");
        return -1;
    }

    // 命名套接字
    struct sockaddr_in local;
    local.sin_family = AF_INET;                           // IPV4
    local.sin_port = htons(UDP_PORT);                     // 端口号
    local.sin_addr.s_addr = inet_addr(WiFi_GetLocalIP()); // 使用本地IP地址进行创建UDP服务端

    // 绑定端口号
    result_code = bind(systemValue.udp_socket_fd, (const struct sockaddr *)&local, sizeof(local));
    if (result_code < 0) {
        printf("udp server bind IP is failed.\r\n");
        return -1;
    } else {
        printf("udp server bind IP is success.");
    }

    SSD1306_CLS(); // 清屏
    return 0;
}
void peripheral_init(void)
{
    /********************************** 外设初始化 **********************************/
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    nfc_Init();     // NFC 初始化
    // 外设的初始化
    PCF8574_Init();
    AW2013_Init(); // 三色LED灯的初始化
    AW2013_Control_Red(0);
    AW2013_Control_Green(0);
    AW2013_Control_Blue(0);
    uart_init(); // 串口2初始化
}
/**
 * @brief  智能小车的入口函数
 * @note
 * @retval None
 */
static void smartCar_example(void)
{
    peripheral_init();
    if (nfc_connect_wifi_init() == -1) {
        return ;
    }
    if (udp_init() == -1) {
        return ;
    }
    /********************************** 创建线程 **********************************/
    osThreadAttr_t options;
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;

    /********************************** UART接收任务 **********************************/
    options.name = "uart_recv_task";
    options.priority = osPriorityNormal1;
    uart_recv_task_id = osThreadNew((osThreadFunc_t)uart_recv_task, NULL, &options);
    if (uart_recv_task_id != NULL) {
        printf("ID = %d, Create uart_recv_task_id is OK!\r\n", uart_recv_task_id);
    }

    /********************************** OLED显示任务 **********************************/
    options.name = "oled_show_task";
    options.priority = osPriorityNormal;
    oled_show_task_id = osThreadNew((osThreadFunc_t)oled_show_task, NULL, &options);
    if (oled_show_task_id != NULL) {
        printf("ID = %d, Create oled_show_task_id is OK!\r\n", oled_show_task_id);
    }

    /********************************** UDP发送任务 **********************************/
    options.name = "udp_send_task";
    options.priority = osPriorityNormal;
    udp_send_task_id = osThreadNew((osThreadFunc_t)udp_send_task, NULL, &options);
    if (udp_send_task_id != NULL) {
        printf("ID = %d, Create udp_send_task_id is OK!\r\n", udp_send_task_id);
    }

    /********************************** UDP接收任务 **********************************/
    options.name = "udp_recv_task";
    options.priority = osPriorityNormal1;
    udp_recv_task_id = osThreadNew((osThreadFunc_t)udp_recv_task, NULL, &options);
    if (udp_recv_task_id != NULL) {
        printf("ID = %d, Create udp_recv_task_id is OK!\r\n", udp_recv_task_id);
    }
}
SYS_RUN(smartCar_example);
