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
#include "ohos_init.h"

#include "wifi_device.h"
#include "wifi_hotspot.h"
#include "wifi_error_code.h"
#include "lwip/netifapi.h"


#define AP_SSID "BearPi"
#define AP_PSK  "0987654321"

#define TASK_STACK_SIZE (1024 * 10)
#define TASK_PRIO 25
#define TASK_DELAY_2S 200
#define ONE_SECOND 1
#define DEF_TIMEOUT 15
#define CHANNEL_NUM 7
#define MAC_ADDRESS_LEN 32
#define MAC_DATA0 0
#define MAC_DATA1 1
#define MAC_DATA2 2
#define MAC_DATA3 3
#define MAC_DATA4 4
#define MAC_DATA5 5
#define UDP_SERVERPORT 8888

static void OnHotspotStaJoinHandler(StationInfo *info);
static void OnHotspotStateChangedHandler(int state);
static void OnHotspotStaLeaveHandler(StationInfo *info);

static struct netif *g_lwip_netif = NULL;
static int g_apEnableSuccess = 0;
static WifiEvent g_wifiEventHandler = { 0 };
WifiErrorCode error;


static void StartUdpServer(void)
{
    /****************以下为UDP服务器代码,默认IP:192.168.5.1***************/
    // 在sock_fd 进行监听
    int sock_fd;
    // 服务端地址信息
    struct sockaddr_in server_sock;

    // 创建socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket is error.\r\n");
        return -1;
    }

    bzero(&server_sock, sizeof(server_sock));
    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(UDP_SERVERPORT);

    // 调用bind函数绑定socket和地址
    if (bind(sock_fd, (struct sockaddr *)&server_sock, sizeof(struct sockaddr)) == -1) {
        perror("bind is error.\r\n");
        return -1;
    }

    int ret;
    char recvBuf[512] = { 0 };
    // 客户端地址信息
    struct sockaddr_in client_addr;
    int size_client_addr = sizeof(struct sockaddr_in);
    while (1) {
        printf("Waiting to receive data...\r\n");
        memset_s(recvBuf, sizeof(recvBuf), 0, sizeof(recvBuf));
        ret = recvfrom(sock_fd, recvBuf, sizeof(recvBuf), 0, (struct sockaddr*)&client_addr,
            (socklen_t*)&size_client_addr);
        if (ret < 0) {
            printf("UDP server receive failed!\r\n");
            return -1;
        }
        printf("receive %d bytes of data from ipaddr = %s, port = %d.\r\n", ret, inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));
        printf("data is %s\r\n", recvBuf);
        ret = sendto(sock_fd, recvBuf, strlen(recvBuf), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (ret < 0) {
            printf("UDP server send failed!\r\n");
            return -1;
        }
    }
    /*********************END********************/
}

static BOOL WifiAPTask(void)
{
    // 延时2S便于查看日志
    osDelay(TASK_DELAY_2S);

    // 注册wifi事件的回调函数
    g_wifiEventHandler.OnHotspotStaJoin = OnHotspotStaJoinHandler;
    g_wifiEventHandler.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
    g_wifiEventHandler.OnHotspotStateChanged = OnHotspotStateChangedHandler;
    error = RegisterWifiEvent(&g_wifiEventHandler);
    if (error != WIFI_SUCCESS) {
        printf("RegisterWifiEvent failed, error = %d.\r\n", error);
        return -1;
    }
    printf("RegisterWifiEvent succeed!\r\n");
    // 检查热点模式是否使能
    if (IsHotspotActive() == WIFI_HOTSPOT_ACTIVE) {
        printf("Wifi station is  actived.\r\n");
        return -1;
    }
    // 设置指定的热点配置
    HotspotConfig config = { 0 };

    strcpy_s(config.ssid, strlen(AP_SSID) + 1, AP_SSID);
    strcpy_s(config.preSharedKey, strlen(AP_PSK) + 1, AP_PSK);
    config.securityType = WIFI_SEC_TYPE_PSK;
    config.band = HOTSPOT_BAND_TYPE_2G;
    config.channelNum = CHANNEL_NUM;

    error = SetHotspotConfig(&config);
    if (error != WIFI_SUCCESS) {
        printf("SetHotspotConfig failed, error = %d.\r\n", error);
        return -1;
    }
    printf("SetHotspotConfig succeed!\r\n");

    // 启动wifi热点模式
    error = EnableHotspot();
    if (error != WIFI_SUCCESS) {
        printf("EnableHotspot failed, error = %d.\r\n", error);
        return -1;
    }
    printf("EnableHotspot succeed!\r\n");

    StartUdpServer();
}

static void OnHotspotStaJoinHandler(StationInfo *info)
{
    if (info == NULL) {
        printf("HotspotStaJoin:info is null.\r\n");
    } else {
        static char macAddress[MAC_ADDRESS_LEN] = { 0 };
        unsigned char *mac = info->macAddress;
        snprintf_s(macAddress, sizeof(macAddress), sizeof(macAddress) - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[MAC_DATA0], mac[MAC_DATA1], mac[MAC_DATA2], mac[MAC_DATA3], mac[MAC_DATA4], mac[MAC_DATA5]);
        printf("HotspotStaJoin: macAddress=%s, reason=%d.\r\n", macAddress, info->disconnectedReason);
        g_apEnableSuccess++;
    }
    return;
}

static void OnHotspotStaLeaveHandler(StationInfo *info)
{
    if (info == NULL) {
        printf("HotspotStaLeave:info is null.\r\n");
    } else {
        static char macAddress[MAC_ADDRESS_LEN] = { 0 };
        unsigned char *mac = info->macAddress;
        snprintf_s(macAddress, sizeof(macAddress), sizeof(macAddress) - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[MAC_DATA0], mac[MAC_DATA1], mac[MAC_DATA2], mac[MAC_DATA3], mac[MAC_DATA4], mac[MAC_DATA5]);
        printf("HotspotStaLeave: macAddress=%s, reason=%d.\r\n", macAddress, info->disconnectedReason);
        g_apEnableSuccess--;
    }
    return;
}

static void OnHotspotStateChangedHandler(int state)
{
    printf("HotspotStateChanged:state is %d.\r\n", state);
    if (state == WIFI_HOTSPOT_ACTIVE) {
        printf("wifi hotspot active.\r\n");
    } else {
        printf("wifi hotspot noactive.\r\n");
    }
}

static void Wifi_AP_Demo(void)
{
    osThreadAttr_t attr;

    attr.name = "WifiAPTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = TASK_STACK_SIZE;
    attr.priority = TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)WifiAPTask, NULL, &attr) == NULL) {
        printf("Falied to create WifiAPTask!\r\n");
    }
}

APP_FEATURE_INIT(Wifi_AP_Demo);
