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
#include <string.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "hi_errno.h"
#include "hi_time.h"
#include "ssd1306_fonts.h"
#include "oled_ssd1306.h"
#include "iot_gpio_ex.h"
#include "iot_i2c.h"
#include "app_demo_rtc.h"

#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define INS5902_I2C_IDX      0
#define DECIMA               10
#define HEX                  16

int second = 0;
int minute = 0;
int hour = 0;
int day = 0;
int date = 0;
int month = 0;
int year = 0;

/* gpio init */
void gpio_init(void)
{
    // 设置GPIO13的管脚复用关系为I2C0_SDA Set the pin reuse relationship of GPIO13 to I2C0_ SDA
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    // 设置GPIO14的管脚复用关系为I2C0_SCL Set the pin reuse relationship of GPIO14 to I2C0_ SCL
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
}

uint32_t ins5902_i2c_write(uint8_t reg_addr, uint8_t high_8, uint8_t low_8, uint8_t reg_len)
{
    uint8_t temp1 = 0;
    uint8_t temp2 = 0;
    temp1 = (high_8 / DECIMA * HEX) + (high_8 % DECIMA); // Hexadecimal to BCD
    temp2 = (low_8 / DECIMA * HEX) + (low_8 % DECIMA); // Hexadecimal to BCD
    uint8_t buffer[3] = {reg_addr, temp1, temp2};
    uint32_t retval = IoTI2cWrite(INS5902_I2C_IDX, INS5902_WRITE_ADDRESS, buffer, reg_len);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    printf("IoTI2cWrite(%02X)\r\n", buffer[0]);
    return IOT_SUCCESS;
}


uint32_t ins5902_write(uint8_t reg_addr, uint8_t high_8)
{
    uint8_t buffer[2] = {reg_addr, high_8};
    uint32_t buffLen = 2;
    uint32_t retval = IoTI2cWrite(INS5902_I2C_IDX, INS5902_WRITE_ADDRESS, buffer, buffLen);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    printf("IoTI2cWrite(%02X)\r\n", buffer[0]);
    return IOT_SUCCESS;
}

uint8_t ins5902_read(uint8_t rtc_reg, uint32_t recv_len, uint8_t *rct_buf)
{
    int ret = 0;
    // ins5902_rtc_type read_rtc;
    uint8_t recv_data[INS5902_REG_ARRAY_LEN] = { 0 };
    /* Request memory space */
    memset_s(rct_buf, RTC_REG_TIME_BUF + 1, 0x00, RTC_REG_TIME_BUF);
    ret = memset_s(recv_data, INS5902_REG_ARRAY_LEN + 1, 0x00, INS5902_REG_ARRAY_LEN);
    if (ret != 0) {
        return 0;
    }
    uint32_t status = IoTI2cRead(INS5902_I2C_IDX, INS5902_READ_ADDRESS, recv_data, recv_len);
    if (status != HI_ERR_SUCCESS) {
        printf("===== Error: ins5902 sencor I2C read status = 0x%x! =====\r\n", status);
        return status;
    }
    switch (rtc_reg) {
        case RTC_SECOND:
            rct_buf[0] = recv_data[0];
            break;
        case RTC_MINUTE:
            rct_buf[0] = recv_data[0];
            break;
        case RTC_HOUR:
            rct_buf[0] = recv_data[0];
            break;
        case RTC_DAY :
            rct_buf[0] = recv_data[0];
            break;
        case RTC_DATE:
            rct_buf[0] = recv_data[0];
            break;
        case RTC_MONTH:
            rct_buf[0] = recv_data[0];
            break;
        case RTC_YEAR:
            rct_buf[0] = recv_data[0];
            break;
        default:
            break;
    }
    return rct_buf[0];
}
/* rtc timer setting */
void rct_set_init(void)
{
    uint32_t ret;
    ins5902_rtc_type rct_time_set = { 0 };
    rct_time_set.rtc_second[0] = 30; // 30代表秒 30 represents seconds
    rct_time_set.rtc_minue[0] = 05; // 05代表分钟 05 for minutes
    rct_time_set.rtc_hour[0] = 17; // 17代表小时 17 represents hours
    rct_time_set.rtc_day[0] = 5; // 5代表周6 5 represents week 6
    rct_time_set.rtc_date[0] = 4; // 4代表号 4 Representative No
    rct_time_set.rtc_month[0] = 6; // 6代表月 6 representative months
    rct_time_set.rtc_year[0] = 22; // 22代表年 22 Representative year
    // set second
    ret = ins5902_i2c_write(RTC_SECOND, rct_time_set.rtc_second[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to second cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
    // set minute
    ret = ins5902_i2c_write(RTC_MINUTE, rct_time_set.rtc_minue[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to minute cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
    // set hour
    ret = ins5902_i2c_write(RTC_HOUR, rct_time_set.rtc_hour[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to hour cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
    // set day
    ret = ins5902_i2c_write(RTC_DAY, rct_time_set.rtc_day[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to day cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
    // set date
    ret = ins5902_i2c_write(RTC_DATE, rct_time_set.rtc_date[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to date cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
    // set month
    ret = ins5902_i2c_write(RTC_MONTH, rct_time_set.rtc_month[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to month cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
    // set year
    ret = ins5902_i2c_write(RTC_YEAR, rct_time_set.rtc_year[0], NULL, SEND_SET_REG_LEN);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to year cmd\r\n");
    }
    hi_udelay(DELAY_TIME);
}

uint8_t GetWeek(uint8_t weekdata)
{
    uint8_t res = 0;
    if (weekdata == 40) { // The read register value is 40, representing week 6
        res = 6;
    } else if (weekdata == 20) { // The read register value is 20, representing week 5
        res = 5;
    } else if (weekdata == 10) { // The read register value is 10, representing week 4
        res = 4;
    } else if (weekdata == 8) { // The read register value is 8, representing week 3
        res = 3;
    } else if (weekdata == 4) { // The read register value is 4, representing week 2
        res = 2;
    } else if (weekdata == 2) { // The read register value is 2, representing week 1
        res = 1;
    } else if (weekdata == 1) { // The read register value is 1, representing week 7
        res = 7;
    }
    return res;
}

void GetSecond(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_SECOND_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_SECOND, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_second[0] != rct_read_data[0]) {
        rtc_data.rtc_second[0] = rct_read_data[0];
        second = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", second);
        /* 需要显示的字符串长度为2和1 */
        /* The length of the string to be displayed is 2 and 1 */
        if (ret < 0) {
            printf("failed\r\n");
        }
        if (second >= RTC_OLED_DATA) {
            /* 在OLED屏幕的第48列6行显示1行 */
            /* Display 1 row in the 48th column and 6 rows of the OLED screen */
            OledShowString(48, 6, line, 1);
        } else {
            OledShowString(48, 6, "0", 1); // Display 1 row in the 48th column and 6 rows of the OLED screen
            OledShowString(56, 6, line, 1); // Display 1 row in the 56th column and 6 rows of the OLED screen
        }
        OledShowString(64, 6, " ", 1); // Display 1 row in the 64th column and 6 rows of the OLED screen
    }
    TaskMsleep(DELAY_TIME);
}

void GetMinute(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_MINUTE_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_MINUTE, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_minue[0] != rct_read_data[0]) {
        rtc_data.rtc_minue[0] = rct_read_data[0];
        minute = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", minute);
        /* 需要显示的字符串长度为2和1 */
        /* The length of the string to be displayed is 2 and 1 */
        if (ret < 0) {
            printf("failed\r\n");
        }
        if (minute >= RTC_OLED_DATA) {
            OledShowString(24, 6, line, 1); // Display 1 row in the 24th column and 6 rows of the OLED screen
            OledShowString(40, 6, ":", 1); // Display 1 row in the 40th column and 6 rows of the OLED screen
        } else {
            OledShowString(24, 6, "0", 1); // Display 1 row in the 24th column and 6 rows of the OLED screen
            OledShowString(32, 6, line, 1); // Display 1 row in the 32th column and 6 rows of the OLED screen
            OledShowString(40, 6, ":", 1); // Display 1 row in the 40th column and 6 rows of the OLED screen
        }
        OledShowString(56, 6, "0", 1); // Display 1 row in the 56th column and 6 rows of the OLED screen
    }
}

void GetHour(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_HOUR_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_HOUR, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_hour[0] != rct_read_data[0]) {
        rtc_data.rtc_hour[0] = rct_read_data[0];
        hour = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", hour);
        /* 需要显示的字符串长度为2和1 */
        /* The length of the string to be displayed is 2 and 1 */
        if (ret < 0) {
            printf("failed\r\n");
        }
        if (hour >= RTC_OLED_DATA) {
            OledShowString(0, 6, line, 1); // Display 1 row in the 0th column and 6 rows of the OLED screen
            OledShowString(16, 6, ":", 1); // Display 1 row in the 16th column and 6 rows of the OLED screen
        } else {
            OledShowString(0, 6, "0", 1); // Display 1 row in the 0th column and 6 rows of the OLED screen
            OledShowString(8, 6, line, 1); // Display 1 row in the 8th column and 6 rows of the OLED screen
            OledShowString(16, 6, ":", 1); // Display 1 row in the 16th column and 6 rows of the OLED screen
        }
        OledShowString(64, 6, " ", 1); // Display 1 row in the 64th column and 6 rows of the OLED screen
    }
}

void GetDay(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_DAY_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_DAY, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_day[0] != rct_read_data[0]) {
        rtc_data.rtc_day[0] = rct_read_data[0];
        day = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        day = GetWeek(day);
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", day);
        /* 需要显示的字符串长度为2和1 */
        /* The length of the string to be displayed is 2 and 1 */
        if (ret < 0) {
            printf("failed\r\n");
        }
        OledShowString(72, 6, "week:", 1); // Display 1 row in the 72th column and 6 rows of the OLED screen
        OledShowString(112, 6, line, 1); // Display 1 row in the 112th column and 6 rows of the OLED screen
    }
}

void GetDate(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_DATE_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_DATE, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_date[0] != rct_read_data[0]) {
        rtc_data.rtc_date[0] = rct_read_data[0];
        date = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", date);
        /* 需要显示的字符串长度为2和1 */
        /* The length of the string to be displayed is 2 and 1 */
        if (ret < 0) {
            printf("failed\r\n");
        }
        if (date >= RTC_OLED_DATA) {
            OledShowString(89, 4, line, 1); // Display 1 row in the 89th column and 4 rows of the OLED screen
        } else {
            OledShowString(89, 4, "0", 1); // Display 1 row in the 89th column and 4 rows of the OLED screen
            OledShowString(97, 4, line, 1); // Display 1 row in the 97th column and 4 rows of the OLED screen
        }
            OledShowString(105, 4, " ", 1); // Display 1 row in the 105th column and 4 rows of the OLED screen
    }
}

void GetMonth(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_MONTH_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_MONTH, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_month[0] != rct_read_data[0]) {
        rtc_data.rtc_month[0] = rct_read_data[0];
        month = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", month);
        /* 需要显示的字符串长度为2和1 */
        /* The length of the string to be displayed is 2 and 1 */
        if (ret < 0) {
            printf("failed\r\n");
        }
        if (month >= RTC_OLED_DATA) {
            OledShowString(65, 4, line, 1); // Display 1 row in the 65th column and 4 rows of the OLED screen
            OledShowString(81, 4, "-", 1); // Display 1 row in the 81th column and 4 rows of the OLED screen
        } else {
            OledShowString(65, 4, "0", 1); // Display 1 row in the 65th column and 4 rows of the OLED screen
            OledShowString(73, 4, line, 1); // Display 1 row in the 73th column and 4 rows of the OLED screen
            OledShowString(81, 4, "-", 1); // Display 1 row in the 81th column and 4 rows of the OLED screen
        }
    }
}

void GetYear(void)
{
    uint8_t rct_read_data[RTC_REG_TIME_BUF] = { 0 };
    ins5902_rtc_type rtc_data = { 0 };
    static char line[SSD1306_LEN] = { 0 };
    ins5902_i2c_write(RTC_YEAR_REGISTER, NULL, NULL, SEND_READ_DATA_LEN);
    ins5902_read(RTC_YEAR, SEND_READ_DATA_LEN, rct_read_data);
    if (rtc_data.rtc_year[0] != rct_read_data[0]) {
        rtc_data.rtc_year[0] = rct_read_data[0];
        year = rct_read_data[0] / HEX * DECIMA + rct_read_data[0] % HEX;
        int ret = snprintf_s(line, sizeof(line), sizeof(line), "%d", year);
        if (ret < 0) { // 需要显示的字符串长度为2
            printf("failed\r\n");
        }
        OledShowString(25, 4, "20", 1); // Display 1 row in the 25th column and 4 rows of the OLED screen
        OledShowString(41, 4, line, 1); // Display 1 row in the 41th column and 4 rows of the OLED screen
        OledShowString(57, 4, "-", 1); // Display 1 row in the 57th column and 4 rows of the OLED screen
        OledShowString(104, 4, " ", 1); // Display 1 row in the 104th column and 4 rows of the OLED screen
    }
}

/* read rtc time */
void rtc_timer(void)
{
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
    IoTI2cInit(INS5902_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    IoTI2cSetBaudrate(INS5902_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /* ssd1306 config init */
    OledInit();
    OledFillScreen(0);
    rct_set_init(); // Set RTC initial time 1.第一次启动set time, 掉电后自动跑，请不要设置时间
    ins5902_write(BATTERY_REGISTER, BATTERY_SWITCH); // 开启电池
    while (1) {
        /*----------------------second--------------*/
        GetSecond();
        // /*----------------------minute--------------*/
        GetMinute();
        /*----------------------hour--------------*/
        GetHour();
        /*----------------------day-------------*/
        GetDay();
        /*----------------------date--------------*/
        GetDate();
        /*----------------------month--------------*/
        GetMonth();
        /*----------------------year--------------*/
        GetYear();
    }
}

static void RTCSampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "RTCTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = INS5902_TASK_STAK_SIZE;
    attr.priority = INS5902_TASK_PRIORITY;

    if (osThreadNew((osThreadFunc_t)rtc_timer, NULL, &attr) == NULL) {
        printf("[LedExample] Failed to create LedTask!\n");
    }
}

APP_FEATURE_INIT(RTCSampleEntry);