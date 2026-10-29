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

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hal_bsp_wifi.h"
#include "hal_bsp_mqtt.h"
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_structAll.h"

#define SERVER_IP_ADDR "192.168.3.209"
#define SERVER_IP_PORT 1883
#define MQTT_TOPIC_SUB "subTopic"
#define MQTT_TOPIC_PUB "pubTopic"
#define TASK_STACK_SIZE (1024 * 5)
#define TASK_INIT_TIME 2 // s
#define MQTT_RECV_TASK_TIME (200 * 1000) // us

tn_pcf8574_io_t pcf8574_io = {0}; // IO扩展芯片的引脚
osThreadId_t mqtt_send_task_id;   // mqtt订阅数据任务
osThreadId_t mqtt_recv_task_id;   // mqtt发布数据任务
int8_t mqtt_sub_payload_callback(unsigned char *topic, unsigned char *payload)
{
    printf("[info] topic:[%s]    recv<== %s\r\n", topic, payload);
}
void mqtt_recv_task(void)
{
    while (1) {
        MQTTClient_sub();
        usleep(MQTT_RECV_TASK_TIME);
    }
}
void mqtt_send_task(void)
{
    // 连接Wifi
    if (WiFi_connectHotspots("AI_DEV", "HQYJ12345678") != WIFI_SUCCESS) {
        printf("[error] WiFi_connectHotspots\r\n");
    }

    // 连接MQTT服务器
    if (MQTTClient_connectServer(SERVER_IP_ADDR, SERVER_IP_PORT) != 0) {
        printf("[error] MQTTClient_connectServer\r\n");
    } else {
        printf("[success] MQTTClient_connectServer\r\n");
    }
    sleep(TASK_INIT_TIME);

    // 初始化MQTT客户端
    if (MQTTClient_init("mqtt_client_123", "username", "password") != 0) {
        printf("[error] MQTTClient_init\r\n");
    } else {
        printf("[success] MQTTClient_init\r\n");
    }
    sleep(TASK_INIT_TIME);

    // 订阅Topic
    if (MQTTClient_subscribe(MQTT_TOPIC_SUB) != 0) {
        printf("[error] MQTTClient_subscribe\r\n");
    } else {
        printf("[success] MQTTClient_subscribe\r\n");
    }
    sleep(TASK_INIT_TIME);

    osThreadAttr_t options;
    options.name = "mqtt_recv_task";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal;

    mqtt_recv_task_id = osThreadNew((osThreadFunc_t)mqtt_recv_task, NULL, &options);
    if (mqtt_recv_task_id != NULL) {
        printf("ID = %d, Create mqtt_recv_task_id is OK!\r\n", mqtt_recv_task_id);
    }

    while (1) {
        MQTTClient_pub(MQTT_TOPIC_PUB, "hello world!!!", strlen("hello world!!!"));
        sleep(TASK_INIT_TIME);
    }
}

static void network_wifi_mqtt_example(void)
{
    printf("Enter network_wifi_mqtt_example()!\r\n");

    PCF8574_Init();

    p_MQTTClient_sub_callback = &mqtt_sub_payload_callback;

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
}
SYS_RUN(network_wifi_mqtt_example);