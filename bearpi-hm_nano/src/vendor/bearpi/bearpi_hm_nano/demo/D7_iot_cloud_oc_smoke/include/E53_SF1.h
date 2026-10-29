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

#ifndef __E53_SF1_H__
#define __E53_SF1_H__

#define ADC_VREF_VOL 1.8
#define ADC_COEFFICIENT 4
#define ADC_RATIO 4096

#define MQ2_CONSTANT_1          4
#define MQ2_CONSTANT_2          613.9f
#define MQ2_CONSTANT_3          (-2.074f)

#define CAL_PPM 20 // 校准环境中PPM值
#define RL 1       // RL阻值

#define WIFI_IOT_IO_NAME_GPIO_8 8
#define WIFI_IOT_PWM_PORT_PWM1 1
#define WIFI_IOT_IO_FUNC_GPIO_8_PWM1_OUT 5
#define PWM_DUTY 50
#define PWM_FREQ 4000

#define WIFI_IOT_ADC_6 6

typedef enum {
    OFF = 0,
    ON
} E53SF1Status;

void E53SF1Init(void);
void MQ2PPMCalibration(void);
int GetMQ2PPM(float *ppm);
void BeepStatusSet(E53SF1Status status);

#endif /* __E53_SF1_H__ */

