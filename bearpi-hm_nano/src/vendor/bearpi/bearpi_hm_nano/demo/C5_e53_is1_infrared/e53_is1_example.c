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

#include "E53_IS1.h"
#include "cmsis_os2.h"
#include "ohos_init.h"

#define TASK_STACK_SIZE (1024 * 8)
#define TASK_PRIO 25
#define TASK_DELAY_3S 300
#define FLAGS_MSK1 0x00000001U

osEventFlagsId_t g_eventFlagsId;

static void BeepAlarm(char *arg)
{
    (void)arg;
    osEventFlagsSet(g_eventFlagsId, FLAGS_MSK1);
}

static void ExampleTask(void)
{
    int ret;
    E53IS1Init();
    ret = E53IS1ReadData(BeepAlarm);
    if (ret != 0) {
        printf("E53_IS1 Read Data failed!\r\n");
        return;
    }
    while (1) {
        osEventFlagsWait(g_eventFlagsId, FLAGS_MSK1, osFlagsWaitAny, osWaitForever);
        BeepStatusSet(ON);
        osDelay(TASK_DELAY_3S);
        BeepStatusSet(OFF);
    }
}

static void ExampleEntry(void)
{
    g_eventFlagsId = osEventFlagsNew(NULL);
    if (g_eventFlagsId == NULL) {
        printf("Failed to create EventFlags!\n");
    }
    osThreadAttr_t attr;

    attr.name = "ExampleTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = TASK_STACK_SIZE;
    attr.priority = TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)ExampleTask, NULL, &attr) == NULL) {
        printf("Failed to create Example_Task!\n");
    }
}

APP_FEATURE_INIT(ExampleEntry);