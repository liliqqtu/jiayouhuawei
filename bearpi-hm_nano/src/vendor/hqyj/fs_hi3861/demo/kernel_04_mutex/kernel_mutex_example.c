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

osThreadId_t Task1_ID; //  任务1 ID
osThreadId_t Task2_ID; //  任务2 ID
osMutexId_t Mutex_ID;       // 定义互斥锁 ID
uint8_t buff[20] = {0};    // 定义一个共享资源
#define TASK_STACK_SIZE 1024
#define TASK_DELAY_TIME 1 // s

/**
 * @description: 任务1
 * @param {*}
 * @return {*}
 */
void Task1(void)
{
    int i = 0;
    printf("enter Task 1.......\n");
    while (1) {
        osMutexAcquire(Mutex_ID, osWaitForever);    // 请求互斥锁

        // 操作共享数据 写数据
        printf("write Buff Data: \n");
        for (i = 0; i < sizeof(buff); i++) {
            buff[i] = i;
        }
        printf("\n");
        osMutexRelease(Mutex_ID);   // 释放互斥锁

        sleep(TASK_DELAY_TIME);
    }
}
/**
 * @description: 任务2
 * @param {*}
 * @return {*}
 */
void Task2(void)
{
    int i = 0;
    printf("enter Task 2.......\n");
    while (1) {
        osMutexAcquire(Mutex_ID, osWaitForever);    // 请求互斥锁
        // 操作共享数据 读数据
        printf("read Buff Data: \n");
        for (i = 0; i < sizeof(buff); i++) {
            printf("%d \n", buff[i]);
        }
        printf("\n");
        osMutexRelease(Mutex_ID);   // 释放互斥锁

        sleep(TASK_DELAY_TIME);
    }
}
/**
 * @description: 初始化并创建任务
 * @param {*}
 * @return {*}
 */
static void kernel_mutex_example(void)
{
    printf("Enter kernel_mutex_example()!\n");

    PCF8574_Init();
    // 创建互斥锁
    Mutex_ID = osMutexNew(NULL);
    if (Mutex_ID != NULL) {
        printf("ID = %d, Create Mutex_ID is OK!\n", Mutex_ID);
    }

    osThreadAttr_t taskOptions;
    taskOptions.name = "Task1";              // 任务的名字
    taskOptions.attr_bits = 0;               // 属性位
    taskOptions.cb_mem = NULL;               // 堆空间地址
    taskOptions.cb_size = 0;                 // 堆空间大小
    taskOptions.stack_mem = NULL;            // 栈空间地址
    taskOptions.stack_size = TASK_STACK_SIZE;           // 栈空间大小 单位:字节
    taskOptions.priority = osPriorityNormal; // 任务的优先级

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &taskOptions);      // 创建任务1
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\n", Task1_ID);
    }

    taskOptions.name = "Task2";              // 任务的名字
    taskOptions.priority = osPriorityNormal; // 任务的优先级
    Task2_ID = osThreadNew((osThreadFunc_t)Task2, NULL, &taskOptions);      // 创建任务2
    if (Task2_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\n", Task2_ID);
    }
}
SYS_RUN(kernel_mutex_example);
