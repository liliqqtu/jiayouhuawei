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


#ifndef DEBUG_UTIL_H
#define DEBUG_UTIL_H

#include <iot_gpio.h>
#include <iot_errno.h>

#define TEST_PIN_NAME       IOT_IO_NAME_GPIO_8
#define TEST_PIN_FUNC       IOT_IO_FUNC_GPIO_8_GPIO

void reset_debug_points(void);
void append_debug_point(int16_t data);
void print_debug_points(void);
void init_test_pin(void);
#endif