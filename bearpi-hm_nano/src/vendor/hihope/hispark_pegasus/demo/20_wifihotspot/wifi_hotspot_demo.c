/*
 * Copyright (c) 2020, HiHope Community.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifi_hotspot.h"
#include "lwip/netifapi.h"

#define STACK_SIZE     10240
#define AP_SSID        "ABCD"
#define AP_SKEY        "123456789"
#define IDX_0          0
#define IDX_1          1
#define IDX_2          2
#define IDX_3          3
#define IDX_4          4
#define IDX_5          5
#define DELAY_TICKS_10     (10)
#define DELAY_TICKS_100    (100)
#define COUNTER            (60)
#define CH_NUM             (7)

static volatile int g_hotspotStarted = 0;

static void OnHotspotStateChanged(int state)
{
    printf("OnHotspotStateChanged: %d.\r\n", state);
    if (state == WIFI_HOTSPOT_ACTIVE) {
        g_hotspotStarted = 1;
    } else {
        g_hotspotStarted = 0;
    }
}

static volatile int g_joinedStations = 0;

static void PrintStationInfo(StationInfo* info)
{
    if (!info) return;
    static char macAddress[32] = {0};
    unsigned char* mac = info->macAddress;
    // int ret = snprintf(macAddress, sizeof(macAddress), "%02X:%02X:%02X:%02X:%02X:%02X",
    int ret = snprintf_s(macAddress, sizeof(macAddress), sizeof(macAddress) - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[IDX_0], mac[IDX_1], mac[IDX_2], mac[IDX_3], mac[IDX_4], mac[IDX_5]);
    if (ret < 0) {
        return;
    }
    printf(" PrintStationInfo: mac=%s, reason=%d.\r\n", macAddress, info->disconnectedReason);
}

static void OnHotspotStaJoin(StationInfo* info)
{
    g_joinedStations++;
    PrintStationInfo(info);
    printf("+OnHotspotStaJoin: active stations = %d.\r\n", g_joinedStations);
}

static void OnHotspotStaLeave(StationInfo* info)
{
    g_joinedStations--;
    PrintStationInfo(info);
    printf("-OnHotspotStaLeave: active stations = %d.\r\n", g_joinedStations);
}

WifiEvent g_defaultWifiEventListener = {
    .OnHotspotStaJoin = OnHotspotStaJoin,
    .OnHotspotStaLeave = OnHotspotStaLeave,
    .OnHotspotStateChanged = OnHotspotStateChanged,
};

static struct netif* g_iface = NULL;

int StartHotspot(const HotspotConfig* config)
{
    WifiErrorCode errCode = WIFI_SUCCESS;

    errCode = RegisterWifiEvent(&g_defaultWifiEventListener);
    printf("RegisterWifiEvent: %d\r\n", errCode);

    errCode = SetHotspotConfig(config);
    printf("SetHotspotConfig: %d\r\n", errCode);

    g_hotspotStarted = 0;
    errCode = EnableHotspot();
    printf("EnableHotspot: %d\r\n", errCode);

    while (!g_hotspotStarted) {
        osDelay(DELAY_TICKS_10);
    }
    printf("g_hotspotStarted = %d.\r\n", g_hotspotStarted);

    g_iface = netifapi_netif_find("ap0");
    if (g_iface) {
        ip4_addr_t ipaddr;
        ip4_addr_t gateway;
        ip4_addr_t netmask;

        IP4_ADDR(&ipaddr,  192, 168, 1, 1);     /* input your IP for example: 192 168 1 1 */
        IP4_ADDR(&gateway, 192, 168, 1, 1);     /* input your gateway for example: 192 168 1 1 */
        IP4_ADDR(&netmask, 255, 255, 255, 0);   /* input your netmask for example: 255 255 255 0 */
        err_t ret = netifapi_netif_set_addr(g_iface, &ipaddr, &netmask, &gateway);
        printf("netifapi_netif_set_addr: %d\r\n", ret);

        ret = netifapi_dhcps_stop(g_iface); // 海思扩展的HDCP服务接口
        printf("netifapi_dhcps_stop: %d\r\n", ret);

        ret = netifapi_dhcps_start(g_iface, 0, 0); // 海思扩展的HDCP服务接口
        printf("netifapi_dhcp_start: %d\r\n", ret);
    }
    return errCode;
}

void StopHotspot(void)
{
    if (g_iface) {
        err_t ret = netifapi_dhcps_stop(g_iface);  // 海思扩展的HDCP服务接口
        printf("netifapi_dhcps_stop: %d\r\n", ret);
    }

    WifiErrorCode errCode = UnRegisterWifiEvent(&g_defaultWifiEventListener);
    printf("UnRegisterWifiEvent: %d\r\n", errCode);

    errCode = DisableHotspot();
    printf("EnableHotspot: %d\r\n", errCode);
}


static void WifiHotspotTask(void)
{
    WifiErrorCode errCode;
    HotspotConfig config = {0};

    // strcpy(config.ssid, "ABCD");
    // strcpy(config.preSharedKey, "12345678");
    strcpy_s(config.ssid, WIFI_MAX_SSID_LEN, AP_SSID);
    strcpy_s(config.preSharedKey, WIFI_MAX_KEY_LEN, AP_SKEY);
    config.securityType = WIFI_SEC_TYPE_PSK;
    config.band = HOTSPOT_BAND_TYPE_2G;
    config.channelNum = CH_NUM;

    osDelay(DELAY_TICKS_10);

    printf("starting AP ...\r\n");
    errCode = StartHotspot(&config);
    printf("StartHotspot: %d\r\n", errCode);

    int timeout = COUNTER;
    while (timeout--) {
        printf("After %d seconds Ap will turn off!\r\n", timeout);
        osDelay(DELAY_TICKS_100);
    }

    printf("stop AP ...\r\n");
    StopHotspot();
    printf("stop AP ...\r\n");
}

static void WifiHotspotDemo(void)
{
    osThreadAttr_t attr;

    attr.name = "WifiHotspotTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(WifiHotspotTask, NULL, &attr) == NULL) {
        printf("[WifiHotspotDemo] Falied to create WifiHotspotTask!\n");
    }
}

APP_FEATURE_INIT(WifiHotspotDemo);
