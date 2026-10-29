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
#include <math.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "E53_SF1.h"

#define TASK_STACK_SIZE (1024*8)
#define TASK_PRIO 25
#define TASK_DELAY_1S 1000000
#define MAX_PPM 15
static void ExampleTask(void)
{
    int ret;
    float  ppm;

    E53SF1Init();
    // Sensor calibration
    usleep(TASK_DELAY_1S);
    MQ2PPMCalibration();

    while (1) {
        printf("=======================================\r\n");
        printf("*************E53_SF1_example***********\r\n");
        printf("=======================================\r\n");
        // get mq2 ppm
        ret = GetMQ2PPM(&ppm);
        if (ret != 0) {
            printf("ADC Read Fail\n");
            return;
        }
        printf("ppm:%.3f \n", ppm);
        if (ppm > MAX_PPM) {
            BeepStatusSet(ON);
        } else {
            BeepStatusSet(OFF);
        }
        usleep(TASK_DELAY_1S);
    }
}

/**
 * @brief Main Entry of the E53_SF1 Example
 *
 */
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