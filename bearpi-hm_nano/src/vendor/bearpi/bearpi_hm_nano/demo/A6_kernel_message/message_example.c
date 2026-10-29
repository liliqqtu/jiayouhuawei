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

// number of Message Queue Objects
#define MSGQUEUE_COUNT 16
#define MSGQUEUE_SIZE 100

#define THREAD_STACK_SIZE (1024 * 10)
#define THREAD_PRIO 25

#define THREAD_DELAY_1S 100

typedef struct {
    // object data type
    char *buf;
    uint8_t idx;
} MSGQUEUE_OBJ_t;

MSGQUEUE_OBJ_t msg;

// message queue id
osMessageQueueId_t g_msgQueueId;

void MsgQueue1Thread(void)
{
    // do some work...
    msg.buf = "Hello BearPi-HM_Nano!";
    msg.idx = 0U;
    while (1) {
        osMessageQueuePut(g_msgQueueId, &msg, 0U, 0U);

        // suspend thread
        osThreadYield();
        osDelay(THREAD_DELAY_1S);
    }
}

void MsgQueue2Thread(void)
{
    osStatus_t status;

    while (1) {
        // wait for message
        status = osMessageQueueGet(g_msgQueueId, &msg, NULL, osWaitForever);
        if (status == osOK) {
            printf("Message Queue Get msg:%s\n", msg.buf);
        }
    }
}

/**
 * @brief Main Entry of the Message Example
 *
 */
static void MessageExample(void)
{
    g_msgQueueId = osMessageQueueNew(MSGQUEUE_COUNT, MSGQUEUE_SIZE, NULL);
    if (g_msgQueueId == NULL) {
        printf("Failed to create Message Queue!\n");
    }

    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    attr.name = "MsgQueue1Thread";
    if (osThreadNew(MsgQueue1Thread, NULL, &attr) == NULL) {
        printf("Failed to create MsgQueue1Thread!\n");
    }

    attr.name = "MsgQueue2Thread";
    if (osThreadNew(MsgQueue2Thread, NULL, &attr) == NULL) {
        printf("Failed to create MsgQueue2Thread!\n");
    }
}

APP_FEATURE_INIT(MessageExample);
