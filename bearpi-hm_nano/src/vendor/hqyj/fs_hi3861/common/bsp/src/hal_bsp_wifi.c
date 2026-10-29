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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "wifi_device.h"
#include "wifi_hotspot.h"
#include "lwip/netifapi.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/api_shell.h"
#include "hal_bsp_wifi.h"

#define DEF_TIMEOUT 15
static int g_ConnectSuccess = 0;
#define SELECT_WLAN_PORT "wlan0"
static struct netif *g_lwip_netif = NULL;
static char g_IP_Addr[20] = {0}; // 连接wifi热点之后，获取到的IP地址

/**
 * @brief  获取连接WiFi后的本地IP地址
 * @note
 * @retval IP地址-字符串
 */
char *WiFi_GetLocalIP(void)
{
    return g_IP_Addr;
}
/**
 * 获取WiFi的IP地址
 **/
void Sta_GetWiFiIP(struct netif *netif, char *ip)
{
    int ret;
    if (netif == NULL) {
        return;
    }

    ip4_addr_t ipAddr;
    ip4_addr_t netMask;
    ip4_addr_t gateWay;

    ret = netifapi_netif_get_addr(netif, &ipAddr, &netMask, &gateWay);
    if (ret == 0) {
        inet_ntop(AF_INET, &ipAddr, ip, INET_ADDRSTRLEN);
    }
}

#define WIFI_CHANNE 5 // WiFi通道

WifiErrorCode WiFi_createHotspots(const char *ssid, const char *psk)
{
    WifiErrorCode result;
    printf("Start initialization of WiFi hotspots\r\n");

    // 使能WiFi
    result = EnableWifi();
    if (result != WIFI_SUCCESS) {
        printf("Enable WiFi failed. result: %d\r\n", result);
        return result;
    }
    // 判断WiFi是否激活
    result = IsWifiActive();
    if (result != WIFI_STA_ACTIVE) {
        printf("WiFi activation failed. result: %d\r\n", result);
        return result;
    }

    // 设置指定的热点信息
    HotspotConfig hotspotConfig = {0};
    strcpy_s(hotspotConfig.ssid, strlen(ssid) + 1, ssid);
    strcpy_s(hotspotConfig.preSharedKey, strlen(psk) + 1, psk);
    hotspotConfig.securityType = WIFI_SEC_TYPE_PSK;
    hotspotConfig.band = HOTSPOT_BAND_TYPE_2G;
    hotspotConfig.channelNum = WIFI_CHANNE;
    result = SetHotspotConfig(&hotspotConfig);
    if (result != WIFI_SUCCESS) {
        printf("Failed to set WiFi hotspot information. result: %d\r\n", result);
        return result;
    }

    // 开启WiFi热点模式
    result = EnableHotspot();
    if (result != WIFI_SUCCESS) {
        printf("Failed to enable wifi hotspot mode. result: %d\r\n", result);
        return result;
    }

    // 检查WiFi热点是否激活
    result = IsHotspotActive();
    if (result != WIFI_HOTSPOT_ACTIVE) {
        printf("WiFi hotspot activation failed. result: %d\r\n", result);
        return result;
    }

    printf("WiFi hotspot initialized successfully\r\n");

    return WIFI_SUCCESS;
}

// 连接WiFi热点时的状态发生改变的回调函数
static void ConnectionWifiChangedHandler(int state, WifiLinkedInfo *info)
{
    if (info == NULL) {
        printf("WifiConnectionChanged:info is null.\r\n");
    } else {
        if (state == WIFI_STATE_AVALIABLE) {
            g_ConnectSuccess = 1;
        } else {
            g_ConnectSuccess = 0;
        }
    }
}
// 等待连接热点 默认15s的超时时间
static int WaitConnectResult(void)
{
    int ConnectTimeout = DEF_TIMEOUT;
    while (ConnectTimeout > 0) {
        sleep(1);
        ConnectTimeout--;
        printf(".");
        if (g_ConnectSuccess == 1) {
            printf("WaitConnectResult:wait success[%d]s.\r\n", (DEF_TIMEOUT - ConnectTimeout));
            break;
        }
    }
    if (ConnectTimeout <= 0) {
        printf("WaitConnectResult:timeout!.\r\n");
        return 0;
    }

    return 1;
}
WifiErrorCode WiFi_connectHotspots(const char *ssid, const char *psk)
{
    WifiErrorCode result;
    int Timeout = 10; // 超时时间 10s

    printf("Start Connect of WiFi hotspots.\r\n");

    // 使能WiFi
    result = EnableWifi();
    if (result != WIFI_SUCCESS) {
        printf("Enable WiFi failed.\r\n");
        return result;
    }
    // 判断WiFi是否激活
    result = IsWifiActive();
    if (result != WIFI_STA_ACTIVE) {
        printf("WiFi activation failed.\r\n");
        return result;
    }
    // 注册wifi的回调函数
    WifiEvent eventConfig = {0};
    eventConfig.OnWifiConnectionChanged = ConnectionWifiChangedHandler; // WiFi连接的状态改变
    result = RegisterWifiEvent(&eventConfig);
    if (result != WIFI_SUCCESS) {
        printf("Failed to register WiFi callback function.\r\n");
        return result;
    }

    // 连接指定的WiFi热点
    WifiDeviceConfig wifiDeviceConfig = {0};
    int wifiResult = 0;
    strcpy_s(wifiDeviceConfig.ssid, strlen(ssid) + 1, ssid);               // 连接WiFi的名称
    strcpy_s(wifiDeviceConfig.preSharedKey, strlen(psk) + 1, psk);        // WiFi的密码
    wifiDeviceConfig.securityType = WIFI_SEC_TYPE_PSK; // WiFi的安全性
    result = AddDeviceConfig(&wifiDeviceConfig, &wifiResult);
    if ((result == WIFI_SUCCESS) && (ConnectTo(wifiResult) == WIFI_SUCCESS) && (WaitConnectResult() == 1)) {
        printf("wifi connect succeed!.\r\n");
        g_lwip_netif = netifapi_netif_find(SELECT_WLAN_PORT);
        // 启动DHCP
        if (g_lwip_netif) {
            dhcp_start(g_lwip_netif);
        }

        // 等待DHCP
        for (;;) {
            if (dhcp_is_bound(g_lwip_netif) == ERR_OK) {
                Sta_GetWiFiIP(g_lwip_netif, g_IP_Addr);
                printf("connect wifi IP addr: %s.\r\n", g_IP_Addr);
                break;
            }
            printf("#");
            Timeout--;
            if (Timeout == 0) {
                // 超时
                return ERROR_WIFI_BUSY;
            }
            sleep(1);
        }
    } else {
        return ERROR_WIFI_BUSY;
    }
    return WIFI_SUCCESS;
}
