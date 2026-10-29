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

#include "udp_recv_task.h"
#include "udp_send_task.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_uart.h"

#include "sys_config.h"

#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/api_shell.h"

#include "cJSON.h"

char udp_recvBuff[512] = {0};                    // 数据缓冲区
char uart_sendBuff[128] = {0};                   // 发送数据缓冲区
uint16_t L_PWM_Value = 350, R_PWM_Value = 350;   // 默认的PWM参数值
uint16_t base_pwm_speed_value = MOTOR_LOW_SPEED; // 速度倍率

void uart_send_control_status(te_car_status_t cmd)
{
    memset_s(uart_sendBuff, sizeof(uart_sendBuff), 0, sizeof(uart_sendBuff));
    switch (cmd) {
        case CAR_STATUS_ON:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff), "{\"control\":{\"power\":\"on\"}}") > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;
        
        case CAR_STATUS_OFF:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff), "{\"control\":{\"power\":\"off\"}}") > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;
        
        case CAR_STATUS_STOP:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff), "{\"control\":{\"turn\":\"stop\"}}") > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;

        default:
            break;
    }
    uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
}
void uart_send_control_cmd(te_car_status_t cmd)
{
    memset_s(uart_sendBuff, sizeof(uart_sendBuff), 0, sizeof(uart_sendBuff));
    switch (cmd) {
        case CAR_STATUS_RUN:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff),
                          "{\"control\":{\"turn\":\"run\",\"pwm\":{\"L_Motor\":%d,\"R_Motor\":%d}}}",
                          base_pwm_speed_value + L_PWM_Value,
                          base_pwm_speed_value + R_PWM_Value) > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;
        
        case CAR_STATUS_BACK:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff),
                          "{\"control\":{\"turn\":\"back\",\"pwm\":{\"L_Motor\":%d,\"R_Motor\":%d}}}",
                          base_pwm_speed_value + L_PWM_Value,
                          base_pwm_speed_value + R_PWM_Value) > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;
        
        case CAR_STATUS_LEFT:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff),
                          "{\"control\":{\"turn\":\"left\",\"pwm\":{\"L_Motor\":%d,\"R_Motor\":%d}}}",
                          L_PWM_Value,
                          R_PWM_Value) > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;

        case CAR_STATUS_RIGHT:
            if (sprintf_s(uart_sendBuff, sizeof(uart_sendBuff),
                          "{\"control\":{\"turn\":\"right\",\"pwm\":{\"L_Motor\":%d,\"R_Motor\":%d}}}",
                          L_PWM_Value,
                          R_PWM_Value) > 0) {
                uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
            }
            break;
        default:
            break;
    }
    uart_send_buff(uart_sendBuff, strlen(uart_sendBuff));
}

void parse_json_data(const char *payload)
{
    /* 解析JSON数据 */
    cJSON *root = cJSON_Parse(udp_recvBuff);
    if (root) {
        cJSON *json_carSpeed = cJSON_GetObjectItem(root, "carSpeed");
        if (json_carSpeed != NULL) {
            printf("carSpeed: %s\r\n", json_carSpeed->valuestring);
            if (!strcmp(json_carSpeed->valuestring, "low")) {
                systemValue.car_status = CAR_STATUS_L_SPEED;
                base_pwm_speed_value = MOTOR_LOW_SPEED;
            } else if (!strcmp(json_carSpeed->valuestring, "middle")) {
                systemValue.car_status = CAR_STATUS_M_SPEED;
                base_pwm_speed_value = MOTOR_MIDDLE_SPEED;
            } else if (!strcmp(json_carSpeed->valuestring, "high")) {
                systemValue.car_status = CAR_STATUS_H_SPEED;
                base_pwm_speed_value = MOTOR_HIGH_SPEED;
            }
            json_carSpeed = NULL;
        }
        cJSON *json_autoMode = cJSON_GetObjectItem(root, "autoMode");
        if (json_autoMode != NULL) {
            systemValue.auto_abstacle_flag = json_autoMode->valueint;
        }
        cJSON *json_carStatus = cJSON_GetObjectItem(root, "carStatus");
        if (json_carStatus != NULL) {
            if (!strcmp(json_carStatus->valuestring, "on")) {
                systemValue.car_status = CAR_STATUS_ON;
                uart_send_control_status(systemValue.car_status);
            } else if (!strcmp(json_carStatus->valuestring, "off")) {
                systemValue.car_status = CAR_STATUS_OFF;
                uart_send_control_status(systemValue.car_status);
            } else if (!strcmp(json_carStatus->valuestring, "stop")) {
                systemValue.car_status = CAR_STATUS_STOP;
                uart_send_control_status(systemValue.car_status);
            } else if (!strcmp(json_carStatus->valuestring, "run")) {
                systemValue.car_status = CAR_STATUS_RUN;
                uart_send_control_cmd(systemValue.car_status);
            } else if (!strcmp(json_carStatus->valuestring, "back")) {
                systemValue.car_status = CAR_STATUS_BACK;
                uart_send_control_cmd(systemValue.car_status);
            } else if (!strcmp(json_carStatus->valuestring, "left")) {
                systemValue.car_status = CAR_STATUS_LEFT;
                uart_send_control_cmd(systemValue.car_status);
            } else if (!strcmp(json_carStatus->valuestring, "right")) {
                systemValue.car_status = CAR_STATUS_RIGHT;
                uart_send_control_cmd(systemValue.car_status);
            }
            json_carStatus = NULL;
        }
    }
    cJSON_Delete(root);
    root = NULL;
}
void udp_recv_task(void)
{
    socklen_t len = sizeof(client);
    while (1) {
        if (recvfrom(systemValue.udp_socket_fd, udp_recvBuff, sizeof(udp_recvBuff) - 1,
                     0, (struct sockaddr *)&client, &len) > 0) {
            // 读成功
            printf("udp recv data is \" %s \".\r\n", udp_recvBuff);
            parse_json_data((const char *)udp_recvBuff);
            memset_s(udp_recvBuff, sizeof(udp_recvBuff), 0, sizeof(udp_recvBuff));
        }
    }
    closesocket(systemValue.udp_socket_fd);
}
