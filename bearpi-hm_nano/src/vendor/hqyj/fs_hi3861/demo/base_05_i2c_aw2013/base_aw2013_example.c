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

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_pcf8574.h"

osThreadId_t Task1_ID; // 任务1设置为低优先级任务
#define TASK_STACK_SIZE 1024
#define RGB_ON 255
#define RGB_OFF 0
#define TASK_DELAY_TIME 1

void Task1(void)
{
    while (1) {
        AW2013_Control_Red(RGB_ON);
        AW2013_Control_Green(RGB_ON);
        AW2013_Control_Blue(RGB_ON);
        sleep(TASK_DELAY_TIME);

        AW2013_Control_Red(RGB_OFF);
        AW2013_Control_Green(RGB_OFF);
        AW2013_Control_Blue(RGB_OFF);
        sleep(TASK_DELAY_TIME);
    }
}
static void base_aw2013_demo(void)
{
    printf("Enter base_aw2013_demo()!\r\n");

    PCF8574_Init();
    AW2013_Init(); // 三色LED灯的初始化
    AW2013_Control_Red(RGB_OFF);
    AW2013_Control_Green(RGB_OFF);
    AW2013_Control_Blue(RGB_OFF);

    osThreadAttr_t options;
    options.name = "thread_1";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
}
SYS_RUN(base_aw2013_demo);