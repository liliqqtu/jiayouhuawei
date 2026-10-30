/*
 * 系统配置与引脚定义
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

// 引脚分配 (BearPi-HM Nano 与 E53_IS1)
#define PIN_INFRARED            7    // 人体红外 (GPIO_7)
#define PIN_BEEPER              8    // 蜂鸣器 (GPIO_8, PWM1)
#define PWM_PORT_BEEPER         1
#define PIN_DOOR_REED           13   // 门磁干簧管 (GPIO_13, 上拉输入, 0:关门 1:开门)
#define PIN_KEY_SOS             11   // F1 紧急求助 (GPIO_11, 按下接地)
#define PIN_KEY_RESET           12   // F2 复位按键 (GPIO_12, 按下接地)
#define PIN_LED_SYS             2    // 板载LED (GPIO_2)
#define PIN_I2C1_SDA            0    // SHT30 SDA
#define PIN_I2C1_SCL            1    // SHT30 SCL

// 告警阈值
#define DEFAULT_NO_MOTION_TIMEOUT_SEC     30     // 连续无活动超时(秒)，测试设为30秒
#define DEFAULT_DOOR_OPEN_TIMEOUT_SEC     15     // 门未关超时(秒)
#define DEFAULT_TEMP_HIGH_ALERT           38.0f  // 高温报警阈值
#define DEFAULT_TEMP_LOW_ALERT            10.0f  // 低温报警阈值

// 告警等级
typedef enum {
    ALERT_LEVEL_NORMAL = 0,     // 正常
    ALERT_LEVEL_INFO,           // 提示
    ALERT_LEVEL_WARNING,        // 预警
    ALERT_LEVEL_CRITICAL        // 严重
} alert_level_t;

// 告警类型
typedef enum {
    ALERT_TYPE_NONE = 0,
    ALERT_TYPE_SOS,             // SOS求助
    ALERT_TYPE_NO_MOTION,       // 长时间无人移动
    ALERT_TYPE_DOOR_TIMEOUT,    // 门未关
    ALERT_TYPE_TEMP_HIGH,       // 高温
    ALERT_TYPE_TEMP_LOW         // 低温
} alert_type_t;

// 传感器状态数据
typedef struct {
    // 红外
    uint8_t  motion_active;              // 1:有人活动 0:无人
    uint32_t seconds_since_last_motion;  // 距上次活动秒数
    uint32_t motion_events_today;        // 今日累计次数

    // 门磁
    uint8_t  door_open;                  // 1:开 0:关
    uint32_t door_open_duration_sec;     // 门已开秒数

    // 求助按键
    uint8_t  sos_active;                 // 1:求助中 0:正常

    // 温湿度
    float    temperature;                // 温度
    float    humidity;                   // 湿度

    alert_level_t current_level;         // 当前等级
    alert_type_t  current_alert;         // 当前告警原因
    uint32_t      uptime_seconds;        // 开机运行时间(秒)
} elderly_care_data_t;

// Wi-Fi与网关连接参数
#define CFG_WIFI_SSID               "xin Pro"
#define CFG_WIFI_PASSWORD           "kdxbryvq35uxd4r"
#define CFG_GATEWAY_IP              "10.179.139.87"
#define CFG_UDP_PORT                9999

// 离线缓存
#define CFG_CACHE_DEPTH             24
#define CFG_CACHE_ITEM_SIZE         256

#ifdef __cplusplus
}
#endif

#endif // ELDERLY_CARE_CONFIG_H
