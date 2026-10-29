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
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_structAll.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_uart.h"
#include "cJSON.h"

typedef struct message_sensorData {
    uint8_t led;    // LED灯当前的状态
    uint8_t fan;    // 风扇当前的状态
    uint8_t buzzer; // 蜂鸣器当前的状态

    uint16_t light;     // 光照强度
    uint16_t proximity; // 接近传感器的数值
    uint16_t infrared;  // 人体红外传感器的数值

    uint16_t distance;       // 超声波传感器的值
    uint16_t batteryVoltage; // 电池电压
    uint16_t enc_l, enc_r;   // 左电机和右电机编码器的值

    float temperature; // 当前的温度值
    float humidity;    // 当前的湿度值
} msg_sensorData_t;
msg_sensorData_t sensorData = {0}; // 传感器的数据

#define PAGE_COEFFICIENT 2
uint8_t recvBuff[200] = {0};
uint8_t *pbuff = recvBuff;

hi_gpio_value key_val;
uint8_t page_num = 2, last_page_num = 0;
uint8_t rgb_value = 0xFF;
#define RGB_ON 255
#define RGB_OFF 0
#define KEY HI_IO_NAME_GPIO_14 // WiFi模组的IO14引脚
osThreadId_t task01_id;
#define TASK01_STACK_SIZE (1024 * 10)
osThreadId_t task02_id;
#define TASK02_STACK_SIZE (1024 * 5)
#define TASK1_DELAY_TIME (200 * 1000) // us
uint8_t displayBuffer[50] = {0};
/**
 * @brief 按键中断回调函数
 * @note   当按键按下的时候才会触发
 * @param  *arg:
 * @retval None
 */
hi_void gpio_callback(hi_void *arg)
{
    page_num++;
}
void page01(void)
{
    SHT20_ReadData(&sensorData.temperature, &sensorData.humidity);
    AP3216C_ReadData(&sensorData.infrared, &sensorData.light, &sensorData.proximity);
    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer),
                  "T:%.1fC H:%.1f%%", sensorData.temperature, sensorData.humidity) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, displayBuffer, TEXT_SIZE_16);
    }

    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "ir:%04d", sensorData.infrared) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, displayBuffer, TEXT_SIZE_16);
    }

    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "ps:%04d", sensorData.proximity) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, displayBuffer, TEXT_SIZE_16);
    }

    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "Lux:%04d", sensorData.light) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, displayBuffer, TEXT_SIZE_16);
    }
    sensorData.led ^= 0x01;
    sensorData.led ? set_led(true) : set_led(false);
    sensorData.fan ^= 0x01;
    sensorData.fan ? set_fan(true) : set_fan(false);
    sensorData.buzzer ^= 0x01;
    sensorData.buzzer ? set_buzzer(true) : set_buzzer(false);
    rgb_value ^= 0xFF;
    AW2013_Control_RGB(rgb_value, rgb_value, rgb_value);
}
void page02(void)
{
    set_led(false);
    set_buzzer(true);
    set_fan(true);
    AW2013_Control_RGB(0, 0, 0);

    /* 显示电池电压 距离 左轮 右轮 */
    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "L_ENC: %05d", sensorData.enc_l) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, displayBuffer, TEXT_SIZE_16);
    }
    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "R_ENC: %05d", sensorData.enc_r) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, displayBuffer, TEXT_SIZE_16);
    }
    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "DC: %05dmV", sensorData.batteryVoltage) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, displayBuffer, TEXT_SIZE_16);
    }
    memset_s(displayBuffer, sizeof(displayBuffer), 0, sizeof(displayBuffer));
    if (sprintf_s((char *)displayBuffer, sizeof(displayBuffer), "Dis: %05dmm", sensorData.distance) > 0) {
        SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, displayBuffer, TEXT_SIZE_16);
    }
}
void task01(void)
{
    while (1) {
        if (last_page_num != page_num) {
            SSD1306_CLS(); // 清屏
        }
        last_page_num = page_num;

        if (!(page_num % PAGE_COEFFICIENT)) { // 第一个页面
            page01();
        }

        if (page_num % PAGE_COEFFICIENT) { // 第二个页面
            page02();
        }
        usleep(TASK1_DELAY_TIME); // 200ms
    }
}
/**
 * @brief  解析JSON数据包
 * @note
 * @param  *pstr:
 * @retval None
 */
void parse_json_data(uint8_t *pstr)
{
    cJSON *json_root = cJSON_Parse((const char *)pstr);
    if (json_root) {
        cJSON *json_status = cJSON_GetObjectItem(json_root, "status");
        if (json_status) {
            cJSON *json_distance = cJSON_GetObjectItem(json_status, "distance");
            if (json_distance) {
                sensorData.distance = json_distance->valueint;
            }
            json_distance = NULL;

            cJSON *json_carPower = cJSON_GetObjectItem(json_status, "carPower");
            if (json_carPower) {
                sensorData.batteryVoltage = json_carPower->valueint;
            }
            json_carPower = NULL;

            cJSON *json_L_speed = cJSON_GetObjectItem(json_status, "L_speed");
            if (json_L_speed) {
                sensorData.enc_l = json_L_speed->valueint;
            }
            json_L_speed = NULL;

            cJSON *json_R_speed = cJSON_GetObjectItem(json_status, "R_speed");
            if (json_R_speed) {
                sensorData.enc_r = json_R_speed->valueint;
            }
            json_R_speed = NULL;
        }
        json_status = NULL;
    }
    cJSON_Delete(json_root);
    json_root = NULL;
}
void task02(void)
{
    hi_u8 uart_buff[20] = {0};
    hi_u8 last_len = 0;
    while (1) {
        // 阻塞接收
        hi_u32 len = hi_uart_read(HI_UART_IDX_2, uart_buff, sizeof(uart_buff));
        if (len > 0) {
            // printf("uart_buff: %s\r\n", uart_buff);
            memcpy_s((char *)pbuff, len, (char *)uart_buff, len);
            pbuff += len;
            if (len < last_len) {
                pbuff = recvBuff; // 回到recvBuff的头位置
                printf("buff: %s\r\n", recvBuff);
                parse_json_data(recvBuff);
                memset_s((char *)recvBuff, sizeof(recvBuff), 0, sizeof(recvBuff));
            }
            last_len = len;
            memset_s((char *)uart_buff, sizeof(uart_buff), 0, sizeof(uart_buff));
        }
    }
}

static void test_board_example(void)
{
    // 外设的初始化
    PCF8574_Init();
    AW2013_Init(); // 三色LED灯的初始化
    AW2013_Control_Red(RGB_ON);
    AW2013_Control_Green(RGB_ON);
    AW2013_Control_Blue(RGB_ON);
    SHT20_Init();   // SHT20初始化
    AP3216C_Init(); // 三合一传感器初始化
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏

    uint32_t ret = 0;
    // 初始化串口
    hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_UART2_TXD);
    hi_io_set_func(HI_IO_NAME_GPIO_12, HI_IO_FUNC_GPIO_12_UART2_RXD);
    hi_uart_attribute uart_param = {
        .baud_rate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = 0,
    };
    ret = hi_uart_init(HI_UART_IDX_2, &uart_param, NULL);
    if (ret != HI_ERR_SUCCESS) {
        printf("hi uart init is faild.\r\n");
    }

    hi_gpio_init();                                            // GPIO初始化
    hi_io_set_pull(KEY, HI_IO_PULL_UP);                        // 设置GPIO上拉
    hi_io_set_func(KEY, HI_IO_FUNC_GPIO_14_GPIO);              // 设置IO14为GPIO功能
    hi_gpio_set_dir(KEY, HI_GPIO_DIR_IN);                      // 设置GPIO为输入模式
    hi_gpio_register_isr_function(KEY,                         // KEY按键引脚
                                  HI_INT_TYPE_EDGE,            // 下降沿检测
                                  HI_GPIO_EDGE_FALL_LEVEL_LOW, // 低电平时触发
                                  &gpio_callback,              // 触发后调用的回调函数
                                  NULL);                       // 回调函数的传参值

    //  创建线程
    osThreadAttr_t options;
    options.name = "task01";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK01_STACK_SIZE;
    options.priority = osPriorityNormal;
    task01_id = osThreadNew((osThreadFunc_t)task01, NULL, &options);
    if (task01_id != NULL) {
        printf("ID = %d, Create task01_id is OK!\r\n", task01_id);
    }

    options.name = "Task2";
    options.stack_size = TASK02_STACK_SIZE;
    options.priority = osPriorityNormal1;
    task02_id = osThreadNew((osThreadFunc_t)task02, NULL, &options);
    if (task02_id != NULL) {
        printf("ID = %d, Create task02_id is OK!\r\n", task02_id);
    }
}
SYS_RUN(test_board_example);
