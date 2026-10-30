/*
 * Wi-Fi 连接管理与 UDP 遥测
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

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
static volatile int g_scan_done = 0;
static volatile int g_scan_num = 0;
static struct netif *g_iface = NULL;

// 离线缓存队列
static char g_cache[CFG_CACHE_DEPTH][CFG_CACHE_ITEM_SIZE];
static int  g_cache_count = 0;
static osMutexId_t g_cache_mutex = NULL;

static void OnWifiConnectionChanged(int state, WifiLinkedInfo *info)
{
    (void)info;
    if (state == WIFI_STATE_AVALIABLE) {
        g_wifi_connected = 1;
        printf("\n[WIFI_EVENT] Connected to AP!\n");
    } else {
        g_wifi_connected = 0;
        g_network_ready = 0;
        printf("\n[WIFI_EVENT] Disconnected (state=%d)\n", state);
    }
}

static void OnWifiScanStateChanged(int state, int size)
{
    (void)state;
    g_scan_num = size;
    g_scan_done = 1;
    printf("[WIFI_EVENT] Scan complete: %d APs\n", size);
}

static WifiEvent g_wifi_event = {
    .OnWifiConnectionChanged = OnWifiConnectionChanged,
    .OnWifiScanStateChanged = OnWifiScanStateChanged,
    .OnHotspotStateChanged = NULL,
    .OnHotspotStaJoin = NULL,
    .OnHotspotStaLeave = NULL
};

// 发送 UDP 数据包
static int SendUdpPacket(const char *data)
{
    if (data == NULL || strlen(data) == 0) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("[UDP] socket failed\n");
        return -1;
    }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    int len = (int)strlen(data);

    // 单播发送
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(CFG_UDP_PORT);
    dest.sin_addr.s_addr = inet_addr(CFG_GATEWAY_IP);
    int sent = sendto(sock, data, len, 0, (struct sockaddr *)&dest, sizeof(dest));

    // 广播兜底
    struct sockaddr_in bcast;
    memset(&bcast, 0, sizeof(bcast));
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(CFG_UDP_PORT);
    bcast.sin_addr.s_addr = inet_addr("255.255.255.255");
    sendto(sock, data, len, 0, (struct sockaddr *)&bcast, sizeof(bcast));

    lwip_close(sock);

    if (sent != len) {
        printf("[UDP] send error: sent=%d, total=%d\n", sent, len);
        return -1;
    }
    return 0;
}

// 离线缓存入队
void NetworkServicePushCache(const char *json_data)
{
    if (json_data == NULL || strlen(json_data) == 0) {
        return;
    }

    if (g_cache_mutex != NULL) {
        osMutexAcquire(g_cache_mutex, osWaitForever);
    }

    if (g_cache_count >= CFG_CACHE_DEPTH) {
        // 队列满则移出最旧的一条
        memmove(g_cache[0], g_cache[1], (CFG_CACHE_DEPTH - 1) * CFG_CACHE_ITEM_SIZE);
        g_cache_count = CFG_CACHE_DEPTH - 1;
    }

    strncpy(g_cache[g_cache_count], json_data, CFG_CACHE_ITEM_SIZE - 1);
    g_cache[g_cache_count][CFG_CACHE_ITEM_SIZE - 1] = '\0';
    g_cache_count++;

    printf("[CACHE] Cached item #%d\n", g_cache_count);

    if (g_cache_mutex != NULL) {
        osMutexRelease(g_cache_mutex);
    }
}

// 补发离线缓存
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
            osDelay(2);
        } else {
            break;
        }
    }

    if (i > 0) {
        if (i < g_cache_count) {
            memmove(g_cache[0], g_cache[i], (g_cache_count - i) * CFG_CACHE_ITEM_SIZE);
        }
        g_cache_count -= i;
        printf("[CACHE] Flushed %d items, remain: %d\n", flushed, g_cache_count);
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

// 扫描周边 Wi-Fi，锁定指定热点的频段 (frequency) 与 BSSID
static int ScanAndMatchTargetAp(WifiDeviceConfig *apConfig, const char *target_ssid)
{
    g_scan_done = 0;
    g_scan_num = 0;

    printf("[NETWORK] Scanning 2.4GHz channels for '%s'...\n", target_ssid);
    WifiErrorCode scanRet = Scan();
    if (scanRet != WIFI_SUCCESS) {
        printf("[NETWORK] Scan() trigger err=%d\n", scanRet);
        return 0;
    }

    // 等待扫描完成，最多 4 秒 (40 * 100ms)
    for (int i = 0; i < 40; i++) {
        osDelay(10);
        if (g_scan_done) {
            break;
        }
    }

    if (!g_scan_done || g_scan_num == 0) {
        printf("[NETWORK] Scan timeout or no APs detected in range.\n");
        return 0;
    }

    WifiScanInfo *scanList = (WifiScanInfo *)malloc(sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);
    if (scanList == NULL) {
        printf("[NETWORK] Failed to allocate memory for scan list!\n");
        return 0;
    }
    memset(scanList, 0, sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);

    unsigned int size = WIFI_SCAN_HOTSPOT_LIMIT;
    WifiErrorCode listRet = GetScanInfoList(scanList, &size);
    if (listRet != WIFI_SUCCESS) {
        printf("[NETWORK] GetScanInfoList err=%d\n", listRet);
        free(scanList);
        return 0;
    }

    int matched = 0;
    for (unsigned int i = 0; i < size && i < WIFI_SCAN_HOTSPOT_LIMIT; i++) {
        if (strlen(scanList[i].ssid) > 0) {
            printf("  [SCAN] #%02u: SSID='%s', RSSI=%d, Freq=%d MHz\n",
                   i + 1, scanList[i].ssid, scanList[i].rssi, scanList[i].frequency);
        }
        if (strcmp(scanList[i].ssid, target_ssid) == 0) {
            printf("\n[NETWORK] >>> AP MATCHED! BSSID=%02X:%02X:%02X:%02X:%02X:%02X, Freq=%d MHz, RSSI=%d <<<\n\n",
                   scanList[i].bssid[0], scanList[i].bssid[1], scanList[i].bssid[2],
                   scanList[i].bssid[3], scanList[i].bssid[4], scanList[i].bssid[5],
                   scanList[i].frequency, scanList[i].rssi);

            apConfig->freq = scanList[i].frequency;
            memcpy(apConfig->bssid, scanList[i].bssid, WIFI_MAC_LEN);
            apConfig->securityType = scanList[i].securityType;
            matched = 1;
            break;
        }
    }

    free(scanList);
    return matched;
}

// Wi-Fi 任务
static void NetworkTask(void *arg)
{
    (void)arg;
    osDelay(100);

    printf("\n[NETWORK] Wi-Fi init, SSID: %s\n\n", CFG_WIFI_SSID);

    RegisterWifiEvent(&g_wifi_event);
    EnableWifi();
    osDelay(30);

    int netId = -1;

    while (1) {
        if (!g_wifi_connected) {
            g_network_ready = 0;

            // 配置热点信息
            WifiDeviceConfig apConfig;
            memset(&apConfig, 0, sizeof(apConfig));
            strncpy(apConfig.ssid, CFG_WIFI_SSID, sizeof(apConfig.ssid) - 1);
            strncpy(apConfig.preSharedKey, CFG_WIFI_PASSWORD, sizeof(apConfig.preSharedKey) - 1);
            apConfig.securityType = WIFI_SEC_TYPE_PSK;
            apConfig.wapiPskType = WIFI_PSK_TYPE_ASCII;
            apConfig.ipType = DHCP;

            // 先扫描热点匹配信道
            int matched = ScanAndMatchTargetAp(&apConfig, CFG_WIFI_SSID);
            if (!matched) {
                printf("[NETWORK] Hotspot '%s' not found, try default connect\n", CFG_WIFI_SSID);
            }

            if (netId >= 0) {
                RemoveDevice(netId);
                netId = -1;
            }

            WifiErrorCode addErr = AddDeviceConfig(&apConfig, &netId);
            if (addErr != WIFI_SUCCESS || netId < 0) {
                printf("[NETWORK] AddDeviceConfig failed! err=%d\n", addErr);
                osDelay(300);
                continue;
            }

            printf("[NETWORK] Connecting to '%s' (netId=%d)...\n", CFG_WIFI_SSID, netId);
            ConnectTo(netId);

            // 等待连接
            for (int i = 0; i < 100 && !g_wifi_connected; i++) {
                osDelay(10); // 100ms
            }

            if (g_wifi_connected) {
                printf("[NETWORK] Connected, requesting DHCP...\n");
                g_iface = netifapi_netif_find("wlan0");
                if (g_iface != NULL) {
                    netifapi_dhcp_stop(g_iface);
                    osDelay(10);

                    netifapi_dhcp_start(g_iface);

                    // 检查DHCP是否成功
                    int dhcp_ok = 0;
                    for (int t = 0; t < 150; t++) {
                        osDelay(10);
                        if (netifapi_dhcp_is_bound(g_iface) == ERR_OK) {
                            if (netif_ip4_addr(g_iface)->addr != 0) {
                                dhcp_ok = 1;
                                break;
                            }
                        }
                    }

                    if (dhcp_ok) {
                        printf("\n[NETWORK] DHCP Success! IP: %s\n\n", ip4addr_ntoa(netif_ip4_addr(g_iface)));
                        g_network_ready = 1;

                        // 补传离线数据
                        NetworkServiceFlushCache();
                    } else {
                        printf("[NETWORK] DHCP timeout\n");
                        netifapi_dhcp_stop(g_iface);
                        Disconnect();
                        g_wifi_connected = 0;
                        g_network_ready = 0;
                        osDelay(300);
                        continue;
                    }
                }
            } else {
                printf("[NETWORK] Connect timeout, retry in 5s\n");
                if (netId >= 0) {
                    RemoveDevice(netId);
                    netId = -1;
                }
                osDelay(500);
                continue;
            }
        } else {
            if (g_iface != NULL && netifapi_dhcp_is_bound(g_iface) != ERR_OK) {
                printf("[NETWORK] DHCP lost, reconnecting...\n");
                g_network_ready = 0;
                g_wifi_connected = 0;
                netifapi_dhcp_stop(g_iface);
                Disconnect();
                continue;
            }

            if (g_network_ready && g_cache_count > 0) {
                NetworkServiceFlushCache();
            }
            osDelay(100);
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
