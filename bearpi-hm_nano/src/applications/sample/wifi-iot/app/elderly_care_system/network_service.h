/*
 * 河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
 * 独居老人居家安全监测与应急辅助系统
 * 无线网络传输管理与边缘离线缓存对账服务
 */

#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化无线网络服务（启动后台 Wi-Fi 连接、DHCP 与自动重连任务）
 * @return 0 成功，负数失败
 */
int NetworkServiceInit(void);

/**
 * @brief 检查无线网络当前是否就绪（已连接 AP 且已成功分配 IP）
 * @return true 已就绪，false 离线/未连接
 */
bool NetworkServiceIsReady(void);

/**
 * @brief 发送遥测数据报文（在线时通过 UDP 发往网关；离线时自动存入 RAM 环形缓存）
 * @param json_data 待发送的 JSON 格式遥测数据
 * @return 0 成功（或成功存入缓存），负数失败
 */
int NetworkServiceSendTelemetry(const char *json_data);

/**
 * @brief 主动将遥测数据存入离线环形缓存队列
 * @param json_data JSON 数据
 */
void NetworkServicePushCache(const char *json_data);

/**
 * @brief 网络恢复后自动触发重发积压的离线缓存数据
 * @return 本次成功补传的帧数
 */
int NetworkServiceFlushCache(void);

/**
 * @brief 获取当前离线缓存队列中的积压帧数
 * @return 积压条数
 */
int NetworkServiceGetCacheCount(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_SERVICE_H
