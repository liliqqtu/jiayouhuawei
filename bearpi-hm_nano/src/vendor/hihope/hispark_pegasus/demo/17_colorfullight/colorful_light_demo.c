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

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_pwm.h"
#include "iot_errno.h"
#include "hi_adc.h"

#define RED_LED_PIN_NAME        10
#define GREEN_LED_PIN_NAME      11
#define BLUE_LED_PIN_NAME       12
#define HUMAN_SENSOR_CHAN_NAME  3
#define LIGHT_SENSOR_CHAN_NAME  4

#define LED_PWM_FUNCTION        5
#define IOT_PWM_PORT_PWM1       1
#define IOT_PWM_PORT_PWM2       2
#define IOT_PWM_PORT_PWM3       3
#define IOT_PWM_PORT_PWM_MAX    4
#define IOT_PWM_DUTY_MAX        100

#define LED_BRIGHT              IOT_GPIO_VALUE1
#define LED_DARK                IOT_GPIO_VALUE0

#define NUM_BLINKS              2
#define NUM_SENSORS             2
#define NUM_2                   2
#define NUM_3                   3

#define ADC_RESOLUTION          4096
#define PWM_FREQ_DIVITION       64000
#define CLK_160M                160000000

#define STACK_SIZE             (4096)
#define DELAY_300MS            (300 * 1000)
#define DELAY_10MS             (10 * 1000)

static void CorlorfulLightTask(void)
{
    static const unsigned int pins[] = {RED_LED_PIN_NAME, GREEN_LED_PIN_NAME, BLUE_LED_PIN_NAME};

    for (int i = 0; i < NUM_BLINKS; i++) {
        for (unsigned j = 0; j < sizeof(pins) / sizeof(pins[0]); j++) {
            IoTGpioSetOutputVal(pins[j], LED_BRIGHT);
            usleep(DELAY_300MS);
            IoTGpioSetOutputVal(pins[j], LED_DARK);
            usleep(DELAY_300MS);
        }
    }

    // set Red/Green/Blue LED pin to pwm function
    hi_io_set_func(RED_LED_PIN_NAME, LED_PWM_FUNCTION);
    hi_io_set_func(GREEN_LED_PIN_NAME, LED_PWM_FUNCTION);
    hi_io_set_func(BLUE_LED_PIN_NAME, LED_PWM_FUNCTION);

    IoTPwmInit(IOT_PWM_PORT_PWM1); // R
    IoTPwmInit(IOT_PWM_PORT_PWM2); // G
    IoTPwmInit(IOT_PWM_PORT_PWM3); // B

    for (int i = 1; i < IOT_PWM_PORT_PWM_MAX; i++) {
        // use PWM control BLUE LED brightness
        for (int j = 1; j <= IOT_PWM_DUTY_MAX; j *= NUM_2) {
            IoTPwmStart(i, j, CLK_160M / PWM_FREQ_DIVITION);
            usleep(DELAY_300MS);
            IoTPwmStop(i);
        }
    }

    static int g_blinkCnt = 1000;
    while (g_blinkCnt--) {
        unsigned short duty[NUM_SENSORS] = {0, 0};
        unsigned short data[NUM_SENSORS] = {0, 0};
        static const int chan[] = {HUMAN_SENSOR_CHAN_NAME, LIGHT_SENSOR_CHAN_NAME};
        static const int port[] = {IOT_PWM_PORT_PWM1, IOT_PWM_PORT_PWM2};

        for (size_t i = 0; i < sizeof(chan) / sizeof(chan[0]); i++) {
            if (hi_adc_read(chan[i], &data[i], HI_ADC_EQU_MODEL_4, HI_ADC_CUR_BAIS_DEFAULT, 0)
                == IOT_SUCCESS) {
                duty[i] = IOT_PWM_DUTY_MAX * (unsigned int)data[i] / ADC_RESOLUTION - NUM_3;
            }
            IoTPwmStart(port[i], duty[i], PWM_FREQ_DIVITION);
            usleep(DELAY_10MS);
            IoTPwmStop(port[i]);
        }
    }
}

static void ColorfulLightDemo(void)
{
    osThreadAttr_t attr;

    IoTGpioInit(RED_LED_PIN_NAME);
    IoTGpioInit(GREEN_LED_PIN_NAME);
    IoTGpioInit(BLUE_LED_PIN_NAME);

    // set Red/Green/Blue LED pin as output
    IoTGpioSetDir(RED_LED_PIN_NAME, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(GREEN_LED_PIN_NAME, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(BLUE_LED_PIN_NAME, IOT_GPIO_DIR_OUT);

    attr.name = "CorlorfulLightTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(CorlorfulLightTask, NULL, &attr) == NULL) {
        printf("[ColorfulLightDemo] Falied to create CorlorfulLightTask!\n");
    }
}

APP_FEATURE_INIT(ColorfulLightDemo);