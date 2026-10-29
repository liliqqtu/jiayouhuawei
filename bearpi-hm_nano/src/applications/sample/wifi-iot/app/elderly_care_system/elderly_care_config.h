/*
 * 河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
 * 独居老人居家安全监测与应急辅助系统
 * 核心系统配置文件与引脚宏定义
 */

#ifndef ELDERLY_CARE_CONFIG_H
#define ELDERLY_CARE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * 硬件引脚分配 (结合 BearPi-HM Nano 与 E53_IS1 接口原理图)
 * ==================================================================== */

// 1. 人体红外传感器 (E53_IS1): GPIO_7, 上升沿检测有人移动
#define PIN_INFRARED            7

// 2. 本地声光报警蜂鸣器 (E53_IS1): GPIO_8 (PWM1)
#define PIN_BEEPER              8
#define PWM_PORT_BEEPER         1

// 3. 门窗磁干簧管 (J1 第 5 脚): GPIO_13, 开内部上拉, 闭合接地(0:关门, 1:开门)
#define PIN_DOOR_REED           13

// 4. 板载紧急求助 SOS 键 (F1 键): GPIO_11, 开内部上拉, 按下接地低电平
#define PIN_KEY_SOS             11

// 5. 板载消警复位按键 (F2 键): GPIO_12, 开内部上拉, 按下接地低电平
#define PIN_KEY_RESET           12

// 6. 板载运行与报警状态 LED (黄色灯): GPIO_2
#define PIN_LED_SYS             2

// 7. I2C 总线引脚 (备选硬件温湿度 SHT30)
#define PIN_I2C1_SDA            0
#define PIN_I2C1_SCL            1


/* ====================================================================
 * 告警逻辑阈值配置 (演示与竞赛优化)
 * ==================================================================== */

// 长时间无活动判定阈值 (为答辩现场快速演示，设为 30 秒；生产环境可配置为 2~4 小时)
#define DEFAULT_NO_MOTION_TIMEOUT_SEC     30

// 门窗长开未关判定阈值 (秒)
#define DEFAULT_DOOR_OPEN_TIMEOUT_SEC     15

// 异常高温告警阈值 (摄氏度)
#define DEFAULT_TEMP_HIGH_ALERT           38.0f

// 异常低温告警阈值 (摄氏度)
#define DEFAULT_TEMP_LOW_ALERT            10.0f


/* ====================================================================
 * 告警类型与枚举
 * ==================================================================== */

typedef enum {
    ALERT_LEVEL_NORMAL = 0,     // 正常安全
    ALERT_LEVEL_INFO,           // 提示状态 (如短暂开门、正常活动)
    ALERT_LEVEL_WARNING,        // 一般预警 (门长开、温湿度偏高)
    ALERT_LEVEL_CRITICAL        // 严重告警 (SOS 求助、跌倒/长时间无人移动)
} alert_level_t;

typedef enum {
    ALERT_TYPE_NONE = 0,
    ALERT_TYPE_SOS,             // 老人按下紧急求助键
    ALERT_TYPE_NO_MOTION,       // 长时间未检测到活动 (疑似跌倒/昏迷)
    ALERT_TYPE_DOOR_TIMEOUT,    // 门窗长开未关异常
    ALERT_TYPE_TEMP_HIGH,       // 室内温度过高 (火警隐患)
    ALERT_TYPE_TEMP_LOW         // 室内温度过低 (采暖异常)
} alert_type_t;


/* ====================================================================
 * 系统实时状态聚合结构体
 * ==================================================================== */

typedef struct {
    // 人体红外状态
    uint8_t  motion_active;              // 0: 当前静止, 1: 正在活动
    uint32_t seconds_since_last_motion;  // 距上次活动已过去多长时间(秒)
    uint32_t motion_events_today;        // 今日累计活动次数

    // 门窗状态
    uint8_t  door_open;                  // 0: 闭合, 1: 打开
    uint32_t door_open_duration_sec;     // 门窗已打开时长(秒)

    // 紧急求助按键
    uint8_t  sos_active;                 // 0: 正常, 1: 处于求助告警中

    // 环境温湿度
    float    temperature;                // 当前室内温度 (℃)
    float    humidity;                   // 当前室内湿度 (%RH)

    // 综合判定
    alert_level_t current_level;         // 当前告警等级
    alert_type_t  current_alert;         // 当前主要告警原因
    uint32_t      uptime_seconds;        // 系统运行总时间(秒)
} elderly_care_data_t;

#ifdef __cplusplus
}
#endif

#endif // ELDERLY_CARE_CONFIG_H
