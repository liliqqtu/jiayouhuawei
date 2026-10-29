/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H

#include "cmsis_os2.h"
#include "hal_bsp_structAll.h"

// DeviceSecret fs12345678
// 设备ID
#define DEVICE_ID ""
// MQTT客户端ID
#define MQTT_CLIENT_ID ""
// MQTT用户名
#define MQTT_USER_NAME ""
// MQTT密码
#define MQTT_PASS_WORD ""
// 华为云平台的IP地址
#define SERVER_IP_ADDR ""
// 华为云平台的IP端口号
#define SERVER_IP_PORT 1883
// 订阅 接收控制命令的主题
#define MQTT_TOPIC_SUB_COMMANDS "$oc/devices/%s/sys/commands/#"
// 发布 成功接收到控制命令后的主题
#define MQTT_TOPIC_PUB_COMMANDS_REQ "$oc/devices/%s/sys/commands/response/request_id=%s"
#define MALLOC_MQTT_TOPIC_PUB_COMMANDS_REQ "$oc/devices//sys/commands/response/request_id="
// 发布 设备属性数据的主题
#define MQTT_TOPIC_PUB_PROPERTIES "$oc/devices/%s/sys/properties/report"
#define MALLOC_MQTT_TOPIC_PUB_PROPERTIES "$oc/devices//sys/properties/report"

typedef struct {
    int top;  // 上边距
    int left; // 下边距
    int width; // 宽
    int hight; // 高
} margin_t;   // 边距类型

typedef struct message_data {
    unsigned char fanStatus; // 风扇的状态
    int humidity;          // 湿度值
    int temperature;       // 温度值
    hi_nv_save_sensor_threshold nvFlash;
    tn_pcf8574_io_t pcf8574_io;
} msg_data_t;

extern msg_data_t sys_msg_data; // 传感器的数据

#endif
