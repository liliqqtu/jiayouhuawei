/**
 * @file    sntp.h
 * @brief   极简 SNTP 校时（UDP，60 行）。
 *
 * 为什么必须有：Hi3861 没有 RTC，断电后系统时间是 0。
 * 不校时的话，上报的 ts 会是 1970 年，网关的日报/趋势全部错位。
 *
 * 说明：用固定 IP 而不是域名，省掉 DNS 解析，少一个失败点。
 */

#ifndef __SNTP_H__
#define __SNTP_H__

/** 同步一次时间；返回 0 表示成功。建议联网成功后立即调用一次。 */
int  Sntp_Sync(void);

/** 是否已同步过 */
int  Sntp_Synced(void);

/** 当前 Unix 秒（未同步时返回开机秒数，仅用于相对判断） */
unsigned int Sntp_NowSec(void);

/** 把任意 Unix 秒格式化成 ISO（用于补 lastMotionTs） */
void Sntp_IsoOfSec(unsigned int unix_sec, char *buf, int size);

/** 输出 ISO8601 东八区时间，例如 2026-11-02T14:03:00+08:00 */
void Sntp_ToIso(char *buf, int size);

#endif /* __SNTP_H__ */
