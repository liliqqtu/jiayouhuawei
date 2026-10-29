/*
 * Copyright (C) 2023 HiHope Open Source Organization .
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
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_gpio.h"
#include "hi_io.h"
#include "hi_adc.h"
#include "hi_errno.h"

#define STACK_SIZE   (1024)
#define DELAY_US     (100000)

static void AdcGpioTask(void)
{
    static int count = 1000;
    hi_u16 value;
    while (count--) {
        if (hi_adc_read(HI_ADC_CHANNEL_4, &value,
            HI_ADC_EQU_MODEL_4, HI_ADC_CUR_BAIS_DEFAULT, 0) != HI_ERR_SUCCESS) {
            printf("ADC read error!\n");
        } else {
            printf("ADC_VALUE = %u\n", (unsigned int)value);
            usleep(DELAY_US);
        }
    }
}

static void AdcGpioEntry(void)
{
    printf("ADC Test!\n");
    osThreadAttr_t attr;

    hi_gpio_init();
    hi_io_set_func(HI_GPIO_IDX_9, HI_IO_FUNC_GPIO_9_GPIO);
    hi_gpio_set_dir(HI_GPIO_IDX_9, HI_GPIO_DIR_IN);

    attr.name = "AdcGpioTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(AdcGpioTask, NULL, &attr) == NULL) {
        printf("[LedExample] Falied to create LedTask!\n");
    }
}
SYS_RUN(AdcGpioEntry);