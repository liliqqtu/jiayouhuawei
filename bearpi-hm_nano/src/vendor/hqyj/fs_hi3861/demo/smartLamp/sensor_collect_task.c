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

#include "sensor_collect_task.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_key.h"
#include "hal_bsp_mqtt.h"
#include "oled_show_log.h"
#include "sys_config.h"
#include "cJSON.h"

msg_data_t sys_msg_data = {0}; // 传感器的数据

void publish_lamp_status_data(msg_data_t *msg);
// 按键按下时，改变灯光的模式，三种模式进行切换（白光模式、睡眠模式、看书模式）
static Lamp_Status_t lamp_mode = OFF_LAMP;
static uint8_t t_led_blink_status = 0;

#define RGB_ON 255
#define RGB_OFF 0

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
}ts_rgb_val_t;

// 睡眠模式 RGB灯的PWM值
ts_rgb_val_t sleep_mode_rgb_val = {
    .red = 20,
    .green = 20,
    .blue = 20,
};

// 看书模式 RGB灯的PWM值
ts_rgb_val_t readBook_mode_rgb_val = {
    .red = 226,
    .green = 203,
    .blue = 173,
};

#define TASK_DELAY_TIME (1000) // us
#define KEY_COUNT_TIEMS (10)
#define SENSOR_COUNT_TIMES (50)
#define CHANGE_MODE_COUNT_TIMES (100)
#define LIGHT_SCALE_VAL 4  // 比例值
#define KEY_DELAY_TIME (100 * 1000) // us

// 循环闪烁的节拍值
#define BEAT_0 0
#define BEAT_1 1
#define BEAT_2 2
#define BEAT_3 3

// 处理RGB灯模式控制
void switch_rgb_mode(Lamp_Status_t mode)
{
    switch (mode) {
        case SUN_LIGHT_MODE: // 白光模式
            sys_msg_data.RGB_Value.red = \
            sys_msg_data.RGB_Value.green = \
            sys_msg_data.RGB_Value.blue = RGB_ON;
            break;

        case SLEEP_MODE: // 睡眠模式
            sys_msg_data.RGB_Value.red = sleep_mode_rgb_val.red;
            sys_msg_data.RGB_Value.green = sleep_mode_rgb_val.green;
            sys_msg_data.RGB_Value.blue = sleep_mode_rgb_val.blue;
            break;

        case READ_BOOK_MODE: // 看书模式
            sys_msg_data.RGB_Value.red = readBook_mode_rgb_val.red;
            sys_msg_data.RGB_Value.green = readBook_mode_rgb_val.green;
            sys_msg_data.RGB_Value.blue = readBook_mode_rgb_val.blue;
            break;

        case LED_BLINK_MODE:
            // 闪烁模式
            t_led_blink_status++;

            if (t_led_blink_status == BEAT_1) {
                sys_msg_data.RGB_Value.red = RGB_ON;
                sys_msg_data.RGB_Value.green = sys_msg_data.RGB_Value.blue = RGB_OFF;
            } else if (t_led_blink_status == BEAT_2) {
                sys_msg_data.RGB_Value.green = RGB_ON;
                sys_msg_data.RGB_Value.red = sys_msg_data.RGB_Value.blue = RGB_OFF;
            } else if (t_led_blink_status == BEAT_3) {
                sys_msg_data.RGB_Value.blue = RGB_ON;
                sys_msg_data.RGB_Value.red = sys_msg_data.RGB_Value.green = RGB_OFF;
            } else {
                t_led_blink_status = BEAT_0;
            }
            break;

        case SET_RGB_MODE: // 调色模式
            break;

        default: // 关闭灯光
            lamp_mode = OFF_LAMP;
            sys_msg_data.RGB_Value.red = RGB_OFF;
            sys_msg_data.RGB_Value.green = RGB_OFF;
            sys_msg_data.RGB_Value.blue = RGB_OFF;
            break;
    }

    // 根据光照强度进行自动调节
    if (sys_msg_data.is_auto_light_mode == LIGHT_AUTO_MODE) {
        AW2013_Control_RGB((uint8_t)(sys_msg_data.AP3216C_Value.light / LIGHT_SCALE_VAL),
                           (uint8_t)(sys_msg_data.AP3216C_Value.light / LIGHT_SCALE_VAL),
                           (uint8_t)(sys_msg_data.AP3216C_Value.light / LIGHT_SCALE_VAL));
    } else {
        // 手动调节
        AW2013_Control_RGB((uint8_t)(sys_msg_data.RGB_Value.red * sys_msg_data.led_light_value),
                           (uint8_t)(sys_msg_data.RGB_Value.green * sys_msg_data.led_light_value),
                           (uint8_t)(sys_msg_data.RGB_Value.blue * sys_msg_data.led_light_value));
    }
}

// 处理按键事件
void deal_key_event(void)
{
    if (KEY_Get_Input_Value() == 0) {
        lamp_mode++;
        // 每次按下进行切换灯的模式
        switch_rgb_mode(lamp_mode);
        while (!KEY_Get_Input_Value()) {
            usleep(KEY_DELAY_TIME);
        }
        // 记录灯的状态
        sys_msg_data.Lamp_Status = lamp_mode;
    }
}

/**
 * @brief  传感器采集任务
 * @note
 * @retval None
 */
void sensor_collect_task(void)
{
    uint16_t times = 0;
    Lamp_Status_t last_lamp_status; // 电灯的上一次状态
    while (1) {
        // 检测按键的值
        if (!(times % KEY_COUNT_TIEMS)) {
            if (sys_msg_data.is_auto_light_mode == LIGHT_MANUAL_MODE) {
                deal_key_event();
            }
        }

        // 采集传感器的值
        if (!(times % SENSOR_COUNT_TIMES)) {
            AP3216C_ReadData(&sys_msg_data.AP3216C_Value.infrared,
                             &sys_msg_data.AP3216C_Value.proximity,
                             &sys_msg_data.AP3216C_Value.light);

            // 显示在OLED显示屏上
            uint8_t oled_display_buff[OLED_DISPLAY_BUFF_SIZE] = {0};
            snprintf_s(oled_display_buff, sizeof(oled_display_buff), OLED_DISPLAY_BUFF_SIZE
                       "%04d Lamp:%s",
                       sys_msg_data.AP3216C_Value.light,
                       (sys_msg_data.Lamp_Status == OFF_LAMP) ? "OFF" : " ON");
            oled_show_line_string(OLED_TEXT16_LINE_2, oled_display_buff);
            times = 0;
        }

        // 更改LED灯的工作模式
        if (!(times % CHANGE_MODE_COUNT_TIMES)) {
            switch_rgb_mode(sys_msg_data.Lamp_Status);
        }

        // 检测灯的状态,当灯的状态不一样的时候,上报最新的数据
        if (last_lamp_status != sys_msg_data.Lamp_Status) {
            publish_lamp_status_data(&sys_msg_data);
        }
        last_lamp_status = sys_msg_data.Lamp_Status;
        times++;
        usleep(TASK_DELAY_TIME);
    }
}

/**
 * @brief  发布灯的状态和RGB灯的值
 * @note
 * @param  msg:
 * @retval None
 */
void publish_lamp_status_data(msg_data_t *msg)
{
    char *publish_topic = (char *)malloc(strlen(MALLOC_MQTT_TOPIC_PUB_PROPERTIES) + strlen(DEVICE_ID) + 1);
    if (publish_topic != NULL) {
        // 拼接Topic
        if (memset_s(publish_topic,
                     strlen(MALLOC_MQTT_TOPIC_PUB_PROPERTIES) + strlen(DEVICE_ID) + 1,
                     0, strlen(DEVICE_ID) + strlen(MALLOC_MQTT_TOPIC_PUB_PROPERTIES) + 1) == 0) {
            if (sprintf_s(publish_topic,
                          strlen(MALLOC_MQTT_TOPIC_PUB_PROPERTIES) + strlen(DEVICE_ID) + 1,
                          MQTT_TOPIC_PUB_PROPERTIES, DEVICE_ID) > 0) {
                // 组装JSON数据
                cJSON *json_root = cJSON_CreateObject();
                cJSON *json_services = cJSON_CreateArray();
                cJSON *json_services_root = cJSON_CreateObject();
                cJSON *json_properties = cJSON_CreateObject();

                cJSON_AddItemToObject(json_root, "services", json_services);
                cJSON_AddItemToArray(json_services, json_services_root);
                cJSON_AddItemToObject(json_services_root, "service_id", cJSON_CreateString("base"));
                cJSON_AddItemToObject(json_services_root, "properties", json_properties);
                cJSON_AddItemToObject(json_properties, "lamp",
                                      msg->Lamp_Status ? cJSON_CreateString("ON") : cJSON_CreateString("OFF"));
                cJSON_AddItemToObject(json_properties, "red", cJSON_CreateNumber(msg->RGB_Value.red));
                cJSON_AddItemToObject(json_properties, "green", cJSON_CreateNumber(msg->RGB_Value.green));
                cJSON_AddItemToObject(json_properties, "blue", cJSON_CreateNumber(msg->RGB_Value.blue));

                char *payload = cJSON_PrintUnformatted(json_root);
                // 发布消息
                MQTTClient_pub(publish_topic, payload, strlen((char *)payload));
                cJSON_Delete(json_root);
                free(publish_topic);
                json_root = json_services = json_services_root = json_properties = NULL;
            }
        }
    }
    publish_topic = NULL;
}
