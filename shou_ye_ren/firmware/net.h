/**
 * @file    net.h
 * @brief   网络层：Wi-Fi STA 连接 + HTTP POST + 断网缓存补传。
 *
 * 为什么用 HTTP 而不是 MQTT：
 *   轻量系统上跑 MQTT 需要额外移植 paho，工期不允许；
 *   局域网 HTTP 短连接在“5 秒一帧”的频率下足够稳，且网关侧零依赖。
 *   代价是断网期间必须缓存 —— 这个已经做在 Net_PushCache 里。
 */

#ifndef __NET_H__
#define __NET_H__

#include "hi_types_base.h"

/** 连接 Wi-Fi 并启动 DHCP；返回 0 表示联网成功 */
int  Net_Init(void);

/** 当前是否已连上 Wi-Fi 且拿到 IP */
int  Net_IsOnline(void);

/** 发送 JSON；返回 0 表示网关返回 2xx */
int  Net_PostJson(const char *path, const char *body);

/**
 * GET 一个 JSON（下行确认用：问网关“我的告警还有人没处理吗”）。
 * 返回 0 表示成功，响应体拷到 resp（含结尾 '\0'，超长截断）。
 */
int  Net_GetJson(const char *path, char *resp, int resp_size);

/** 断网时把报文塞进环形缓存（满了丢最旧的一条） */
void Net_PushCache(const char *body);

/** 网络恢复后按顺序补传；返回实际补传成功条数 */
int  Net_FlushCache(void);

#endif /* __NET_H__ */
