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
#include "hal_bsp_pcf8574.h"

osThreadId_t Task1_ID;   //  任务1 ID
osThreadId_t Task2_ID;   //  任务2 ID
#define TASK_STACK_SIZE 1024
#define TASK1_DELAY_TIME 1 // s
#define TASK2_DELAY_TIME 5 // s

/**
 * @description: 任务1为低优先级任务
 * @param {*}
 * @return {*}
 */
void Task1(void)
{
    while (1) {
        printf("Task 1.......\n");
        sleep(TASK1_DELAY_TIME);
    }
}
/**
 * @description: 任务2为高优先级任务
 * @param {*}
 * @return {*}
 */
void Task2(void)
{
    uint8_t num = 0;
    while (1) {
        printf("Task 2, 开始挂起任务1\n");
        osThreadSuspend(Task1_ID);      // 挂起任务1
        sleep(TASK2_DELAY_TIME);        // 延时5秒钟

        printf("Task 2, 开始恢复任务1\n");
        osThreadResume(Task1_ID);      // 恢复任务1
        sleep(TASK2_DELAY_TIME);       // 延时5秒钟
    }
}
/**
 * @description: 初始化并创建任务
 * @param {*}
 * @return {*}
 */
static void kernel_task_example(void)
{
    printf("Enter kernel_task_example()!\n");
    
    PCF8574_Init();
    osThreadAttr_t options;
    options.name = "Task1";       // 任务的名字
    options.attr_bits = 0;      // 属性位
    options.cb_mem = NULL;      // 堆空间地址
    options.cb_size = 0;        // 堆空间大小
    options.stack_mem = NULL;   // 栈空间地址
    options.stack_size = TASK_STACK_SIZE;  // 栈空间大小 单位:字节
    options.priority = osPriorityNormal;  // 任务的优先级

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);      // 创建任务1
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\n", Task1_ID);
    }

    options.name = "Task2";
    options.priority = osPriorityNormal1;
    Task2_ID = osThreadNew((osThreadFunc_t)Task2, NULL, &options);      // 创建任务2
    if (Task2_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\n", Task2_ID);
    }
}
SYS_RUN(kernel_task_example);
