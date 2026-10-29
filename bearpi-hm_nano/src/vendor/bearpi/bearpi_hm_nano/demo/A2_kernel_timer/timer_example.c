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

/**
 * @brief Callback for Timer1 triggering
 *
 */
void Timer1Callback(void)
{
    printf("This is BearPi Timer1Callback!\n");
}

/**
 * @brief Callback for Timer2 triggering
 *
 */
void Timer2Callback(void)
{
    printf("This is BearPi Timer2Callback!\n");
}

/**
 * @brief Main Entry of the Timer Example
 *
 */
static void TimerExample(void)
{
    osTimerId_t id1, id2;
    uint32_t timerDelay;
    osStatus_t status;

    id1 = osTimerNew(Timer1Callback, osTimerPeriodic, NULL, NULL);
    if (id1 != NULL) {
        // Hi3861 1U=10ms,100U=1S
        timerDelay = 100U;

        status = osTimerStart(id1, timerDelay);
        if (status != osOK) {
            printf("Failed to start Timer1!\n");
        }
    }

    id2 = osTimerNew(Timer2Callback, osTimerPeriodic, NULL, NULL);
    if (id2 != NULL) {
        // Hi3861 1U=10ms,300U=3S
        timerDelay = 300U;

        status = osTimerStart(id2, timerDelay);
        if (status != osOK) {
            printf("Failed to start Timer2!\n");
        }
    }
}

APP_FEATURE_INIT(TimerExample);
