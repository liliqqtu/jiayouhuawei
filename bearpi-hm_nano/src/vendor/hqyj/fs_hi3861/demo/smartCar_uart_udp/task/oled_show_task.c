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

#include "oled_show_task.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hi_uart.h"
#include "hi_io.h"
#include "hi_gpio.h"

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hal_bsp_wifi.h"
#include "hal_bsp_ssd1306.h"
#include "oled_show_log.h"
#include "sys_config.h"
#include "udp_recv_task.h"

#define COEFFICIENT_1000 1000.0
#define COEFFICIENT_4 4
#define COEFFICIENT_5 5
#define COEFFICIENT_10000 10000
#define MIN_BATTERY_VOL 10000 // mV
#define MIN_DISTANCE_VOL 100 // mm
// OLED显示屏显示任务
uint8_t oledShowBuff[20] = {0};
#define OLED_SHOW_TASK_TIME (50 * 1000) // us
uint8_t led_status = 0;
uint8_t buzzer_status = 0;

// 切换小车的工作状态
char *get_CurrentCarStatus(system_value_t sys)
{
    char *data = NULL;
    switch (sys.car_status) {
        case CAR_STATUS_ON:
            data = "on";
            break;
        case CAR_STATUS_OFF:
            data = "off";
            break;
        case CAR_STATUS_RUN:
            data = "run";
            break;
        case CAR_STATUS_BACK:
            data = "back";
            break;
        case CAR_STATUS_LEFT:
            data = "left";
            break;
        case CAR_STATUS_RIGHT:
            data = "right";
            break;
        case CAR_STATUS_STOP:
            data = "stop";
            break;
        case CAR_STATUS_L_SPEED:
            data = "low";
            break;
        case CAR_STATUS_M_SPEED:
            data = "middle";
            break;
        case CAR_STATUS_H_SPEED:
            data = "high";
            break;
        default:
            data = "    ";
            break;
    }
    
    return data;
}

void oled_show_task(void)
{
    // 在界面第一行显示 IP地址
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, (uint8_t *)WiFi_GetLocalIP(), TEXT_SIZE_16);
    te_car_status_t last_car_status;
    uint8_t t_times = 0;
    uint8_t t_flag = 0;
    while (1) {
        /* 显示小车底盘的电量 */
        memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
        if (sprintf_s((char *)oledShowBuff, sizeof(oledShowBuff), "power: %02.01fV",
                      (float)(systemValue.battery_voltage) / COEFFICIENT_1000) > 0) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_1, oledShowBuff, TEXT_SIZE_16);
        }

        /* 显示小车当前的状态 */
        if (systemValue.car_status != last_car_status) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, "                ", TEXT_SIZE_16);
            memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
            if (sprintf_s((char *)oledShowBuff, sizeof(oledShowBuff), "car: %s",
                          get_CurrentCarStatus(systemValue)) > 0) {
                SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_2, oledShowBuff, TEXT_SIZE_16);
            }
            last_car_status = systemValue.car_status;
        }

        /* 显示小车当前的速度 */
        memset_s((char *)oledShowBuff, sizeof(oledShowBuff), 0, sizeof(oledShowBuff));
        if (sprintf_s((char *)oledShowBuff, sizeof(oledShowBuff), "L: %04d R: %04d", systemValue.left_motor_speed,
                      systemValue.right_motor_speed) > 0) {
            SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_3, oledShowBuff, TEXT_SIZE_16);
        }
        
        /* 车的状态检测 电池电量小于10V时 */
        if (systemValue.battery_voltage <= MIN_BATTERY_VOL) {
        // 10V
            if (!(t_times % COEFFICIENT_4)) {
            // 200ms    蜂鸣器报警
                /* 关闭电机指令 */
                systemValue.car_status = CAR_STATUS_OFF;
                uart_send_buff("{\"control\":{\"power\":\"off\"}}", strlen("{\"control\":{\"power\":\"off\"}}"));
                buzzer_status ^= 0x01;
                buzzer_status ? set_buzzer(true) : set_buzzer(false);
            }
        }

        if ((systemValue.auto_abstacle_flag) && (systemValue.distance <= MIN_DISTANCE_VOL)) {
            /* 停车指令 */
            systemValue.car_status = CAR_STATUS_OFF;
            uart_send_buff("{\"control\":{\"turn\":\"stop\"}}", strlen("{\"control\":{\"turn\":\"stop\"}}"));
            buzzer_status ^= 0x01;
            buzzer_status ? set_buzzer(true) : set_buzzer(false);
        } else {
            set_buzzer(false); // 关闭蜂鸣器
        }

        if (!(t_times % COEFFICIENT_5)) {
            // 500ms    系统状态指示灯
            led_status ^= 0x01;
            led_status ? set_led(true) : set_led(false);
        }
        t_times++;
        usleep(OLED_SHOW_TASK_TIME);
    }
}
