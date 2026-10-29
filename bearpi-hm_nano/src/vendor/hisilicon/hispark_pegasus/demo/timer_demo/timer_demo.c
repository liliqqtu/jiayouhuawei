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
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_watchdog.h"

#define STACK_SIZE          1024
#define COUNT_STOP          5
#define TIMER_COUNT_NUM     100
#define FEED_DELAY          10

static int count = 0;   // 定义一个count变量，用于控制投喂的次数
                        // Define a count variable to control the feeding times

void FeedSomeFood(void)
{
    count++;
    printf("[Timer_demo] Start feeding count is %d \r\n", count);
}

void TimerThread(const char *arg)
{
    (void)arg;
    osTimerId_t id;
    osStatus_t status;

    // 创建一个周期性的定时器,回调函数是FeedSomeFood，用于宠物喂食机的投喂
    // Create a periodic timer. The callback function is FeedSomeFood, which is used for feeding the pet feeder
    id = osTimerNew((osTimerFunc_t)FeedSomeFood, osTimerPeriodic, NULL, NULL);
    if (id == NULL) {
        printf("[Timer_demo] osTimerNew failed.\r\n");
    } else {
        printf("[Timer_demo] osTimerNew success.\r\n");
    }

    // 开始计时100个时钟周期,一个时钟周期是10ms，100个时钟周期就是1s
    // Start timing 100 clock cycles, one clock cycle is 10ms, 100 clock cycles is 1s
    status = osTimerStart(id, TIMER_COUNT_NUM);
    if (status != osOK) {
        printf("[Timer_demo] osTimerStart failed.\r\n");
    } else {
        printf("[Timer_demo] osTimerStart success.\r\n");
    }

    // 投喂5次后，停止投喂
    // Stop feeding after feeding for 5 times
    while (count < COUNT_STOP) {
        osDelay(FEED_DELAY);
    }

    // 停止定时器
    // Stop Timer
    status = osTimerStop(id);
    printf("[Timer_demo] Timer Stop, status :%d. \r\n", status);

    // 删除定时器
    // Delete Timer
    status = osTimerDelete(id);
    printf("[Timer_demo] Timer Delete, status :%d. \r\n", status);
}

void TimerExampleEntry(void)
{
    osThreadAttr_t attr;

    IoTWatchDogDisable();

    attr.name = "TimerThread";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    // 创建一个线程，并注册一个回调函数 TimerThread，控制红色LED灯每隔1秒钟闪烁一次
    // Create a thread, register a callback function TimerThread, and control the red LED to flash once every 1 second
    if (osThreadNew((osThreadFunc_t)TimerThread, NULL, &attr) == NULL) {
        printf("[Timer_demo] osThreadNew Falied to create TimerThread!\n");
    }
}


APP_FEATURE_INIT(TimerExampleEntry);
