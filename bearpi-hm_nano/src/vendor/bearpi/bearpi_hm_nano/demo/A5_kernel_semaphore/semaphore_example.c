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

osSemaphoreId_t g_semaphoreId;

#define THREAD_STACK_SIZE (1024 * 4)
#define THREAD_PRIO 24

#define THREAD_DELAY_1S 100
#define THREAD_DELAY_10MS 1

#define SEM_MAX_COUNT 4

void Semaphore1Thread(void)
{
    while (1) {
        // release Semaphores twice so that Semaphore2Thread and Semaphore3Thread can execute synchronously
        osSemaphoreRelease(g_semaphoreId);

        // if the Semaphore is released only once, Semaphore2Thread and Semaphore3Thread will run alternately.
        osSemaphoreRelease(g_semaphoreId);

        printf("Semaphore1Thread Release  Semap \n");
        osDelay(THREAD_DELAY_1S);
    }
}
void Semaphore2Thread(void)
{
    while (1) {
        // wait Semaphore
        osSemaphoreAcquire(g_semaphoreId, osWaitForever);

        printf("Semaphore2Thread get Semap \n");
        osDelay(THREAD_DELAY_10MS);
    }
}

void Semaphore3Thread(void)
{
    while (1) {
        // wait Semaphore
        osSemaphoreAcquire(g_semaphoreId, osWaitForever);

        printf("Semaphore3Thread get Semap \n");
        osDelay(THREAD_DELAY_10MS);
    }
}

/**
 * @brief Main Entry of the Semaphore Example
 *
 */
void SemaphoreExample(void)
{
    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    attr.name = "Semaphore1Thread";
    if (osThreadNew((osThreadFunc_t)Semaphore1Thread, NULL, &attr) == NULL) {
        printf("Failed to create Semaphore1Thread!\n");
    }

    attr.name = "Semaphore2Thread";
    if (osThreadNew((osThreadFunc_t)Semaphore2Thread, NULL, &attr) == NULL) {
        printf("Failed to create Semaphore2Thread!\n");
    }

    attr.name = "Semaphore3Thread";
    if (osThreadNew((osThreadFunc_t)Semaphore3Thread, NULL, &attr) == NULL) {
        printf("Failed to create Semaphore3Thread!\n");
    }

    g_semaphoreId = osSemaphoreNew(SEM_MAX_COUNT, 0, NULL);
    if (g_semaphoreId == NULL) {
        printf("Failed to create Semaphore!\n");
    }
}
APP_FEATURE_INIT(SemaphoreExample);
