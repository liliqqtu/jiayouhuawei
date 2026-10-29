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
#include <stdint.h>
#include <iot_gpio_ex.h>
#include "debug_util.h"

#define MAX_PNTS        (10000)
int16_t g_dbg_buffer[MAX_PNTS];
int g_dbg_buffer_usage = 0;

void reset_debug_points(void)
{
    g_dbg_buffer_usage = 0;
}

void append_debug_point(int16_t data)
{
    if (g_dbg_buffer_usage < MAX_PNTS) {
        g_dbg_buffer[g_dbg_buffer_usage++] = data;
    }
}

void print_debug_points(void)
{
    int i;
    printf("debug_points=[\n");
    for (i = 0; i < g_dbg_buffer_usage; i++) {
        printf("%d,\n", g_dbg_buffer[i]);
    }
    printf("];\n");
}


void init_test_pin(void)
{
    IoSetFunc(TEST_PIN_NAME, TEST_PIN_FUNC);
    IoTGpioSetDir(TEST_PIN_NAME, IOT_GPIO_DIR_OUT);
}