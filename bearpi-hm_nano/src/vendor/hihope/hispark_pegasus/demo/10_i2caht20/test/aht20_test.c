/*
 * Copyright (C) 2021 HiHope Open Source Organization .
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
 *
 * limitations under the License.
 */

#include "aht20.h"

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_gpio.h"
#include "hi_io.h"
#include "hi_i2c.h"

#define STACK_SIZE     (4096)
#define I2C_DATA_RATE  (400 * 1000)  // 400K
#define DELAY_S        (1)

void Aht20TestTask(void)
{
    static int count = 1000;
    uint32_t retval = 0;

    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);

    hi_i2c_init(HI_I2C_IDX_0, I2C_DATA_RATE);

    retval = AHT20_Calibrate();
    printf("AHT20_Calibrate: %u\r\n", retval);

    while (count--) {
        float temp = 0.0, humi = 0.0;

        retval = AHT20_StartMeasure();
        printf("AHT20_StartMeasure: %u\r\n", retval);

        retval = AHT20_GetMeasureResult(&temp, &humi);
        printf("AHT20_GetMeasureResult: %u, temp = %.2f, humi = %.2f\r\n",
               retval, temp, humi);

        sleep(DELAY_S);
    }
}

void Aht20Test(void)
{
    osThreadAttr_t attr;

    attr.name = "Aht20Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(Aht20TestTask, NULL, &attr) == NULL) {
        printf("[Aht20Test] Failed to create Aht20TestTask!\n");
    }
}
APP_FEATURE_INIT(Aht20Test);
