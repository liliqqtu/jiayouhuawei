/**
 * @file    net.c
 * @brief   Wi-Fi 连接 + HTTP 客户端（POST 上报 / GET 拉取下行确认）
 *
 * 依赖：wifi_device.h、lwip（netifapi / sockets）
 *
 * 重要提示：
 *   1. Hi3861 只支持 2.4G Wi-Fi，5G 连不上。
 *   2. 若你的 SDK 版本里 WifiEvent 回调的返回值是 WifiErrorCode 而不是 void，
 *      把下面三个回调的返回类型改掉即可（编译器会直接指出来）。
 *   3. 也可以直接用官方 bearpi_hm_nano/sample/wifi_connect 里的 WifiConnect()，
 *      替换掉 Net_ConnectWifi() 即可，其它代码不用动。
 *   4. 板端只解析响应里的两三个整数字段（识别“是否还有待确认告警”），
 *      不引入 JSON 库 —— 512 字节 RAM 比一个解析器值钱。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "wifi_device.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "config.h"
#include "net.h"

#define HTTP_BUF_SIZE   640     /* 请求行 + 头 + body */
#define HTTP_RESP_SIZE  512     /* 响应缓冲（含头），网关响应都小于 1KB */
#define CACHE_ITEM_SIZE 512

static volatile int g_wifi_connected = 0;
static char g_cache[CFG_CACHE_DEPTH][CACHE_ITEM_SIZE];
static int  g_cache_count = 0;

/* ------------------------------------------------------------------ */
/* Wi-Fi                                                               */
/* ------------------------------------------------------------------ */
static void OnWifiConnectionChanged(int state, WifiLinkedInfo *info)
{
    (void)info;
    g_wifi_connected = (state == WIFI_STATE_AVALIABLE) ? 1 : 0;
    printf("[net] wifi state -> %d\r\n", state);
}

static void OnWifiScanStateChanged(int state, int size)
{
    (void)state;
    (void)size;
}

static int Net_ConnectWifi(void)
{
    WifiEvent event = {0};
    event.OnWifiConnectionChanged = OnWifiConnectionChanged;
    event.OnWifiScanStateChanged = OnWifiScanStateChanged;

    if (RegisterWifiEvent(&event) != WIFI_SUCCESS) {
        printf("[net] RegisterWifiEvent failed\r\n");
        return -1;
    }
    if (EnableWifi() != WIFI_SUCCESS) {
        printf("[net] EnableWifi failed\r\n");
        return -1;
    }

    WifiDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    /* 注意缓冲区长度：strncpy 不会自动补 '\0'，这里用 snprintf 兜底 */
    snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", CFG_WIFI_SSID);
    snprintf(cfg.preSharedKey, sizeof(cfg.preSharedKey), "%s", CFG_WIFI_PASSWORD);
    cfg.securityType = WIFI_SEC_TYPE_PSK;

    int netId = -1;
    if (AddDeviceConfig(&cfg, &netId) != WIFI_SUCCESS) {
        printf("[net] AddDeviceConfig failed\r\n");
        return -1;
    }
    if (ConnectTo(netId) != WIFI_SUCCESS) {
        printf("[net] ConnectTo failed\r\n");
        return -1;
    }

    /* 等连接结果，最多 15 秒 */
    for (int i = 0; i < 150 && !g_wifi_connected; i++) {
        osDelay(100);
    }
    if (!g_wifi_connected) {
        printf("[net] wifi connect timeout\r\n");
        return -1;
    }

    /* 拿到 IP（DHCP） */
    struct netif *iface = netifapi_netif_find("wlan0");
    if (iface == NULL) {
        printf("[net] wlan0 not found\r\n");
        return -1;
    }
    netifapi_dhcp_start(iface);
    osDelay(500);
    printf("[net] connected, ip=%s\r\n", ip4addr_ntoa(netif_ip4_addr(iface)));
    return 0;
}

int Net_Init(void)
{
    if (Net_ConnectWifi() != 0) {
        return -1;
    }
    return 0;
}

int Net_IsOnline(void)
{
    return g_wifi_connected;
}

/* ------------------------------------------------------------------ */
/* HTTP 基础                                                           */
/* ------------------------------------------------------------------ */
/** 从响应头里取 Content-Length；取不到返回 -1（那就按“读到对端关闭”为准） */
static int content_length_of(const char *raw)
{
    const char *p = strstr(raw, "Content-Length:");
    if (p == NULL) {
        p = strstr(raw, "content-length:");
    }
    if (p == NULL) {
        return -1;
    }
    p += strlen("Content-Length:");
    while (*p == ' ') {
        p++;
    }
    return atoi(p);
}

/**
 * 发起一次 HTTP 请求（短连接）。
 * @param body      POST 用；为 NULL 表示 GET
 * @param resp      非 NULL 时把响应体拷出去（GET 用）
 * @param resp_size resp 的容量
 * @return 0 = 状态码 200；-1 = 失败
 */
static int http_do(const char *method, const char *ip, hi_u16 port, const char *path,
                   const char *body, char *resp, int resp_size)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    /* 收发超时，防止网关不在时卡死整个任务 */
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        closesocket(fd);
        return -1;
    }

    char req[HTTP_BUF_SIZE];
    int len;
    if (body != NULL) {
        len = snprintf(req, sizeof(req),
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            path, ip, (int)port, (int)strlen(body), body);
    } else {
        len = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, ip, (int)port);
    }
    if (len <= 0 || len >= (int)sizeof(req)) {
        closesocket(fd);
        printf("[net] request too large\r\n");
        return -1;
    }

    int sent = send(fd, req, len, 0);
    if (sent != len) {
        closesocket(fd);
        return -1;
    }

    /* 收响应：读到“头 + 完整 body”为止，避免每次多等一个 2 秒超时 */
    static char raw[HTTP_RESP_SIZE];
    memset(raw, 0, sizeof(raw));
    int total = 0;
    int expect = -1;
    while (total < (int)sizeof(raw) - 1) {
        int n = recv(fd, raw + total, sizeof(raw) - 1 - total, 0);
        if (n <= 0) {
            break;
        }
        total += n;
        char *hdr_end = strstr(raw, "\r\n\r\n");
        if (hdr_end != NULL) {
            if (expect < 0) {
                expect = content_length_of(raw);
            }
            int body_have = (int)(total - (int)(hdr_end + 4 - raw));
            if (expect >= 0 && body_have >= expect) {
                break;
            }
        }
    }
    closesocket(fd);

    if (total <= 0) {
        return -1;
    }
    raw[total] = '\0';

    /* 只要状态行里出现 200 就算成功 */
    if (strstr(raw, " 200 ") == NULL) {
        printf("[net] http status not 200: %.24s\r\n", raw);
        return -1;
    }

    if (resp != NULL && resp_size > 0) {
        char *hdr_end = strstr(raw, "\r\n\r\n");
        if (hdr_end == NULL) {
            resp[0] = '\0';
        } else {
            snprintf(resp, resp_size, "%s", hdr_end + 4);
        }
    }
    return 0;
}

int Net_PostJson(const char *path, const char *body)
{
    if (!g_wifi_connected) {
        return -1;
    }
    return http_do("POST", CFG_GATEWAY_IP, CFG_GATEWAY_PORT, path, body, NULL, 0);
}

int Net_GetJson(const char *path, char *resp, int resp_size)
{
    if (!g_wifi_connected) {
        return -1;
    }
    return http_do("GET", CFG_GATEWAY_IP, CFG_GATEWAY_PORT, path, NULL, resp, resp_size);
}

/* ------------------------------------------------------------------ */
/* 断网缓存（环形，满了丢最旧）                                         */
/* ------------------------------------------------------------------ */
void Net_PushCache(const char *body)
{
    if (body == NULL) {
        return;
    }
    if (g_cache_count >= CFG_CACHE_DEPTH) {
        /* 前移覆盖最旧一条 */
        memmove(g_cache[0], g_cache[1], (CFG_CACHE_DEPTH - 1) * CACHE_ITEM_SIZE);
        g_cache_count = CFG_CACHE_DEPTH - 1;
        printf("[net] cache full, dropping oldest\r\n");
    }
    snprintf(g_cache[g_cache_count], CACHE_ITEM_SIZE, "%s", body);
    g_cache_count++;
}

int Net_FlushCache(void)
{
    if (!g_wifi_connected || g_cache_count == 0) {
        return 0;
    }
    int ok = 0;
    int i = 0;
    while (i < g_cache_count) {
        if (Net_PostJson("/api/v1/snapshot", g_cache[i]) == 0) {
            ok++;
            i++;
        } else {
            break;      /* 还是连不上，下次再补 */
        }
    }
    if (i > 0) {
        memmove(g_cache[0], g_cache[i], (g_cache_count - i) * CACHE_ITEM_SIZE);
        g_cache_count -= i;
    }
    if (ok > 0) {
        printf("[net] backfilled %d frames, %d left\r\n", ok, g_cache_count);
    }
    return ok;
}
