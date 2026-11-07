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
#define PIN_DOOR_REED           10   // 门磁干簧管 (GPIO_10: J1 第 1 孔 SPI_SCK，与烟雾 SF1 零冲突)
#define PIN_KEY_SOS             11   // F1 紧急求助 (GPIO_11, 按下接地)
#define PIN_KEY_RESET           12   // F2 复位按键 (GPIO_12, 按下接地)
#define PIN_WIRELESS_SOS        14   // 433MHz 无线遥控求助按钮 (GPIO_14: J1 第 4 孔 GPIO1，按下闭合接地)
#define PIN_LED_SYS             2    // 板载LED (GPIO_2)
#define PIN_I2C1_SDA            0    // SHT30 SDA
#define PIN_I2C1_SCL            1    // SHT30 SCL

// SYN6288 中文语音合成播报模块
#define PIN_VOICE_TXD           6    // SYN6288 RXD (GPIO_6, UART1_TXD)
#define PIN_VOICE_RXD           5    // SYN6288 TXD/BUSY (GPIO_5, UART1_RXD)
#define UART_VOICE_PORT         1    // UART1
#define DEFAULT_VOICE_VOLUME    12   // 默认音量 (0~16, 12为默认适中偏响，清晰易听)
#define CFG_UDP_CMD_PORT        9998 // 局域网接收控制指令 UDP 端口

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
    ALERT_TYPE_TEMP_LOW,        // 低温
    ALERT_TYPE_SMOKE            // 浓烟与燃气火险
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

    // 求助按键 (支持板载 F1 与 433MHz 无线遥控按钮)
    uint8_t  sos_active;                 // 1:求助中 0:正常
    uint8_t  sos_source;                 // 0:无 1:板载按键 2:无线遥控按钮 3:远程下发

    // 温湿度
    float    temperature;                // 温度
    float    humidity;                   // 湿度

    // 语音音量 (SYN6288)
    uint8_t  voice_volume;               // 当前播报音量 (0~16)

    alert_level_t current_level;         // 当前等级
    alert_type_t  current_alert;         // 当前告警原因
    uint32_t      uptime_seconds;        // 开机运行时间(秒)
} elderly_care_data_t;

// Wi-Fi与网关连接参数
#define CFG_WIFI_SSID               "xin Pro"
#define CFG_WIFI_PASSWORD           "kdxbryvq35uxd4r"
#define CFG_GATEWAY_IP              "10.179.139.87"
#define CFG_UDP_PORT                9999

// 华为云 IoTDA (设备接入服务) 鉴权三元组
#define CFG_OC_SERVER_IP            "117.78.5.125"
#define CFG_OC_SERVER_DOMAIN        "c4d3a2eef8.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define CFG_OC_SERVER_PORT          1883
#define CFG_OC_CLIENT_ID            "6aedfdfe7f2e6c302faaed1e_ElderlyCare_001_0_0_2026110616"
#define CFG_OC_USERNAME             "6aedfdfe7f2e6c302faaed1e_ElderlyCare_001"
#define CFG_OC_PASSWORD             "15310ad6792b5db9d46daf259c00586bd56e5a90dedb87963a3449f57f22458f"
#define CFG_OC_DEVICE_ID            "6aedfdfe7f2e6c302faaed1e_ElderlyCare_001"

// 离线缓存
#define CFG_CACHE_DEPTH             24
#define CFG_CACHE_ITEM_SIZE         256

#ifdef __cplusplus
}
#endif

#endif // ELDERLY_CARE_CONFIG_H
