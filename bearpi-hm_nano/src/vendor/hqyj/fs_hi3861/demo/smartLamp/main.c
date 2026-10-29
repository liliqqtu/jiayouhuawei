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

#include "system_init_task.h"

#define TASK_STACK_SIZE (1024 * 5)

static void smartLamp_main(void)
{
    // 创建系统初始化任务
    osThreadAttr_t options;
    options.name = "system_Init_Task";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal;
    system_Init_Task_ID = osThreadNew((osThreadFunc_t)system_Init_Task, NULL, &options);
    if (system_Init_Task_ID != NULL) {
        printf("ID = %d, Create mqtt_send_task_id is OK!\r\n", system_Init_Task_ID);
    }
}
SYS_RUN(smartLamp_main);