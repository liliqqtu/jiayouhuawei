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

#define FLAGS_MSK1 0x00000001U
#define THREAD_STACK_SIZE (1024 * 4)
#define THREAD_PRIO 25
#define SEND_THREAD_DELAY_1S 100

osEventFlagsId_t g_eventFlagsId; // event flags id

/**
 * @brief Event sender thread used to set event flag
 *
 */
void EventSenderThread(void)
{
    while (1) {
        osEventFlagsSet(g_eventFlagsId, FLAGS_MSK1);

        // suspend thread
        osThreadYield();

        osDelay(SEND_THREAD_DELAY_1S);
    }
}

/**
 * @brief Event receiver thread blocking wait event flag
 *
 */
void EventReceiverThread(void)
{
    uint8_t flags;

    while (1) {
        flags = osEventFlagsWait(g_eventFlagsId, FLAGS_MSK1, osFlagsWaitAny, osWaitForever);
        printf("Receive Flags is %d\n", flags);
    }
}

/**
 * @brief Main Entry of the Event Example
 *
 */
static void EventExample(void)
{
    g_eventFlagsId = osEventFlagsNew(NULL);
    if (g_eventFlagsId == NULL) {
        printf("Failed to create EventFlags!\n");
    }

    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    attr.name = "EventSenderThread";
    if (osThreadNew(EventSenderThread, NULL, &attr) == NULL) {
        printf("Failed to create EventSenderThread!\n");
    }

    attr.name = "EventReceiverThread";
    if (osThreadNew(EventReceiverThread, NULL, &attr) == NULL) {
        printf("Failed to create EventReceiverThread!\n");
    }
}

APP_FEATURE_INIT(EventExample);
