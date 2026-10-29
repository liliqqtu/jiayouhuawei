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
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hal_bsp_pcf8574.h"
#include "hal_bsp_wifi.h"

#include "wifi_device.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/api_shell.h"

osThreadId_t Task1_ID; // 任务1设置为低优先级任务
#define TASK_STACK_SIZE (1024 * 10)
#define UDP_SERVER_IP "192.168.3.209"
#define UDP_SERVER_PORT 6789

void Task1(void)
{
    int socket_fd = 0;
    int result;

    // 服务器的地址信息
    struct sockaddr_in send_addr;
    socklen_t addr_length = sizeof(send_addr);
    char recvBuf[512];

    // 连接Wifi
    WiFi_connectHotspots("AI_DEV", "HQYJ12345678");

    // 创建socket
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        printf("create socket failed!\r\n");
        return;
    }

    // 初始化预连接的服务端地址
    send_addr.sin_family = AF_INET;                        // IPV4
    send_addr.sin_port = htons(UDP_SERVER_PORT);                      // 远端服务器的端口号
    send_addr.sin_addr.s_addr = inet_addr(UDP_SERVER_IP); // 远端服务器的IP地址
    addr_length = sizeof(send_addr);

    // 发送数据到服务远端
    result = sendto(socket_fd, "hello farsight\r\n", strlen("hello farsight\r\n"), 0,
                    (struct sockaddr *)&send_addr, addr_length);
    if (result) {
        printf("result: %d, sendData:%s\r\n", result, "hello farsight");
    }
    while (1) {
        memset_s(recvBuf, sizeof(recvBuf), 0, sizeof(recvBuf));

        // 接收服务端返回的字符串
        result = recvfrom(socket_fd, recvBuf, sizeof(recvBuf), 0, (struct sockaddr *)&send_addr, &addr_length);
        if (result > 0) {
            // 发送数据到服务远端
            result = sendto(socket_fd, recvBuf, sizeof(recvBuf), 0, (struct sockaddr *)&send_addr, addr_length);
            if (result) {
                printf("result: %d, sendData:%s\r\n", result, recvBuf);
            }
        }
        sleep(1);
    }

    // 关闭这个 socket
    closesocket(socket_fd);
}
static void network_wifi_udp_example(void)
{
    printf("Enter network_wifi_udp_example()!\r\n");
    PCF8574_Init();

    osThreadAttr_t options;
    options.name = "thread_1";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
}
SYS_RUN(network_wifi_udp_example);