/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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

#define SIDE_WOLK_DELAY         5   // 人行道持续工作5秒
// The sidewalk works continuously for 5 seconds
#define MOTOR_WAY_DELAY         10  // 机动车道持续工作10秒
                                    // The motorway works continuously for 10 seconds
#define NUM                     1
#define STACK_SIZE              1024

osSemaphoreId_t traffic_light;      // 交通灯信号量
                                    // Traffic light signal volume

// 人行道线程
// Sidewalk thread
void SideWalkThread(const char *arg)
{
    unsigned int cout;
    (void)arg;

    while (NUM) {
        // 获取交通灯信号量令牌
        // Obtain traffic light semaphore token
        osSemaphoreAcquire(traffic_light, osWaitForever);
        printf("**************Pedestrians pass and vehicles stop**************\r\n");
        for (cout = SIDE_WOLK_DELAY; cout > 0; cout--) {
            printf("[SideWalkThread] Countdown for sidewalk work %d \r\n", cout);
            osDelay(100); // 10ms * 100 = 1s
        }
        // 释放交通灯信号量
        // Release traffic light signal
        osSemaphoreRelease(traffic_light);
    }
}

// 机动车道线程
// Motorway thread
void MotorWayThread(const char *arg)
{
    unsigned int cout;
    (void)arg;
    while (NUM) {
        // 获取交通灯信号量令牌
        // Obtain traffic light semaphore token
        osSemaphoreAcquire(traffic_light, osWaitForever);
        printf("**************Vehicles pass and pedestrians stop**************\r\n");
        for (cout = MOTOR_WAY_DELAY; cout > 0; cout--) {
            printf("[MotorWayThread] Countdown for MotorWay work %d\r\n", cout);
            osDelay(100); // 10ms * 100 = 1s
        }
        // 释放交通灯信号量
        // Release traffic light signal
        osSemaphoreRelease(traffic_light);
    }
}

static void SemaphoreDemoEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "SideWalkThread";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    // 创建一个线程，回调函数是SideWalkThread，用来执行人行道的工作
    // Create a thread. The callback function is SideWalkThread, which is used to execute the work of the sidewalk
    if (osThreadNew((osThreadFunc_t)SideWalkThread, NULL, &attr) == NULL) {
        printf("[SideWalkThread] Failed to create SideWalkThread!\n");
    }

    // 创建另一个线程，回调函数是MotorWayThread，用来执行机动车道线程的工作
    // Create another thread. The callback function is MotorWayThread,
    // which is used to execute the work of the motorway thread
    if (osThreadNew((osThreadFunc_t)MotorWayThread, NULL, &attr) == NULL) {
        printf("[MotorWayThread] Failed to create MotorWayThread!\n");
    }

    // 创建一个交通灯的信号量，可用令牌的最大数量为1，可用令牌的初始化数量为0
    // Create a traffic light semaphore. The maximum number of available tokens is 1,
    // and the initialization number of available tokens is 0
    traffic_light = osSemaphoreNew(1, 0, NULL);
    if (traffic_light == NULL) {
        printf("Falied to create Semaphore!\n");
    }

    // 释放交通灯信号量
    // Release traffic light signal
    osSemaphoreRelease(traffic_light);
}

APP_FEATURE_INIT(SemaphoreDemoEntry);