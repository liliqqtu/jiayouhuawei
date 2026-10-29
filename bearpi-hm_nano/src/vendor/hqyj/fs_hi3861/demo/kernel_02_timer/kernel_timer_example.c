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
osTimerId_t Timer_ID;  // 定时器ID
#define TASK_STACK_SIZE 1024
#define TASK1_DELAY_TIME 1 // s

/**
 * 注意： 任务延时不能使用osDelay，这个函数延时有问题
 */

/**
 * @description: 任务1
 * @param {*}
 * @return {*}
 */
void Task1(void)
{
    while (1) {
        printf("enter Task 1.......\n");
        sleep(TASK1_DELAY_TIME); // 1秒
    }
}
/**
 * @description: 定时器1回调函数
 * @param {*}
 * @return {*}
 */
void timer1_Callback(void)
{
    printf("enter timer1_Callback.......\n");
}
/**
 * @description: 初始化并创建任务
 * @param {*}
 * @return {*}
 */
static void kernel_timer_example(void)
{
    printf("Enter kernel_timer_example()!\n");

    PCF8574_Init();
    osThreadAttr_t taskOptions;
    taskOptions.name = "Task1";              // 任务的名字
    taskOptions.attr_bits = 0;               // 属性位
    taskOptions.cb_mem = NULL;               // 堆空间地址
    taskOptions.cb_size = 0;                 // 堆空间大小
    taskOptions.stack_mem = NULL;            // 栈空间地址
    taskOptions.stack_size = TASK_STACK_SIZE;           // 栈空间大小 单位:字节
    taskOptions.priority = osPriorityNormal; // 任务的优先级

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &taskOptions);      // 创建任务
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\n", Task1_ID);
    }

    Timer_ID = osTimerNew(timer1_Callback, osTimerPeriodic, NULL, NULL);       // 创建定时器
    if (Timer_ID != NULL) {
        printf("ID = %d, Create Timer_ID is OK!\n", Timer_ID);

        osStatus_t timerStatus = osTimerStart(Timer_ID, 300U);      // 开始定时器， 并赋予定时器的定时值（在Hi3861中，1U=10ms，100U=1S）
        if (timerStatus != osOK) {
            printf("timer is not startRun !\n");
        }
    }
}
SYS_RUN(kernel_timer_example);
