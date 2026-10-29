/**
 * @file    sntp.c
 * @brief   UDP 向 NTP 服务器取一次时间，换算成本地 Unix 秒。
 *
 * NTP 时间戳 = 1900-01-01 起的秒数；Unix = 1970-01-01 起，两者差 2208988800 秒。
 */

#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "hi_time.h"

#include "sntp.h"

/* ntp.aliyun.com 的固定 IP；换服务器只改这一行 */
#define SNTP_SERVER_IP   "203.107.6.88"
#define SNTP_PORT        123
#define NTP_UNIX_DELTA   2208988800UL

static int g_synced = 0;
static unsigned int g_offset = 0;   /* unix_sec - uptime_sec */

static unsigned int uptime_sec(void)
{
    return (unsigned int)(hi_get_milli_seconds() / 1000);
}

int Sntp_Sync(void)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SNTP_PORT);
    addr.sin_addr.s_addr = inet_addr(SNTP_SERVER_IP);

    /* 客户端请求：第 1 字节 LI=0 VN=4 Mode=3 -> 0x1B，其余为 0 */
    unsigned char pkt[48];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0x1B;

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (sendto(fd, pkt, sizeof(pkt), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        closesocket(fd);
        return -1;
    }

    unsigned char resp[48];
    int n = recv(fd, resp, sizeof(resp), 0);
    closesocket(fd);
    if (n < 48) {
        printf("[sntp] no response\r\n");
        return -1;
    }

    /* 发送时间戳在 40~43 字节（大端） */
    unsigned int ntp_sec = ((unsigned int)resp[40] << 24) |
                           ((unsigned int)resp[41] << 16) |
                           ((unsigned int)resp[42] << 8) |
                           ((unsigned int)resp[43]);
    if (ntp_sec < NTP_UNIX_DELTA) {
        printf("[sntp] bad timestamp\r\n");
        return -1;
    }

    unsigned int unix_sec = ntp_sec - NTP_UNIX_DELTA;
    g_offset = unix_sec - uptime_sec();
    g_synced = 1;
    printf("[sntp] synced, unix=%u\r\n", unix_sec);
    return 0;
}

int Sntp_Synced(void)
{
    return g_synced;
}

unsigned int Sntp_NowSec(void)
{
    return uptime_sec() + g_offset;
}

/* 把 Unix 秒格式化成东八区 ISO；不依赖 time.h 的本地时区设置 */
void Sntp_IsoOfSec(unsigned int unix_sec, char *buf, int size)
{
    unsigned int t = unix_sec + 8 * 3600;   /* 转东八区 */
    unsigned int days = t / 86400;
    unsigned int secs = t % 86400;
    int hour = (int)(secs / 3600);
    int min = (int)((secs % 3600) / 60);
    int sec = (int)(secs % 60);

    /* 1970-01-01 起的天数 -> 年月日（Howard Hinnant 算法，无闰年判断分支） */
    unsigned int z = days + 719468;
    unsigned int era = z / 146097;
    unsigned int doe = z - era * 146097;
    unsigned int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned int y = yoe + era * 400;
    unsigned int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned int mp = (5 * doy + 2) / 153;
    unsigned int d = doy - (153 * mp + 2) / 5 + 1;
    unsigned int m = mp + (mp < 10 ? 3 : -9);
    unsigned int year = y + (m <= 2 ? 1 : 0);

    snprintf(buf, size, "%04u-%02u-%02uT%02d:%02d:%02d+08:00",
             year, m, d, hour, min, sec);
}

void Sntp_ToIso(char *buf, int size)
{
    Sntp_IsoOfSec(Sntp_NowSec(), buf, size);
}
