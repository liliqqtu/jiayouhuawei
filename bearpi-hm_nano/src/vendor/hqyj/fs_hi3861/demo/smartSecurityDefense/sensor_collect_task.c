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
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_ssd1306_bmps.h"
#include "hal_bsp_key.h"
#include "hal_bsp_mqtt.h"
#include "oled_show_log.h"
#include "sys_config.h"

#define TASK_DELAY_TIME (100 * 1000) // us
#define PS_SENSOR_MAX 500 // 接近传感器(PS)的检测值

margin_t png_left = {
    .top = 16,
    .left = 10,
    .hight = 32,
    .width = 32,
}; // 左侧图片
margin_t png_right = {
    .top = 16,
    .left = 68,
    .hight = 32,
    .width = 32,
}; // 右侧图片

margin_t text_left = {
    .top = 3,
    .left = 18,
    .hight = 32,
    .width = 32,
}; // 左侧图片名称
margin_t text_right = {
    .top = 3,
    .left = 76,
    .hight = 32,
    .width = 32,
}; // 右侧图片名称

/**
 * @brief  显示湿度页面
 * @note
 * @param  val:
 * @retval None
 */
void show_page(void)
{
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, " Smart Security", TEXT_SIZE_16);

    if (sys_msg_data.buzzerStatus != 0) {
        // 蜂鸣器状态 开启
        SSD1306_DrawBMP(png_right.left, png_right.top,
                        png_right.width, png_right.hight, bmp_32X32_BaoJing); // 显示 报警
        SSD1306_ShowStr(text_right.left, text_right.top, "Yes", TEXT_SIZE_16);
    } else {
        // 关闭
        SSD1306_DrawBMP(png_right.left, png_right.top,
                        png_right.width, png_right.hight, bmp_32X32_No_BaoJing); // 显示 不报警状态
        SSD1306_ShowStr(text_right.left, text_right.top, "No ", TEXT_SIZE_16);
    }

    if (sys_msg_data.is_Body != 0) {
        SSD1306_DrawBMP(png_left.left, png_left.top,
                        png_left.width, png_left.hight, bmp_32X32_Body); // 显示 有人
        SSD1306_ShowStr(text_left.left, text_left.top, "Yes", TEXT_SIZE_16);
    } else {
        SSD1306_DrawBMP(png_left.left, png_left.top,
                        png_left.width, png_left.hight, bmp_32X32_No_Body); // 显示 无人
        SSD1306_ShowStr(text_left.left, text_left.top, "No ", TEXT_SIZE_16);
    }
}

/**
 * @brief  传感器采集任务
 * @note
 * @retval None
 */
void sensor_collect_task(void)
{
    while (1) {
        // 采集传感器的值
        uint16_t irData, alsData;
        AP3216C_ReadData(&irData, &alsData, &sys_msg_data.psData);
        // 逻辑判断 当接近传感器检测有物体接近的时候，表示有人靠近
        sys_msg_data.is_Body = (sys_msg_data.psData >= PS_SENSOR_MAX ? 1 : 0);
        // 显示在OLED显示屏上
        show_page();

        // 是否开启自动控制
        if (sys_msg_data.nvFlash.smartControl_flag != 0) {
            if (sys_msg_data.is_Body != 0) {
                // 开启蜂鸣器报警
                set_buzzer(true);
            } else {
                // 关闭蜂鸣器报警
                set_buzzer(false);
            }
        } else {
            if (sys_msg_data.buzzerStatus != 0) {
                set_buzzer(true);
                usleep(TASK_DELAY_TIME);
                set_buzzer(false);
                usleep(TASK_DELAY_TIME);
            } else {
                set_buzzer(false);
            }
        }
        usleep(TASK_DELAY_TIME);
    }
}
