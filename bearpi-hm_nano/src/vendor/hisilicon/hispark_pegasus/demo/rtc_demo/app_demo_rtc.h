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

#ifndef APP_DEMO_RTC_H
#define APP_DEMO_RTC_H

#include <hi_types_base.h>

#define INS5902_WRITE_ADDRESS    0x64
#define INS5902_READ_ADDRESS     0x65
#define INS5902_REG_ARRAY_LEN    2
#define RTC_REG_TIME_BUF         2
#define SEND_BUF_LEN             3
#define SEND_SET_REG_LEN         2
#define SEND_READ_DATA_LEN       1
#define INS5902_TASK_STAK_SIZE   (1024*2)
#define INS5902_TASK_PRIORITY    25
#define DELAY_TIME               1000
#define SSD1306_LEN              32
#define SSD1306_

#define BATTERY_REGISTER         0x21
#define BATTERY_SWITCH           0x80
#define RTC_SECOND_REGISTER      0x00
#define RTC_MINUTE_REGISTER      0x01
#define RTC_HOUR_REGISTER        0x02
#define RTC_DAY_REGISTER         0x03
#define RTC_DATE_REGISTER        0x04
#define RTC_MONTH_REGISTER       0x05
#define RTC_YEAR_REGISTER        0x06
#define RTC_OLED_DATA            10

/* INS5902 reg */
typedef enum {
    RTC_SECOND = 0,
    RTC_MINUTE,
    RTC_HOUR,
    RTC_DAY,
    RTC_DATE,
    RTC_MONTH,
    RTC_YEAR,
}rct_reg;

typedef struct {
    uint8_t rtc_second[RTC_REG_TIME_BUF];
    uint8_t rtc_minue[RTC_REG_TIME_BUF];
    uint8_t rtc_hour[RTC_REG_TIME_BUF];
    uint8_t rtc_day[RTC_REG_TIME_BUF];
    uint8_t rtc_date[RTC_REG_TIME_BUF];
    uint8_t rtc_month[RTC_REG_TIME_BUF];
    uint8_t rtc_year[RTC_REG_TIME_BUF];
}ins5902_rtc_type;

#endif