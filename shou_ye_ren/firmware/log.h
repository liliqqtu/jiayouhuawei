/**
 * @file    log.h
 * @brief   结构化串口日志：版本号 + 时间戳 + 模块 + 级别 + 错误码。
 *
 * 统一格式，方便现场用串口助手过滤，也方便网关“串口桥接模式”解析：
 *   [uptime][ver][module][LEVEL] msg code=N
 *
 * 约定：
 *   - 正常流程用 LOGI，可恢复异常用 LOGW，致命用 LOGE（带错误码）
 *   - 日志里禁止出现医学结论类措辞，只描述信号与状态
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

#include "hi_time.h"
#include "config.h"

#define LOG_TS()  ((unsigned long)(hi_get_milli_seconds() / 1000))

#define LOGI(tag, fmt, ...) \
    printf("[%lu][%s][%s][INFO] " fmt "\r\n", LOG_TS(), CFG_FW_VERSION, tag, ##__VA_ARGS__)

#define LOGW(tag, fmt, ...) \
    printf("[%lu][%s][%s][WARN] " fmt "\r\n", LOG_TS(), CFG_FW_VERSION, tag, ##__VA_ARGS__)

#define LOGE(tag, code, fmt, ...) \
    printf("[%lu][%s][%s][ERR] code=%d " fmt "\r\n", LOG_TS(), CFG_FW_VERSION, tag, (int)(code), ##__VA_ARGS__)

/** 串口 JSON 备用通道的前缀：网关串口桥接靠它识别 */
#define JSON_MARK "#JSON#"

#endif /* __LOG_H__ */
