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

#include "mqtt_recv_task.h"
#include "hal_bsp_mqtt.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_structAll.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cJSON.h"
#include "cmsis_os2.h"
#include "sys_config.h"

#define MQTT_RECV_TASK_TIME (200 * 1000) // us

int get_jsonData_value(const cJSON *const object, uint8_t *value)
{
    cJSON *json_value = NULL;
    int ret = -1;
    json_value = cJSON_GetObjectItem(object, "value");
    if (json_value) {
        if (!strcmp(json_value->valuestring, "ON")) {
            *value = 1;
            json_value = NULL;
            ret = 0; // 0为成功
        } else if (!strcmp(json_value->valuestring, "OFF")) {
            *value = 0;
            json_value = NULL;
            ret = 0;
        }
    }
    json_value = NULL;
    return ret; // -1为失败
}

// 解析JSON数据
uint8_t cJSON_Parse_Payload(uint8_t *payload)
{
    uint8_t ret = 0;
    cJSON *root = cJSON_Parse((const char *)payload);
    cJSON *service_id = cJSON_GetObjectItem(root, "service_id");
    cJSON *command_name = cJSON_GetObjectItem(root, "command_name");
    cJSON *paras = cJSON_GetObjectItem(root, "paras");
    if (paras && root && service_id && command_name && !strcmp(service_id->valuestring, "control")) {
        if (!strcmp(command_name->valuestring, "lamp")) {
            // 灯的手动控制
            get_jsonData_value(paras, &sys_msg_data.Lamp_Status);
            sys_msg_data.Lamp_Status = ((sys_msg_data.Lamp_Status) ? SUN_LIGHT_MODE : OFF_LAMP);
        }

        if (!strcmp(command_name->valuestring, "RGB")) {
            // RGB灯的颜色控制
            cJSON *red = cJSON_GetObjectItem(paras, "red");
            cJSON *green = cJSON_GetObjectItem(paras, "green");
            cJSON *blue = cJSON_GetObjectItem(paras, "blue");
            sys_msg_data.RGB_Value.red = red->valueint;
            sys_msg_data.RGB_Value.green = green->valueint;
            sys_msg_data.RGB_Value.blue = blue->valueint;
            sys_msg_data.Lamp_Status = SET_RGB_MODE;
            red = green = blue = NULL;
        }

        if (!strcmp(command_name->valuestring, "led_light")) {
            // 手动调节亮度
            cJSON *value = cJSON_GetObjectItem(paras, "value");
            sys_msg_data.led_light_value = value->valueint;
            value = NULL;
        }

        /* 下面是自动控制的标志位 */
        if (!strcmp(command_name->valuestring, "is_auto_light_mode")) {
            // 是否开启自动亮度调节
            get_jsonData_value(paras, &sys_msg_data.is_auto_light_mode);
        }

        if (!strcmp(command_name->valuestring, "is_sleep_mode")) {
            // 是否开启睡眠模式
            get_jsonData_value(paras, &sys_msg_data.Lamp_Status);
            sys_msg_data.Lamp_Status = ((sys_msg_data.Lamp_Status) ? SLEEP_MODE : OFF_LAMP);
        }

        if (!strcmp(command_name->valuestring, "is_readbook_mode")) {
            // 是否开启阅读模式
            get_jsonData_value(paras, &sys_msg_data.Lamp_Status);
            sys_msg_data.Lamp_Status = ((sys_msg_data.Lamp_Status) ? READ_BOOK_MODE : OFF_LAMP);
        }

        if (!strcmp(command_name->valuestring, "is_blink_mode")) {
            // 是否开启闪烁模式
            get_jsonData_value(paras, &sys_msg_data.Lamp_Status);
            sys_msg_data.Lamp_Status = ((sys_msg_data.Lamp_Status) ? LED_BLINK_MODE : OFF_LAMP);
        }
    }
    cJSON_Delete(root);
    root = NULL;
    service_id = NULL;
    command_name = NULL;
    paras = NULL;
    return 0;
}
// 向云端发送返回值
void send_cloud_request_code(const char *request_id, int ret_code, int request_len)
{
    char *request_topic = (char *)malloc(strlen(MALLOC_MQTT_TOPIC_PUB_COMMANDS_REQ) +
                                         strlen(DEVICE_ID) + request_len + 1);
    if (request_topic != NULL) {
        memset_s(request_topic,
                 strlen(DEVICE_ID) + strlen(MALLOC_MQTT_TOPIC_PUB_COMMANDS_REQ) + request_len + 1,
                 0,
                 strlen(DEVICE_ID) + strlen(MALLOC_MQTT_TOPIC_PUB_COMMANDS_REQ) + request_len + 1);
        if (sprintf_s(request_topic,
                      strlen(DEVICE_ID) + strlen(MALLOC_MQTT_TOPIC_PUB_COMMANDS_REQ) + request_len + 1,
                      MQTT_TOPIC_PUB_COMMANDS_REQ, DEVICE_ID, request_id) > 0) {
            if (ret_code == 0) {
                MQTTClient_pub(request_topic, "{\"result_code\":0}", strlen("{\"result_code\":0}"));
            } else if (ret_code == 1) {
                MQTTClient_pub(request_topic, "{\"result_code\":1}", strlen("{\"result_code\":1}"));
            }
        }
        free(request_topic);
        request_topic = NULL;
    }
}
/**
 * @brief MQTT接收数据的回调函数
 */
int8_t mqttClient_sub_callback(unsigned char *topic, unsigned char *payload)
{
    if ((topic == NULL) || (payload == NULL)) {
        return -1;
    } else {
        printf("topic: %s\r\n", topic);
        printf("payload: %s\r\n", payload);

        // 提取出topic中的request_id
        char request_id[50] = {0};
        int ret_code = 1; // 0为成功, 其余为失败。不带默认表示成功
        if (strcpy_s(request_id, sizeof(request_id),
                     topic + strlen(DEVICE_ID) + strlen("$oc/devices//sys/commands/request_id=")) == 0) {
            printf("request_id: %s\r\n", request_id);
            // 解析JSON数据并控制
            ret_code = cJSON_Parse_Payload(payload);
            send_cloud_request_code(request_id, ret_code, sizeof(request_id));
        }
    }
    return 0;
}

/**
 * @brief MQTT  接收消息任务
 */
void mqtt_recv_task(void)
{
    while (1) {
        MQTTClient_sub();
        usleep(MQTT_RECV_TASK_TIME);
    }
}
