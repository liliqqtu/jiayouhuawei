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
#include "hal_bsp_sht20.h"
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_ssd1306_bmps.h"

osThreadId_t Task1_ID; //  任务1 ID

typedef struct {
    int top;  // 上边距
    int left; // 左边距
    int hight; // 高
    int width; // 宽
} margin_t;   // 边距类型

/* 标题-温度 */
margin_t temp_title = {
    .top = 0,
    .left = 0,
};
/* 标题-湿度 */
margin_t humi_title = {
    .top = 0,
    .left = 0,
};
/* 数字-十位 */
margin_t number_1 = {
    .top = 16,
    .left = 8,
    .width = 16,
    .hight = 32
};
/* 数字-个位 */
margin_t number_2 = {
    .top = 16,
    .left = 24,
    .width = 16,
    .hight = 32
};
/* 小数点 */
margin_t dian = {
    .top = 32,
    .left = 40,
    .width = 16,
    .hight = 16
};
/* 数字-小数 */
margin_t number_3 = {
    .top = 32,
    .left = 56,
    .width = 8,
    .hight = 16
};
/* 单位 */
margin_t danwei = {
    .top = 16,
    .left = 52,
    .width = 16,
    .hight = 16
};
/* 图片 */
margin_t image = {
    .top = 16,
    .left = 80,
    .width = 48,
    .hight = 48
};

#define TASK_STACK_SIZE (1024 * 5)
#define TASK_DELAY_TIME 3 // s
#define COEFFICIENT_10 10
#define COEFFICIENT_100 100
#define COEFFICIENT_1000 1000
typedef enum {
    TEMP_RANG_0 = 0,
    TEMP_RANG_6 = 6,
    TEMP_RANG_12 = 12,
    TEMP_RANG_18 = 18,
    TEMP_RANG_24 = 24,
}te_temperature_range_t;

typedef enum {
    HUMI_RANG_0 = 0,
    HUMI_RANG_20 = 20,
    HUMI_RANG_40 = 40,
    HUMI_RANG_60 = 60,
    HUMI_RANG_80 = 80,
}te_humidity_range_t;

/**
 * @brief  显示温度页面
 * @note
 * @param  val:
 * @retval None
 */
void show_temp_page(float val)
{
    SSD1306_CLS();
    // 显示标题 中间居中显示
    SSD1306_ShowStr(temp_title.left, temp_title.top, "   Temperature ", TEXT_SIZE_16);

    // 放大100倍后，计算出十位数字
    int x = (val * COEFFICIENT_100) / COEFFICIENT_1000;
    SSD1306_DrawBMP(number_1.left, number_1.top, number_1.width, number_1.hight, bmp_16X32_number[x]); // 显示数字的十位

    // 放大100倍后，计算出个位数字
    x = ((int)(val * COEFFICIENT_100)) / COEFFICIENT_100 % COEFFICIENT_10;
    SSD1306_DrawBMP(number_2.left, number_2.top, number_2.width, number_2.hight, bmp_16X32_number[x]); // 显示数字的个位
    SSD1306_DrawBMP(dian.left, dian.top, dian.width, dian.hight, bmp_16X16_dian);              // 显示小数点
    SSD1306_DrawBMP(danwei.left, danwei.top, danwei.width, danwei.hight, bmp_16X16_sheShiDu);      // 显示℃符号

    // 放大100倍后，计算出小数点位数字
    x = ((int)(val * COEFFICIENT_100)) / COEFFICIENT_10 % COEFFICIENT_10;
    SSD1306_DrawBMP(number_3.left, number_3.top, number_3.width, number_3.hight, bmp_8X16_number[x]); // 数字小数位

    // 适宜温度 0 ~ 30℃
    if (val < TEMP_RANG_0) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_5_ku_qi);
    } else if (val >= TEMP_RANG_0 && val < TEMP_RANG_6) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_5_ku_qi);
    } else if (val >= TEMP_RANG_6 && val < TEMP_RANG_12) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_4_nan_guo);
    } else if (val >= TEMP_RANG_12 && val < TEMP_RANG_18) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_2_wei_xiao);
    } else if (val >= TEMP_RANG_18 && val < TEMP_RANG_24) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_1_mi_yan_xiao);
    } else if (val >= TEMP_RANG_24) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_3_wu_biao_qing);
    }
}

/**
 * @brief  显示湿度页面
 * @note
 * @param  val:
 * @retval None
 */
void show_humi_page(float val)
{
    // 显示标题 中间居中显示
    SSD1306_CLS();
    SSD1306_ShowStr(humi_title.left, humi_title.top, "    Humidity   ", TEXT_SIZE_16);

    int x = (val * COEFFICIENT_100) / COEFFICIENT_1000;
    SSD1306_DrawBMP(number_1.left, number_1.top, number_1.width, number_1.hight, bmp_16X32_number[x]); // 显示数字的十位

    x = ((int)(val * COEFFICIENT_100)) / COEFFICIENT_100 % COEFFICIENT_10;
    SSD1306_DrawBMP(number_2.left, number_2.top, number_2.width, number_2.hight, bmp_16X32_number[x]); // 显示数字的个位
    SSD1306_DrawBMP(dian.left, dian.top, dian.width, dian.hight, bmp_16X16_dian);              // 显示小数点
    SSD1306_DrawBMP(danwei.left, danwei.top, danwei.width, danwei.hight, bmp_16X16_baifenhao);     // 显示%符号

    x = ((int)(val * COEFFICIENT_100)) / COEFFICIENT_10 % COEFFICIENT_10;
    SSD1306_DrawBMP(number_3.left, number_3.top, number_3.width, number_3.hight, bmp_8X16_number[x]); // 数字小数位

    // 范围： 0 ~ 100%
    if (val >= HUMI_RANG_0 && val < HUMI_RANG_20) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_5_ku_qi);
    } else if (val >= HUMI_RANG_20 && val < HUMI_RANG_40) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_4_nan_guo);
    } else if (val >= HUMI_RANG_40 && val < HUMI_RANG_60) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_3_wu_biao_qing);
    } else if (val >= HUMI_RANG_60 && val < HUMI_RANG_80) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_2_wei_xiao);
    } else if (val >= HUMI_RANG_80) {
        SSD1306_DrawBMP(image.left, image.top, image.width, image.hight, bmp_48X48_1_mi_yan_xiao);
    }
}

/**
 * @description: 任务1为低优先级任务
 * @param {*}
 * @return {*}
 */
void Task1(void)
{
    uint8_t x = 0, y = 0;
    while (1) {
        float temp_val, humi_val;
        SHT20_ReadData(&temp_val, &humi_val); // 读取温湿度传感器的值
        printf("temp_val: %.2f   humi_val: %.2f\r\n", temp_val, humi_val);
        show_temp_page(temp_val); // 显示温度页面
        sleep(TASK_DELAY_TIME);
        show_humi_page(humi_val); // 显示湿度页面
        sleep(TASK_DELAY_TIME);
    }
}

static void smartTemp_demo(void)
{
    printf("Enter smartTemp_demo()!");

    PCF8574_Init();                     // 初始化IO扩展芯片
    SHT20_Init();   // SHT20初始化
    SSD1306_Init(); // 初始化OLED
    SSD1306_CLS();  // 清屏

    osThreadAttr_t options;
    options.name = "Task1";              // 任务的名字
    options.attr_bits = 0;               // 属性位
    options.cb_mem = NULL;               // 堆空间地址
    options.cb_size = 0;                 // 堆空间大小
    options.stack_mem = NULL;            // 栈空间地址
    options.stack_size = TASK_STACK_SIZE;       // 栈空间大小 单位:字节
    options.priority = osPriorityNormal; // 任务的优先级

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options); // 创建任务1
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\n", Task1_ID);
    }
}
SYS_RUN(smartTemp_demo);