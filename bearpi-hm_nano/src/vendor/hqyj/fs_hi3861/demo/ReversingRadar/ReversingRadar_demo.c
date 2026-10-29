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
#include "cJSON.h"
#include "sys_config.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_uart.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_ssd1306_bmps.h"
#include "hal_bsp_pcf8574.h"
#include "oled_show_log.h"

#define TASK_STACK_SIZE (5 * 1024)
#define BACK_DISTANCE 50 // mm
#define COEFFICIENT_10 10
#define COEFFICIENT_100 100
#define COEFFICIENT_1000 1000

msg_data_t sys_msg_data;
hi_u8 recvBuff[200] = {0};
hi_u8 *pbuff = recvBuff;
osThreadId_t uart_recv_task_id; // 任务 ID
uint8_t rgb_val = 0;

// 数字-百位
margin_t bmp_number_1 = {
    .top = 16 + 8,
    .left = 8,
    .width = 16,
    .hight = 32,
};
// 数字-十位
margin_t bmp_number_2 = {
    .top = 16 + 8,
    .left = 24,
    .width = 16,
    .hight = 32,
};
// 数字-个位
margin_t bmp_number_3 = {
    .top = 16 + 8,
    .left = 40,
    .width = 16,
    .hight = 32,
};
// 小数点
margin_t bmp_dian = {
    .top = 32 + 8,
    .left = 56,
    .width = 16,
    .hight = 16,
};
// 数字-小数位
margin_t bmp_number_4 = {
    .top = 32 + 8,
    .left = 72,
    .width = 8,
    .hight = 16,
};
// 单位
margin_t bmp_danwei = {
    .top = 16 + 8,
    .left = 88,
    .width = 32,
    .hight = 32,
};

/**
 * @brief  串口初始化
 * @note   与STM32单片机之间的串口通信
 * @retval None
 */
void uart_init(void)
{
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
}
/**
 * @brief  显示页面
 * @note
 * @param  val:
 * @retval None
 */
void show_page(uint16_t val)
{
    SSD1306_ShowStr(0, 0, " ReversingRadar", TEXT_SIZE_16);

    if (val <= BACK_DISTANCE) {
        // 5cm
        rgb_val ^= 0xFF;
        AW2013_Control_Red(rgb_val);            // 红灯闪烁
        sys_msg_data.pcf8574_io.bit.p1 ^= 0x01; // 蜂鸣器滴滴响
        PCF8574_Write(sys_msg_data.pcf8574_io.all);
    } else {
        rgb_val = 0;
        AW2013_Control_Red(rgb_val);        // 红灯灭
        sys_msg_data.pcf8574_io.bit.p1 = 1; // 蜂鸣器 关
        PCF8574_Write(sys_msg_data.pcf8574_io.all);
    }

    // 显示数字
    uint8_t x = val / COEFFICIENT_1000; // 显示数字的千位
    SSD1306_DrawBMP(bmp_number_1.left, bmp_number_1.top, bmp_number_1.width, bmp_number_1.hight, bmp_16X32_number[x]);

    x = val / COEFFICIENT_100 % COEFFICIENT_10; // 显示数字的百位
    SSD1306_DrawBMP(bmp_number_2.left, bmp_number_2.top, bmp_number_2.width, bmp_number_2.hight, bmp_16X32_number[x]);

    x = val / COEFFICIENT_10 % COEFFICIENT_10; // 显示数字的十位
    SSD1306_DrawBMP(bmp_number_3.left, bmp_number_3.top, bmp_number_3.width, bmp_number_3.hight, bmp_16X32_number[x]);

    x = val % COEFFICIENT_10; // 显示数字的个位
    SSD1306_DrawBMP(bmp_number_4.left, bmp_number_4.top, bmp_number_4.width, bmp_number_4.hight, bmp_8X16_number[x]);

    // 显示小数点
    SSD1306_DrawBMP(bmp_dian.left, bmp_dian.top, bmp_dian.width, bmp_dian.hight, bmp_16X16_dian);
    // 显示cm
    SSD1306_DrawBMP(bmp_danwei.left, bmp_danwei.top, bmp_danwei.width, bmp_danwei.hight, bmp_32X32_cm);
}
/**
 * @brief  解析JSON数据包
 * @note
 * @param  *pstr:
 * @retval None
 */
void parse_json_data(void)
{
    cJSON *json_root = cJSON_Parse((const char *)recvBuff);
    if (json_root) {
        printf("json_root");
        cJSON *json_status = cJSON_GetObjectItem(json_root, "status");
        if (json_status) {
            printf("json_status");
            cJSON *json_distance = cJSON_GetObjectItem(json_status, "distance");
            if (json_distance) {
                sys_msg_data.distance = json_distance->valueint;
                show_page(sys_msg_data.distance); // 显示页面
            }
            json_distance = NULL;
        }
        json_status = NULL;
    }
    cJSON_Delete(json_root);
    json_root = NULL;
}

void uart_recv_task(void)
{
    hi_u8 uart_buff[20] = {0};
    hi_u8 last_len = 0;
    uart_init(); // 串口2初始化
    while (1) {
        // 阻塞接收
        if (memset_s((char *)uart_buff, sizeof(recvBuff), 0, sizeof(uart_buff)) == 0) {
            hi_u32 len = hi_uart_read(HI_UART_IDX_2, uart_buff, sizeof(uart_buff));
            if (len > 0) {
                memcpy_s((char *)pbuff, len, (char *)uart_buff, len);
                pbuff += len;
                if (len < last_len) {
                    pbuff = recvBuff; // 回到recvBuff的头位置
                    parse_json_data();
                    memset_s((char *)recvBuff, sizeof(recvBuff), 0, sizeof(recvBuff));
                }
                last_len = len;
            }
        }
    }
}
static void ReversingRadar_main(void)
{
    printf("Enter ReversingRadar_main()!\r\n");

    // 外设的初始化
    PCF8574_Init();
    AW2013_Init(); // AW2013初始化
    AW2013_Control_RGB(0, 0, 0);
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, " ReversingRadar", TEXT_SIZE_16);

    //  创建线程
    osThreadAttr_t options;
    options.name = "uart_recv_task";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal1;
    uart_recv_task_id = osThreadNew((osThreadFunc_t)uart_recv_task, NULL, &options);
    if (uart_recv_task_id != NULL) {
        printf("ID = %d, Create uart_recv_task_id is OK!\r\n", uart_recv_task_id);
    }
}

SYS_RUN(ReversingRadar_main);
