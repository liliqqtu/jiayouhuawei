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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hal_bsp_wifi.h"
#include "hal_bsp_mqtt.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_structAll.h"

#include "cJSON.h"

// 设备密码 fs12345678
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

#define TASK_STACK_SIZE (1024 * 10)
#define MsgQueueObjectNumber 16 // 定义消息队列对象的个数
typedef struct message_sensorData {
    uint8_t led;        // LED灯当前的状态
    uint8_t fan;        // 风扇当前的状态
    uint8_t buzzer;     // 蜂鸣器当前的状态
    uint16_t light;     // 光照强度
    uint16_t proximity; // 接近传感器的数值
    uint16_t infrared;  // 人体红外传感器的数值
    float temperature;  // 当前的温度值
    float humidity;     // 当前的湿度值
} msg_sensorData_t;
msg_sensorData_t sensorData = {0}; // 传感器的数据
osThreadId_t mqtt_send_task_id; // mqtt 发布数据任务ID
osThreadId_t mqtt_recv_task_id; // mqtt 接收数据任务ID
#define MQTT_SEND_TASK_TIME 3 // s
#define MQTT_RECV_TASK_TIME 1 // s
#define TASK_INIT_TIME 2 // s
#define DISPLAY_BUFF_MAX 64
#define MQTT_DATA_MAX 256
uint8_t publish_topic[MQTT_DATA_MAX] = {0};
uint8_t mqtt_data[MQTT_DATA_MAX] = {0};
uint8_t displayBuffer[DISPLAY_BUFF_MAX] = {0};

/**
 * @brief 组JSON数据
 */
int Packaged_json_data(void)
{
    cJSON *root = NULL, *array = NULL, *services = NULL;
    cJSON *properties = NULL;
    int ret = 0;

    // 组JSON数据
    root = cJSON_CreateObject(); // 创建一个对象
    services = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "services", services);
    array = cJSON_CreateObject();
    cJSON_AddStringToObject(array, "service_id", "attribute");
    properties = cJSON_CreateObject();
    cJSON_AddItemToObject(array, "properties", properties);
    cJSON_AddStringToObject(properties, "buzzer", sensorData.buzzer ? "ON" : "OFF");
    cJSON_AddStringToObject(properties, "fan", sensorData.fan ? "ON" : "OFF");
    cJSON_AddStringToObject(properties, "led", sensorData.led ? "ON" : "OFF");
    cJSON_AddNumberToObject(properties, "humidity", (int)sensorData.humidity);
    cJSON_AddNumberToObject(properties, "temperature", (int)sensorData.temperature);
    cJSON_AddNumberToObject(properties, "light", sensorData.light);
    cJSON_AddNumberToObject(properties, "proximity", sensorData.proximity);
    cJSON_AddNumberToObject(properties, "infrared", sensorData.infrared);
    cJSON_AddItemToArray(services, array);  // 将对象添加到数组中

    /* 格式化打印创建的带数组的JSON对象 */
    char *str_print = cJSON_PrintUnformatted(root);
    if (str_print != NULL) {
        // printf("%s\n", str_print);
        if (strcpy_s(mqtt_data, strlen(str_print) + 1, str_print) == 0) {
            ret = 0;
        } else {
            ret = -1;
        }
        cJSON_free(str_print);
    } else {
        ret = -1;
    }
    if (root != NULL) {
        cJSON_Delete(root);
    } else {
        ret = -1;
    }
    properties = str_print = root = array = services = NULL;

    return ret;
}

/**
 * @brief MQTT  发布消息任务
 */
void mqtt_send_task(void)
{
    while (1) {
        // 获取传感器的数据
        SHT20_ReadData(&sensorData.temperature, &sensorData.humidity);
        AP3216C_ReadData(&sensorData.infrared, &sensorData.proximity, &sensorData.light);

        memset_s(displayBuffer, DISPLAY_BUFF_MAX, 0, DISPLAY_BUFF_MAX);
        if (sprintf_s((char *)displayBuffer, DISPLAY_BUFF_MAX, "T:%.1fC H:%.1f%%",
                      sensorData.temperature, sensorData.humidity) > 0) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, displayBuffer, TEXT_SIZE_16);
        }

        memset_s(displayBuffer, DISPLAY_BUFF_MAX, 0, DISPLAY_BUFF_MAX);
        if (sprintf_s((char *)displayBuffer, DISPLAY_BUFF_MAX, "ir:%04d", sensorData.infrared) > 0) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, displayBuffer, TEXT_SIZE_16);
        }

        memset_s(displayBuffer, DISPLAY_BUFF_MAX, 0, DISPLAY_BUFF_MAX);
        if (sprintf_s((char *)displayBuffer, DISPLAY_BUFF_MAX, "ps:%04d", sensorData.proximity) > 0) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, displayBuffer, TEXT_SIZE_16);
        }

        memset_s(displayBuffer, DISPLAY_BUFF_MAX, 0, DISPLAY_BUFF_MAX);
        if (sprintf_s((char *)displayBuffer, DISPLAY_BUFF_MAX, "Lux:%04d", sensorData.light) > 0) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, displayBuffer, TEXT_SIZE_16);
        }
        // 组Topic
        memset_s(publish_topic, MQTT_DATA_MAX, 0, MQTT_DATA_MAX);
        if (sprintf_s(publish_topic, MQTT_DATA_MAX, MQTT_TOPIC_PUB_PROPERTIES, DEVICE_ID) > 0) {
            // 组JSON数据
            Packaged_json_data();
            // 发布消息
            MQTTClient_pub(publish_topic, mqtt_data, strlen((char *)mqtt_data));
        }
    }
    sleep(MQTT_SEND_TASK_TIME);
}

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

/**
 * @brief 解析JSON数据
 */
int Parsing_json_data(const char *payload)
{
    cJSON *root = NULL, *command_name = NULL, *paras = NULL, *value = NULL;
    cJSON *red = NULL, *green = NULL, *blue = NULL;
    int ret_code = 1;
    root = cJSON_Parse((const char *)payload);
    if (root) {
        // 解析JSON数据
        command_name = cJSON_GetObjectItem(root, "command_name");
        paras = cJSON_GetObjectItem(root, "paras");
        if (command_name) {
            if (!strcmp(command_name->valuestring, "led")) {
                ret_code = get_jsonData_value(paras, &sensorData.led);
            } else if (!strcmp(command_name->valuestring, "fan")) {
                ret_code = get_jsonData_value(paras, &sensorData.fan);
            } else if (!strcmp(command_name->valuestring, "buzzer")) {
                ret_code = get_jsonData_value(paras, &sensorData.buzzer);
            } else if (!strcmp(command_name->valuestring, "RGB")) {
                red = cJSON_GetObjectItem(paras, "red");
                green = cJSON_GetObjectItem(paras, "green");
                blue = cJSON_GetObjectItem(paras, "blue");
                AW2013_Control_RGB(red->valueint, green->valueint, blue->valueint);
                ret_code = 0; // 0为成功
            }
        }
    }
    cJSON_Delete(root);
    root = command_name = paras = value = red = green = blue = NULL;
    (sensorData.led == 1) ? set_led(true) : set_led(false);
    (sensorData.fan == 1) ? set_fan(true) : set_fan(false);
    (sensorData.buzzer == 1) ? set_buzzer(true) : set_buzzer(false);
    return ret_code;
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
        int ret_code = 1; // 1为失败
        if (0 == strcpy_s(request_id, sizeof(request_id),
                          topic + strlen(DEVICE_ID) + strlen("$oc/devices//sys/commands/request_id="))) {
            printf("request_id: %s\r\n", request_id);
            // 解析JSON数据
            ret_code = Parsing_json_data(payload);
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
        sleep(MQTT_RECV_TASK_TIME);
    }
}

static void network_wifi_mqtt_example(void)
{
    // 外设的初始化
    PCF8574_Init();
    AW2013_Init(); // 三色LED灯的初始化
    AW2013_Control_Red(0);
    AW2013_Control_Green(0);
    AW2013_Control_Blue(0);
    SHT20_Init();   // SHT20初始化
    AP3216C_Init(); // 三合一传感器初始化
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏

    p_MQTTClient_sub_callback = &mqttClient_sub_callback;

    // 连接WiFi
    if (WiFi_connectHotspots("AI_DEV", "HQYJ12345678") != WIFI_SUCCESS) {
        printf("[error] connectWiFiHotspots\r\n");
    }
    sleep(TASK_INIT_TIME);

    // 连接MQTT服务器
    if (MQTTClient_connectServer(SERVER_IP_ADDR, SERVER_IP_PORT) != WIFI_SUCCESS) {
        printf("[error] mqttClient_connectServer\r\n");
    }
    sleep(TASK_INIT_TIME);

    // 初始化MQTT客户端
    if (MQTTClient_init(MQTT_CLIENT_ID, MQTT_USER_NAME, MQTT_PASS_WORD) != WIFI_SUCCESS) {
        printf("[error] mqttClient_init\r\n");
    }
    sleep(TASK_INIT_TIME);

    // 订阅主题
    if (MQTTClient_subscribe(MQTT_TOPIC_SUB_COMMANDS) != WIFI_SUCCESS) {
        printf("[error] mqttClient_subscribe\r\n");
    }
    sleep(TASK_INIT_TIME);

    //  创建线程
    osThreadAttr_t options;
    options.name = "mqtt_send_task";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal;
    mqtt_send_task_id = osThreadNew((osThreadFunc_t)mqtt_send_task, NULL, &options);
    if (mqtt_send_task_id != NULL) {
        printf("ID = %d, Create mqtt_send_task_id is OK!\r\n", mqtt_send_task_id);
    }

    options.name = "mqtt_recv_task";
    options.stack_size = TASK_STACK_SIZE;
    mqtt_recv_task_id = osThreadNew((osThreadFunc_t)mqtt_recv_task, NULL, &options);
    if (mqtt_recv_task_id != NULL) {
        printf("ID = %d, Create mqtt_recv_task_id is OK!\r\n", mqtt_recv_task_id);
    }
}
SYS_RUN(network_wifi_mqtt_example);
