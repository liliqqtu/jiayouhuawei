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
#define TCP_SERVER_IP "192.168.3.209"
#define TCP_SERVER_PORT 6789

// 在sock_fd 进行监听，在 new_fd 接收新的链接
void Task1(void)
{
    int socket_fd = 0;
    char buff[256];
    int re = 0;

    // 连接Wifi
    WiFi_connectHotspots("AI_DEV", "HQYJ12345678");
    socket_fd = socket(AF_INET, SOCK_STREAM, 0); // 创建套接字（TCP）
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(TCP_SERVER_IP); // 填写服务器的IP地址

    re = connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)); // 连接服务器
    if (re == -1) {
        printf("Failed to connect to the server\r\n");
        return;
    }
    printf("Connection to server successful\r\n");

    // 发送第一条数据
    send(socket_fd, "Connection to server successful.", strlen("Connection to server successful."), 0);

    while (1) {
        memset_s(buff, sizeof(buff), 0, sizeof(buff));
        re = recv(socket_fd, buff, sizeof(buff), 0); //	接收客户端发送过来的消息
        if (re <= 0) {
            break;
        } else {
            printf("Receive data received by the server: %s\r\n", buff);
            send(socket_fd, buff, sizeof(buff), 0);
        }
    }

    close(socket_fd);
}
static void network_wifi_tcp_example(void)
{
    printf("Enter network_wifi_tcp_example()!\r\n");
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
SYS_RUN(network_wifi_tcp_example);