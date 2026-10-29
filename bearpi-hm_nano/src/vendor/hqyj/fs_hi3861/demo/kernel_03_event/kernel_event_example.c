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
osEventFlagsId_t event_ID;  // 事件 ID
uint32_t event1_Flags = 0x00000001U;  // 事件掩码 每一位代表一个事件
uint32_t event2_Flags = 0x00000002U;  // 事件掩码 每一位代表一个事件
uint32_t event3_Flags = 0x00000004U;  // 事件掩码 每一位代表一个事件
#define TASK_STACK_SIZE 1024
#define TASK_DELAY_TIME 1 // s

/**
 * 注意： 任务延时不能使用osDelay，这个函数延时有问题
 */

/**
 * @description: 任务1 用于发送事件
 * @param {*}
 * @return {*}
 */
void Task1(void)
{
    while (1) {
        printf("enter Task 1.......\n");
        osEventFlagsSet(event_ID, event1_Flags);        // 设置事件标记
        printf("send eventFlag1.......\n");
        sleep(TASK_DELAY_TIME); // 1秒
        osEventFlagsSet(event_ID, event2_Flags);        // 设置事件标记
        printf("send eventFlag2.......\n");
        sleep(TASK_DELAY_TIME); // 1秒
        osEventFlagsSet(event_ID, event3_Flags);        // 设置事件标记
        printf("send eventFlag3.......\n");
        sleep(TASK_DELAY_TIME); // 1秒
    }
}
/**
 * @description: 任务2 用于接受事件
 * @param {*}
 * @return {*}
 */
void Task2(void)
{
    uint32_t flags = 0;
    while (1) {
        // 永远等待事件标记触发，当接收到 event1_Flags 和 event2_Flags 和 event3_Flags时才会执行printf函数
        // osFlagsWaitAll ：全部事件标志位接收到    osFlagsWaitAny: 任意一个事件标志位接收到
        // 当只有一个事件的时候，事件的类型选择哪个都可以
        flags = osEventFlagsWait(event_ID,  event1_Flags | event2_Flags | event3_Flags, osFlagsWaitAll, osWaitForever);
        printf("receive event is OK\n");        // 事件已经标记
    }
}
/**
 * @description: 初始化并创建任务
 * @param {*}
 * @return {*}
 */
static void kernel_event_example(void)
{
    printf("Enter kernel_event_example()!\n");

    PCF8574_Init();
    event_ID = osEventFlagsNew(NULL);       // 创建事件
    if (event_ID != NULL) {
        printf("ID = %d, Create event_ID is OK!\n", event_ID);
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
    Task2_ID = osThreadNew((osThreadFunc_t)Task2, NULL, &taskOptions);      // 创建任务2
    if (Task2_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\n", Task2_ID);
    }
}
SYS_RUN(kernel_event_example);
