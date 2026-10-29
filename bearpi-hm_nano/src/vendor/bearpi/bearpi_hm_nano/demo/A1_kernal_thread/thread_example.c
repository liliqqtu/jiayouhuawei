/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
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
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"

#define THREAD_STACK_SIZE (1024 * 4)
#define THREAD_PRIO 25
#define THREAD_DELAY_1S 1000000
#define THREAD_DELAY_500MS 500000

/**
 * @brief Thread1 entry
 *
 */
void Thread1(void)
{
    int sum = 0;

    while (1) {
        printf("This is BearPi-HM_Nano Thread1----%d\n", sum++);
        usleep(THREAD_DELAY_1S);
    }
}

/**
 * @brief Thread2 entry
 *
 */
void Thread2(void)
{
    int sum = 0;

    while (1) {
        printf("This is BearPi-HM_Nano Thread2----%d\n", sum++);
        usleep(THREAD_DELAY_500MS);
    }
}

/**
 * @brief Main Entry of the Thread Example
 *
 */
static void ThreadExample(void)
{
    osThreadAttr_t attr;

    attr.name = "Thread1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    // Create the Thread1 task
    if (osThreadNew((osThreadFunc_t)Thread1, NULL, &attr) == NULL) {
        printf("Failed to create Thread1!\n");
    }

    // Create the Thread2 task
    attr.name = "Thread2";
    if (osThreadNew((osThreadFunc_t)Thread2, NULL, &attr) == NULL) {
        printf("Failed to create Thread2!\n");
    }
}

APP_FEATURE_INIT(ThreadExample);
