/*
 * 河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
 * 独居老人居家安全监测与应急辅助系统
 * 无线网络传输管理与边缘离线缓存对账服务
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "wifi_device.h"
#include "lwip/netifapi.h"
#include "lwip/api_shell.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "elderly_care_config.h"
#include "network_service.h"

#define NET_TASK_STACK_SIZE     (1024 * 8)
#define NET_TASK_PRIORITY       osPriorityNormal

static volatile int g_wifi_connected = 0;
static volatile int g_network_ready = 0;
static struct netif *g_iface = NULL;

// 边缘计算：离线断网 RAM 环形缓存队列
static char g_cache[CFG_CACHE_DEPTH][CFG_CACHE_ITEM_SIZE];
static int  g_cache_count = 0;
static osMutexId_t g_cache_mutex = NULL;

static void OnWifiConnectionChanged(int state, WifiLinkedInfo *info)
{
    (void)info;
    if (state == WIFI_STATE_AVALIABLE) {
        g_wifi_connected = 1;
        printf("[WIFI_EVENT] State -> WIFI_STATE_AVALIABLE (Connected to AP!)\n");
    } else {
        g_wifi_connected = 0;
        g_network_ready = 0;
        printf("[WIFI_EVENT] State -> Disconnected (state=%d)\n", state);
    }
}

static void OnWifiScanStateChanged(int state, int size)
{
    (void)state;
    (void)size;
}

static WifiEvent g_wifi_event = {
    .OnWifiConnectionChanged = OnWifiConnectionChanged,
    .OnWifiScanStateChanged = OnWifiScanStateChanged,
    .OnHotspotStateChanged = NULL,
    .OnHotspotStaJoin = NULL,
    .OnHotspotStaLeave = NULL
};

// 发送单个 UDP 数据包到网关（单播指定 IP + 广播 255.255.255.255 双重保活）
static int SendUdpPacket(const char *data)
{
    if (data == NULL || strlen(data) == 0) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[UDP] socket creation failed\n");
        return -1;
    }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    int len = (int)strlen(data);

    // 1. 单播给配置的网关目标 IP
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(CFG_UDP_PORT);
    dest.sin_addr.s_addr = inet_addr(CFG_GATEWAY_IP);
    int sent = sendto(sock, data, len, 0, (struct sockaddr *)&dest, sizeof(dest));

    // 2. 局域网全网受限广播（防电脑换热点后 IP 漂移）
    struct sockaddr_in bcast;
    memset(&bcast, 0, sizeof(bcast));
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(CFG_UDP_PORT);
    bcast.sin_addr.s_addr = inet_addr("255.255.255.255");
    sendto(sock, data, len, 0, (struct sockaddr *)&bcast, sizeof(bcast));

    lwip_close(sock);

    if (sent != len) {
        printf("[UDP] sendto error (sent=%d, total=%d)\n", sent, len);
        return -1;
    }
    return 0;
}

// 离线缓存入队 (环形覆盖最旧)
void NetworkServicePushCache(const char *json_data)
{
    if (json_data == NULL || strlen(json_data) == 0) {
        return;
    }

    if (g_cache_mutex != NULL) {
        osMutexAcquire(g_cache_mutex, osWaitForever);
    }

    if (g_cache_count >= CFG_CACHE_DEPTH) {
        // 覆盖最早的一条，整体前移
        memmove(g_cache[0], g_cache[1], (CFG_CACHE_DEPTH - 1) * CFG_CACHE_ITEM_SIZE);
        g_cache_count = CFG_CACHE_DEPTH - 1;
    }

    strncpy(g_cache[g_cache_count], json_data, CFG_CACHE_ITEM_SIZE - 1);
    g_cache[g_cache_count][CFG_CACHE_ITEM_SIZE - 1] = '\0';
    g_cache_count++;

    printf("[NET_CACHE] Offline cached frame #%d (total backlog: %d)\n", g_cache_count, g_cache_count);

    if (g_cache_mutex != NULL) {
        osMutexRelease(g_cache_mutex);
    }
}

// 断网恢复后补传
int NetworkServiceFlushCache(void)
{
    if (!g_network_ready || g_cache_count == 0) {
        return 0;
    }

    if (g_cache_mutex != NULL) {
        osMutexAcquire(g_cache_mutex, osWaitForever);
    }

    int flushed = 0;
    int i = 0;
    while (i < g_cache_count) {
        int ret = SendUdpPacket(g_cache[i]);
        if (ret == 0) {
            flushed++;
            i++;
            osDelay(2); // 20ms 错峰避免网络拥塞
        } else {
            break; // 仍有网络波动，留待下一周期补发
        }
    }

    if (i > 0) {
        if (i < g_cache_count) {
            memmove(g_cache[0], g_cache[i], (g_cache_count - i) * CFG_CACHE_ITEM_SIZE);
        }
        g_cache_count -= i;
        printf("[NET_CACHE] Backfilled %d cached frames! Backlog remaining: %d\n", flushed, g_cache_count);
    }

    if (g_cache_mutex != NULL) {
        osMutexRelease(g_cache_mutex);
    }

    return flushed;
}

int NetworkServiceGetCacheCount(void)
{
    return g_cache_count;
}

bool NetworkServiceIsReady(void)
{
    return (g_network_ready != 0);
}

int NetworkServiceSendTelemetry(const char *json_data)
{
    if (json_data == NULL) {
        return -1;
    }

    if (!g_network_ready) {
        NetworkServicePushCache(json_data);
        return 0;
    }

    int ret = SendUdpPacket(json_data);
    if (ret != 0) {
        // UDP 发送异常，自动转存入离线缓存
        NetworkServicePushCache(json_data);
        return -1;
    }
    return 0;
}

// 后台 Wi-Fi 网络守护与重连任务
static void NetworkTask(void *arg)
{
    (void)arg;
    osDelay(100); // 开机延时 1 秒，等待主控与传感器就绪

    printf("\n======================================================\n");
    printf("   [NETWORK] Independent Power & Wi-Fi Service Init   \n");
    printf("   Target SSID: %s\n", CFG_WIFI_SSID);
    printf("   Target Gateway: %s:%d (UDP Mode)\n", CFG_GATEWAY_IP, CFG_UDP_PORT);
    printf("======================================================\n\n");

    RegisterWifiEvent(&g_wifi_event);
    EnableWifi();
    osDelay(30); // 等待 300ms 确保底层 Wi-Fi 芯片就绪

    WifiDeviceConfig apConfig;
    memset(&apConfig, 0, sizeof(apConfig));
    strncpy(apConfig.ssid, CFG_WIFI_SSID, sizeof(apConfig.ssid) - 1);
    strncpy(apConfig.preSharedKey, CFG_WIFI_PASSWORD, sizeof(apConfig.preSharedKey) - 1);
    apConfig.securityType = WIFI_SEC_TYPE_PSK;

    int netId = -1;
    WifiErrorCode addErr = AddDeviceConfig(&apConfig, &netId);
    printf("[NETWORK] AddDeviceConfig ret=%d, netId=%d\n", addErr, netId);

    while (1) {
        if (!g_wifi_connected) {
            g_network_ready = 0;
            if (netId < 0) {
                AddDeviceConfig(&apConfig, &netId);
                printf("[NETWORK] Re-adding AP config, netId=%d\n", netId);
            }
            printf("[NETWORK] Attempting to connect to Wi-Fi AP '%s' (netId=%d)...\n", CFG_WIFI_SSID, netId);
            ConnectTo(netId);

            // 等待 AP 连接握手（最多等待 10 秒）
            for (int i = 0; i < 100 && !g_wifi_connected; i++) {
                osDelay(10); // 100ms
            }

            if (g_wifi_connected) {
                printf("[NETWORK] AP Associated! Requesting DHCP IP address...\n");
                g_iface = netifapi_netif_find("wlan0");
                if (g_iface != NULL) {
                    netifapi_dhcp_start(g_iface);
                    osDelay(150); // 等待 1.5 秒完成 DHCP 分配
                    netifapi_netif_common(g_iface, dhcp_clients_info_show, NULL);

                    printf("[NETWORK] DHCP Success! Board IP: %s\n", ip4addr_ntoa(netif_ip4_addr(g_iface)));
                    printf("[NETWORK] Ready for Untethered Independent Power Operation!\n");
                    g_network_ready = 1;

                    // 补传断网期间积压的传感器数据
                    NetworkServiceFlushCache();
                }
            } else {
                printf("[NETWORK] Wi-Fi AP connect timeout. Running in Local Offline Edge mode.\n");
                printf("[NETWORK] (Local safety & alarms 100%% active. Next Wi-Fi retry in 8s)\n");
                osDelay(800); // 离线等待 8 秒再尝试重连，避免频繁消耗 CPU
                continue;
            }
        } else {
            // 在线状态：若有积压缓存，尝试补发
            if (g_network_ready && g_cache_count > 0) {
                NetworkServiceFlushCache();
            }
            osDelay(200); // 2秒心跳巡检
        }
    }
}

int NetworkServiceInit(void)
{
    g_cache_mutex = osMutexNew(NULL);

    osThreadAttr_t attr;
    attr.name = "NetworkTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = NET_TASK_STACK_SIZE;
    attr.priority = NET_TASK_PRIORITY;

    if (osThreadNew(NetworkTask, NULL, &attr) == NULL) {
        printf("[ERROR] Failed to create NetworkTask thread!\n");
        return -1;
    }
    printf("[SUCCESS] NetworkTask thread created for independent Wi-Fi operation.\n");
    return 0;
}
