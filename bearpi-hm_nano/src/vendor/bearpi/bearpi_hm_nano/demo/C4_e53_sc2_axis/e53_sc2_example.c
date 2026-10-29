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

#include "E53_SC2.h"
#include "cmsis_os2.h"
#include "ohos_init.h"

#define TASK_STACK_SIZE (1024 * 8)
#define TASK_PRIO 25
#define ACCEL_THRESHOLD 100
#define TASK_DELAY_1S 1000000

static void ExampleTask(void)
{
    uint8_t ret;
    E53SC2Data data;
    int X = 0, Y = 0, Z = 0;

    ret = E53SC2Init();
    if (ret != 0) {
        printf("E53_SC2 Init failed!\r\n");
        return;
    }
    while (1) {
        printf("=======================================\r\n");
        printf("*************E53_SC2_example***********\r\n");
        printf("=======================================\r\n");
        ret = E53SC2ReadData(&data);
        if (ret != 0) {
            printf("E53_SC2 Read Data!\r\n");
            return;
        }
        printf("\r\n**************Temperature      is  %d\r\n", (int)data.Temperature);
        printf("\r\n**************Accel[0]         is  %d\r\n", (int)data.Accel[ACCEL_X_AXIS]);
        printf("\r\n**************Accel[1]         is  %d\r\n", (int)data.Accel[ACCEL_Y_AXIS]);
        printf("\r\n**************Accel[2]         is  %d\r\n", (int)data.Accel[ACCEL_Z_AXIS]);
        if (X == 0 && Y == 0 && Z == 0) {
            X = (int)data.Accel[ACCEL_X_AXIS];
            Y = (int)data.Accel[ACCEL_Y_AXIS];
            Z = (int)data.Accel[ACCEL_Z_AXIS];
        } else {
            if (X + ACCEL_THRESHOLD < data.Accel[ACCEL_X_AXIS] || X - ACCEL_THRESHOLD > data.Accel[ACCEL_X_AXIS]
                || Y + ACCEL_THRESHOLD < data.Accel[ACCEL_Y_AXIS] || Y - ACCEL_THRESHOLD > data.Accel[ACCEL_Y_AXIS]
                || Z + ACCEL_THRESHOLD < data.Accel[ACCEL_Z_AXIS] || Z - ACCEL_THRESHOLD > data.Accel[ACCEL_Z_AXIS]) {
                LedD1StatusSet(OFF);
                LedD2StatusSet(ON);
            } else {
                LedD1StatusSet(ON);
                LedD2StatusSet(OFF);
            }
        }
        usleep(TASK_DELAY_1S);
    }
}

static void ExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "ExampleTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = TASK_STACK_SIZE;
    attr.priority = TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)ExampleTask, NULL, &attr) == NULL) {
        printf("Failed to create ExampleTask!\n");
    }
}

APP_FEATURE_INIT(ExampleEntry);