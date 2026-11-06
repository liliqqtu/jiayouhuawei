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
#include "voice_syn6288.h"
#include "emergency_sos.h"
#include "alarm_beeper.h"
#include "cJSON.h"
#include "oc_mqtt.h"
#include "oc_mqtt_profile_package.h"

#define NET_TASK_STACK_SIZE     (1024 * 8)
#define NET_TASK_PRIORITY       osPriorityNormal

static volatile int g_wifi_connected = 0;
static volatile int g_network_ready = 0;
static volatile int g_cloud_connected = 0;
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

// 云端控制指令响应回调 (处理华为云 IoTDA 下发的指令，如远程消警、音量调节)
static void OnCloudCommandReceived(uint8_t *recv_data, uint32_t recv_size, uint8_t **resp_data, uint32_t *resp_size)
{
    if (recv_data == NULL || recv_size == 0) {
        *resp_data = NULL;
        *resp_size = 0;
        return;
    }

    char *buf = malloc(recv_size + 1);
    if (buf == NULL) {
        *resp_data = NULL;
        *resp_size = 0;
        return;
    }
    memcpy(buf, recv_data, recv_size);
    buf[recv_size] = '\0';

    printf("\n[HUAWEI_CLOUD] >>> Command received from Cloud! <<<\n");
    printf("[HUAWEI_CLOUD] Raw JSON: %s\n", buf);

    cJSON *root = cJSON_Parse(buf);
    if (root != NULL) {
        cJSON *cmd_item = cJSON_GetObjectItem(root, "command_name");
        cJSON *paras_item = cJSON_GetObjectItem(root, "paras");

        if (cmd_item != NULL && cJSON_IsString(cmd_item)) {
            const char *cmd_name = cmd_item->valuestring;
            printf("[HUAWEI_CLOUD] Command Name: '%s'\n", cmd_name);

            // 1. 远程消警命令 ResetAlert
            if (strcmp(cmd_name, "ResetAlert") == 0) {
                printf("[HUAWEI_CLOUD] Remote ResetAlert executed! Cancelling alarm...\n");
                EmergencySosClear();
                AlarmBeeperMute();
                VoicePlayPresetAlert(VOICE_ALERT_CLEAR);
            }
            // 2. 远程调节语音音量 SetVoiceVolume
            else if (strcmp(cmd_name, "SetVoiceVolume") == 0) {
                if (paras_item != NULL) {
                    cJSON *vol_item = cJSON_GetObjectItem(paras_item, "volume");
                    if (vol_item != NULL && cJSON_IsNumber(vol_item)) {
                        int new_vol = vol_item->valueint;
                        if (new_vol >= 0 && new_vol <= 16) {
                            VoiceSetVolume((uint8_t)new_vol);
                            printf("[HUAWEI_CLOUD] Voice volume updated to: %d\n", new_vol);
                        }
                    }
                }
            }
        }
        cJSON_Delete(root);
    }
    free(buf);

    *resp_data = NULL;
    *resp_size = 0;
}

int NetworkServiceReportHuaweiCloud(const elderly_care_data_t *data)
{
    if (!g_cloud_connected || data == NULL) {
        return -1;
    }

    oc_mqtt_profile_service_t service;
    oc_mqtt_profile_kv_t kv_sos_active;
    oc_mqtt_profile_kv_t kv_sos_source;
    oc_mqtt_profile_kv_t kv_motion_active;
    oc_mqtt_profile_kv_t kv_no_motion_sec;
    oc_mqtt_profile_kv_t kv_motion_count;
    oc_mqtt_profile_kv_t kv_door_open;
    oc_mqtt_profile_kv_t kv_temp;
    oc_mqtt_profile_kv_t kv_humi;
    oc_mqtt_profile_kv_t kv_smoke;
    oc_mqtt_profile_kv_t kv_alert_level;
    oc_mqtt_profile_kv_t kv_alert_type;
    oc_mqtt_profile_kv_t kv_voice_vol;

    int val_sos_active = data->sos_active ? 1 : 0;
    int val_sos_source = (int)data->sos_source;
    int val_motion_active = data->motion_active ? 1 : 0;
    int val_no_motion_sec = (int)data->seconds_since_last_motion;
    int val_motion_count = (int)data->motion_events_today;
    int val_door_open = data->door_open ? 1 : 0;
    double val_temp = (double)data->temperature;
    double val_humi = (double)data->humidity;
    double val_smoke = 0.05; // MQ-2 烟雾模拟/采样电压
    int val_alert_level = (int)data->current_level;
    int val_alert_type = (int)data->current_alert;
    int val_voice_vol = (int)data->voice_volume;

    service.event_time = NULL;
    service.service_id = "ElderlyCare";
    service.service_property = &kv_sos_active;
    service.nxt = NULL;

    // 1. SosActive
    kv_sos_active.key = "SosActive";
    kv_sos_active.value = &val_sos_active;
    kv_sos_active.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_sos_active.nxt = &kv_sos_source;

    // 2. SosSource
    kv_sos_source.key = "SosSource";
    kv_sos_source.value = &val_sos_source;
    kv_sos_source.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_sos_source.nxt = &kv_motion_active;

    // 3. MotionActive
    kv_motion_active.key = "MotionActive";
    kv_motion_active.value = &val_motion_active;
    kv_motion_active.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_motion_active.nxt = &kv_no_motion_sec;

    // 4. SecondsSinceMotion
    kv_no_motion_sec.key = "SecondsSinceMotion";
    kv_no_motion_sec.value = &val_no_motion_sec;
    kv_no_motion_sec.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_no_motion_sec.nxt = &kv_motion_count;

    // 5. MotionEventsToday
    kv_motion_count.key = "MotionEventsToday";
    kv_motion_count.value = &val_motion_count;
    kv_motion_count.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_motion_count.nxt = &kv_door_open;

    // 6. DoorOpen
    kv_door_open.key = "DoorOpen";
    kv_door_open.value = &val_door_open;
    kv_door_open.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_door_open.nxt = &kv_temp;

    // 7. Temperature
    kv_temp.key = "Temperature";
    kv_temp.value = &val_temp;
    kv_temp.type = EN_OC_MQTT_PROFILE_VALUE_FLOAT;
    kv_temp.nxt = &kv_humi;

    // 8. Humidity
    kv_humi.key = "Humidity";
    kv_humi.value = &val_humi;
    kv_humi.type = EN_OC_MQTT_PROFILE_VALUE_FLOAT;
    kv_humi.nxt = &kv_smoke;

    // 9. SmokeVoltage
    kv_smoke.key = "SmokeVoltage";
    kv_smoke.value = &val_smoke;
    kv_smoke.type = EN_OC_MQTT_PROFILE_VALUE_FLOAT;
    kv_smoke.nxt = &kv_alert_level;

    // 10. AlertLevel
    kv_alert_level.key = "AlertLevel";
    kv_alert_level.value = &val_alert_level;
    kv_alert_level.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_alert_level.nxt = &kv_alert_type;

    // 11. AlertType
    kv_alert_type.key = "AlertType";
    kv_alert_type.value = &val_alert_type;
    kv_alert_type.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_alert_type.nxt = &kv_voice_vol;

    // 12. VoiceVolume
    kv_voice_vol.key = "VoiceVolume";
    kv_voice_vol.value = &val_voice_vol;
    kv_voice_vol.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    kv_voice_vol.nxt = NULL;

    int ret = oc_mqtt_profile_propertyreport(CFG_OC_DEVICE_ID, &service);
    if (ret == 0) {
        printf("[HUAWEI_CLOUD] >>> Property reported OK (IoTDA Shadow Updated) <<<\n");
    } else {
        printf("[HUAWEI_CLOUD] Property report failed! err=%d\n", ret);
    }
    return ret;
}

bool NetworkServiceIsCloudConnected(void)
{
    return (g_cloud_connected != 0);
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

                        // 初始化并连接华为云 IoTDA
                        printf("[HUAWEI_CLOUD] Connecting to Huawei Cloud IoTDA (%s:%d)...\n", CFG_OC_SERVER_IP, CFG_OC_SERVER_PORT);
                        device_info_init(CFG_OC_CLIENT_ID, CFG_OC_USERNAME, CFG_OC_PASSWORD);
                        int oc_ret = oc_mqtt_init();
                        if (oc_ret == 0) {
                            printf("\n=======================================================\n");
                            printf("[HUAWEI_CLOUD] >>> SUCCESS: Connected to Huawei Cloud IoTDA! <<<\n");
                            printf("=======================================================\n\n");
                            oc_set_cmd_rsp_cb(OnCloudCommandReceived);
                            g_cloud_connected = 1;
                        } else {
                            printf("[HUAWEI_CLOUD] oc_mqtt_init failed ret=%d\n", oc_ret);
                            g_cloud_connected = 0;
                        }

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

// 处理局域网或外部接收到的控制指令 (支持音量设置、查询、文本语音播报)
int NetworkServiceProcessCommand(const char *cmd_str, int cmd_len, char *ack_buf, int ack_buf_size)
{
    if (cmd_str == NULL || cmd_len <= 0) {
        return -1;
    }

    // 1. 查询音量指令 (getVolume / get_vol)
    if (strstr(cmd_str, "getVolume") != NULL || strstr(cmd_str, "get_vol") != NULL) {
        uint8_t cur_vol = VoiceGetVolume();
        if (ack_buf && ack_buf_size > 0) {
            snprintf(ack_buf, ack_buf_size,
                     "{\"status\":\"ok\",\"cmd\":\"getVolume\",\"volume\":%u,\"deviceId\":\"bearpi-001\"}",
                     (unsigned int)cur_vol);
        }
        return 0;
    }

    // 2. 设置音量指令 (兼容 {"cmd":"setVolume","volume":12} 或 {"volume":12} 或 volume=12 或 vol:12)
    const char *vol_pos = strstr(cmd_str, "volume");
    if (vol_pos == NULL) {
        vol_pos = strstr(cmd_str, "Volume");
    }
    if (vol_pos == NULL) {
        vol_pos = strstr(cmd_str, "vol");
    }
    if (vol_pos == NULL) {
        vol_pos = strstr(cmd_str, "VOL");
    }

    if (vol_pos != NULL) {
        const char *p = vol_pos;
        while (*p && (*p < '0' || *p > '9') && *p != '-') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            int val = atoi(p);
            // 自动换算: 17~100 视为百分比
            if (val > 16 && val <= 100) {
                val = (val * 16 + 50) / 100;
            } else if (val > 100) {
                val = 16;
            } else if (val < 0) {
                val = 0;
            }

            VoiceSetVolume((uint8_t)val);

            // 语音播放音量调节测试提示
            VoicePlayVolumeTest((uint8_t)val);

            if (ack_buf && ack_buf_size > 0) {
                snprintf(ack_buf, ack_buf_size,
                         "{\"status\":\"ok\",\"cmd\":\"setVolume\",\"volume\":%u,\"deviceId\":\"bearpi-001\"}",
                         (unsigned int)val);
            }
            return 0;
        }
    }

    // 3. 停止播放指令
    if (strstr(cmd_str, "stop") != NULL) {
        VoiceStop();
        if (ack_buf && ack_buf_size > 0) {
            snprintf(ack_buf, ack_buf_size, "{\"status\":\"ok\",\"cmd\":\"stop\"}");
        }
        return 0;
    }

    // 4. 远程文字转语音直接播报指令 ({"cmd":"play","text":"..."})
    const char *text_pos = strstr(cmd_str, "\"text\":");
    if (text_pos != NULL) {
        const char *q1 = strchr(text_pos + 7, '\"');
        if (q1 != NULL) {
            const char *q2 = strchr(q1 + 1, '\"');
            if (q2 != NULL && q2 > q1) {
                char text_buf[128];
                int len = (int)(q2 - q1 - 1);
                if (len >= (int)sizeof(text_buf)) {
                    len = (int)sizeof(text_buf) - 1;
                }
                memcpy(text_buf, q1 + 1, len);
                text_buf[len] = '\0';
                VoicePlayText(text_buf);
                if (ack_buf && ack_buf_size > 0) {
                    snprintf(ack_buf, ack_buf_size, "{\"status\":\"ok\",\"cmd\":\"play\",\"text\":\"%s\"}", text_buf);
                }
                return 0;
            }
        }
    }

    return -1;
}

#define NET_CMD_TASK_STACK_SIZE (1024 * 4)

// 局域网控制指令监听任务 (UDP)
static void NetCmdListenerTask(void *arg)
{
    (void)arg;
    int cmd_sock = -1;
    struct sockaddr_in bind_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char rx_buf[256];
    char ack_buf[256];

    while (1) {
        if (!g_network_ready) {
            if (cmd_sock >= 0) {
                lwip_close(cmd_sock);
                cmd_sock = -1;
            }
            osDelay(50);
            continue;
        }

        if (cmd_sock < 0) {
            cmd_sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (cmd_sock < 0) {
                printf("[NET_CMD] Error creating UDP socket\n");
                osDelay(100);
                continue;
            }

            int reuse = 1;
            setsockopt(cmd_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            int bcast = 1;
            setsockopt(cmd_sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            setsockopt(cmd_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            memset(&bind_addr, 0, sizeof(bind_addr));
            bind_addr.sin_family = AF_INET;
            bind_addr.sin_port = htons(CFG_UDP_CMD_PORT);
            bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(cmd_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
                printf("[NET_CMD] Failed to bind UDP port %d, retrying...\n", CFG_UDP_CMD_PORT);
                lwip_close(cmd_sock);
                cmd_sock = -1;
                osDelay(200);
                continue;
            }
            printf("\n[NET_CMD] >>> UDP Command Listener Ready on Port %d (Volume Control Active) <<<\n\n",
                   CFG_UDP_CMD_PORT);
        }

        client_len = sizeof(client_addr);
        memset(&client_addr, 0, sizeof(client_addr));
        int n = recvfrom(cmd_sock, rx_buf, sizeof(rx_buf) - 1, 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if (n > 0) {
            rx_buf[n] = '\0';
            printf("[NET_CMD] Received %d bytes from %s:%d -> %s\n",
                   n, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), rx_buf);

            memset(ack_buf, 0, sizeof(ack_buf));
            int ret = NetworkServiceProcessCommand(rx_buf, n, ack_buf, sizeof(ack_buf));
            if (ret == 0 && strlen(ack_buf) > 0) {
                sendto(cmd_sock, ack_buf, strlen(ack_buf), 0,
                       (struct sockaddr *)&client_addr, client_len);
                printf("[NET_CMD] Sent ACK -> %s\n", ack_buf);
            }
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

    // 创建局域网指令监听任务 (用于远程音量调节等控制)
    osThreadAttr_t cmd_attr;
    cmd_attr.name = "NetCmdListenerTask";
    cmd_attr.attr_bits = 0U;
    cmd_attr.cb_mem = NULL;
    cmd_attr.cb_size = 0U;
    cmd_attr.stack_mem = NULL;
    cmd_attr.stack_size = NET_CMD_TASK_STACK_SIZE;
    cmd_attr.priority = osPriorityNormal;

    if (osThreadNew(NetCmdListenerTask, NULL, &cmd_attr) == NULL) {
        printf("[ERROR] Failed to create NetCmdListenerTask thread!\n");
    } else {
        printf("[SUCCESS] NetCmdListenerTask thread created for LAN command control.\n");
    }

    return 0;
}
