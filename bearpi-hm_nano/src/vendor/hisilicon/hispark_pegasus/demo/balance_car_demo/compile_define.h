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


#ifndef COMPILE_DEFINE_H
#define COMPILE_DEFINE_H

#include "iot_gpio_ex.h"

#define VERA            (1)
#define VERB            (2)
#define VERC            (3)
#define BOARD_VER       (VERC)

#if BOARD_VER == VERA
#define WHEEL_RIGHT_CA_PIN_NAME     (IOT_IO_NAME_GPIO_1)
#define WHEEL_RIGHT_CA_PIN_FUNC     (IOT_IO_FUNC_GPIO_1_GPIO)
#define WHEEL_RIGHT_CB_PIN_NAME     (IOT_IO_NAME_GPIO_0)
#define WHEEL_RIGHT_CB_PIN_FUNC     (IOT_IO_FUNC_GPIO_0_GPIO)
                                            
#define WHEEL_LEFT_CA_PIN_NAME      (IOT_IO_NAME_GPIO_8)
#define WHEEL_LEFT_CA_PIN_FUNC      (IOT_IO_FUNC_GPIO_8_GPIO)
#define WHEEL_LEFT_CB_PIN_NAME      (IOT_IO_NAME_GPIO_7)
#define WHEEL_LEFT_CB_PIN_FUNC      (IOT_IO_FUNC_GPIO_7_GPIO)
#define WHEEL_DIRECTION_REVERT      (0)     // 0: 使编码器增加方向与车前进方向相同
                                            // 1: 使编码器增加方向与车前进方向相反
#elif BOARD_VER == VERB
#define WHEEL_LEFT_CA_PIN_NAME     (IOT_IO_NAME_GPIO_12)
#define WHEEL_LEFT_CA_PIN_FUNC     (IOT_IO_FUNC_GPIO_12_GPIO)
#define WHEEL_LEFT_CB_PIN_NAME     (IOT_IO_NAME_GPIO_0)
#define WHEEL_LEFT_CB_PIN_FUNC     (IOT_IO_FUNC_GPIO_0_GPIO)
#define WHEEL_RIGHT_CA_PIN_NAME    (IOT_IO_NAME_GPIO_1)
#define WHEEL_RIGHT_CA_PIN_FUNC    (IOT_IO_FUNC_GPIO_1_GPIO)
#define WHEEL_RIGHT_CB_PIN_NAME    (IOT_IO_NAME_GPIO_7)
#define WHEEL_RIGHT_CB_PIN_FUNC    (IOT_IO_FUNC_GPIO_7_GPIO)
#define WHEEL_DIRECTION_REVERT     (0)      // 0: 使编码器增加方向与车前进方向相同
                                            // 1: 使编码器增加方向与车前进方向相反
#elif BOARD_VER == VERC
#define WHEEL_LEFT_CA_PIN_NAME     (IOT_IO_NAME_GPIO_0)
#define WHEEL_LEFT_CA_PIN_FUNC     (IOT_IO_FUNC_GPIO_0_GPIO)
#define WHEEL_LEFT_CB_PIN_NAME     (IOT_IO_NAME_GPIO_12)
#define WHEEL_LEFT_CB_PIN_FUNC     (IOT_IO_FUNC_GPIO_12_GPIO)
#define WHEEL_RIGHT_CA_PIN_NAME    (IOT_IO_NAME_GPIO_7)
#define WHEEL_RIGHT_CA_PIN_FUNC    (IOT_IO_FUNC_GPIO_7_GPIO)
#define WHEEL_RIGHT_CB_PIN_NAME    (IOT_IO_NAME_GPIO_1)
#define WHEEL_RIGHT_CB_PIN_FUNC    (IOT_IO_FUNC_GPIO_1_GPIO)
#define WHEEL_DIRECTION_REVERT     (0)      // 0: 使编码器增加方向与车前进方向相同
                                            // 1: 使编码器增加方向与车前进方向相反
#endif
#endif