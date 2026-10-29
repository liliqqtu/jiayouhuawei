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

#include "uart_recv_task.h"
#include "sys_config.h"
#include "hi_uart.h"
#include "hi_io.h"
#include "hi_gpio.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "cmsis_os2.h"
#include "cJSON.h"

hi_u8 recvBuff[200] = {0};
hi_u8 *pbuff = recvBuff;

void uart_send_buff(unsigned char *str, unsigned short len)
{
    hi_u32 ret = 0;
    ret = hi_uart_write(HI_UART_IDX_2, (uint8_t *)str, len);
    if (ret == HI_ERR_FAILURE) {
        printf("uart send buff is failed.\r\n");
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
                systemValue.distance = json_distance->valueint;
            }
            json_distance = NULL;

            cJSON *json_carPower = cJSON_GetObjectItem(json_status, "carPower");
            if (json_carPower) {
                systemValue.battery_voltage = json_carPower->valueint;
            }
            json_carPower = NULL;

            cJSON *json_L_speed = cJSON_GetObjectItem(json_status, "L_speed");
            if (json_L_speed) {
                systemValue.left_motor_speed = json_L_speed->valueint;
            }
            json_L_speed = NULL;

            cJSON *json_R_speed = cJSON_GetObjectItem(json_status, "R_speed");
            if (json_R_speed) {
                systemValue.right_motor_speed = json_R_speed->valueint;
            }
            json_R_speed = NULL;
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
    while (1) {
        // 阻塞接收
        if (memset_s((char *)uart_buff, sizeof(recvBuff), 0, sizeof(uart_buff)) == 0) {
            hi_u32 len = hi_uart_read(HI_UART_IDX_2, uart_buff, sizeof(uart_buff));
            if (len > 0) {
                memcpy_s((char *)pbuff, len, (char *)uart_buff, len);
                pbuff += len;
                if (len < last_len) {
                    pbuff = recvBuff; // 回到recvBuff的头位置
                    printf("buff: %s\n", recvBuff);
                    parse_json_data(recvBuff);
                    memset_s((char *)recvBuff, sizeof(recvBuff), 0, sizeof(recvBuff));
                }
                last_len = len;
            }
        }
    }
}
