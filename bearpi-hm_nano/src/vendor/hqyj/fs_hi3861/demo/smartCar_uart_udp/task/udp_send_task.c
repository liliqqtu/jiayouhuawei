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

#include "udp_send_task.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "sys_config.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/api_shell.h"
#include "cJSON.h"

struct sockaddr_in client; // 客户端
#define  TASK_DELAY_TIME (50 * 1000)

void udp_send_task(void)
{
    int ret = 0;
    while (1) {
        cJSON *json_root = cJSON_CreateObject();
        cJSON *json_speed = cJSON_CreateObject();
        if (json_root) {
            cJSON_AddItemToObject(json_root, "carStatus", cJSON_CreateString(get_CurrentCarStatus(systemValue)));
            cJSON_AddItemToObject(json_root, "carSpeed", json_speed);
            cJSON_AddItemToObject(json_speed, "left", cJSON_CreateNumber(systemValue.left_motor_speed));
            cJSON_AddItemToObject(json_speed, "right", cJSON_CreateNumber(systemValue.right_motor_speed));
            cJSON_AddItemToObject(json_root, "carPower", cJSON_CreateNumber(systemValue.battery_voltage));
            cJSON_AddItemToObject(json_root, "distance", cJSON_CreateNumber(systemValue.distance));
        }
        char *payload = cJSON_PrintUnformatted(json_root);

        ret = sendto(systemValue.udp_socket_fd, payload, strlen(payload), 0,
                     (struct sockaddr *)&client, sizeof(client));
        cJSON_Delete(json_root);
        free(payload);
        usleep(TASK_DELAY_TIME); // 50ms
    }
    closesocket(systemValue.udp_socket_fd);
}
