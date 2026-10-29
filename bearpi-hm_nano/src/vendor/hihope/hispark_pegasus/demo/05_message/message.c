/*
 * Copyright (C) 2023 HiHope Open Source Organization .
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

#define STACK_SIZE   (1024)
#define DELAY_TICKS_3   (3)
#define DELAY_TICKS_5   (5)
#define DELAY_TICKS_20 (20)
#define DELAY_TICKS_80 (80)

#define QUEUE_SIZE 3
typedef struct {
    osThreadId_t tid;
    int count;
} message_entry;
osMessageQueueId_t qid;

void sender_thread(void)
{
    static int count = 0;
    message_entry sentry;
    while (1) {
        sentry.tid = osThreadGetId();
        sentry.count = count;
        printf("[Message Test] %s send %d to message queue.\r\n",
               osThreadGetName(osThreadGetId()), count);
        osMessageQueuePut(qid, (const void *)&sentry, 0, osWaitForever);
        count++;
        osDelay(DELAY_TICKS_5);
    }
}

void receiver_thread(void)
{
    message_entry rentry;
    while (1) {
        osMessageQueueGet(qid, (void *)&rentry, NULL, osWaitForever);
        printf("[Message Test] %s get %d from %s by message queue.\r\n",
               osThreadGetName(osThreadGetId()), rentry.count, osThreadGetName(rentry.tid));
        osDelay(DELAY_TICKS_3);
    }
}

osThreadId_t newThread(char *name, osThreadFunc_t func, char *arg)
{
    osThreadAttr_t attr = {
        name, 0, NULL, 0, NULL, STACK_SIZE*2, osPriorityNormal, 0, 0
    };
    osThreadId_t tid = osThreadNew(func, (void *)arg, &attr);
    if (tid == NULL) {
        printf("[Message Test] osThreadNew(%s) failed.\r\n", name);
    } else {
        printf("[Message Test] osThreadNew(%s) success, thread id: %d.\r\n", name, tid);
    }
    return tid;
}

void rtosv2_msgq_main(void)
{
    qid = osMessageQueueNew(QUEUE_SIZE, sizeof(message_entry), NULL);

    osThreadId_t ctid1 = newThread("recevier1", receiver_thread, NULL);
    osThreadId_t ctid2 = newThread("recevier2", receiver_thread, NULL);
    osThreadId_t ptid1 = newThread("sender1", sender_thread, NULL);
    osThreadId_t ptid2 = newThread("sender2", sender_thread, NULL);
    osThreadId_t ptid3 = newThread("sender3", sender_thread, NULL);

    osDelay(DELAY_TICKS_20);
    uint32_t cap = osMessageQueueGetCapacity(qid);
    printf("[Message Test] osMessageQueueGetCapacity, capacity: %u.\r\n", cap);
    uint32_t msg_size =  osMessageQueueGetMsgSize(qid);
    printf("[Message Test] osMessageQueueGetMsgSize, size: %u.\r\n", msg_size);
    uint32_t count = osMessageQueueGetCount(qid);
    printf("[Message Test] osMessageQueueGetCount, count: %u.\r\n", count);
    uint32_t space = osMessageQueueGetSpace(qid);
    printf("[Message Test] osMessageQueueGetSpace, space: %u.\r\n", space);

    osDelay(DELAY_TICKS_80);
    osThreadTerminate(ctid1);
    osThreadTerminate(ctid2);
    osThreadTerminate(ptid1);
    osThreadTerminate(ptid2);
    osThreadTerminate(ptid3);
    osMessageQueueDelete(qid);
}

static void MessageTestTask(void)
{
    osThreadAttr_t attr;

    attr.name = "rtosv2_msgq_main";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)rtosv2_msgq_main, NULL, &attr) == NULL) {
        printf("[MessageTestTask] Falied to create rtosv2_msgq_main!\n");
    }
}
APP_FEATURE_INIT(MessageTestTask);
