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


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "iot_errno.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_pwm.h"
#include "iot_uart.h"
#include "E53_ST1.h"

#define WIFI_IOT_PWM_PORT_PWM1 1
#define WIFI_IOT_PWM1_GPIO 8
#define WIFI_IOT_IO_FUNC_GPIO_8_PWM1_OUT 5
#define PWM_DUTY 50
#define PWM_FREQ 4000

#define WIFI_IOT_IO_FUNC_GPIO_7_GPIO 0
#define WIFI_IOT_UART_IDX_1 1
#define WIFI_IOT_GPIO_IDX_7 7

gps_msg gpsmsg;
static unsigned char gps_uart[1000];
/***************************************************************
 * 函数名称: GPSInit
 * 说    明: 初始化GPS模块
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void GPSInit(void)
{
    uint8_t ret;

    IotUartAttribute uart_attr = {

        // baud_rate: 9600
        .baudRate = 9600,

        // data_bits: 8bits
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    // Initialize uart driver
    ret = IoTUartInit(WIFI_IOT_UART_IDX_1, &uart_attr);
    if (ret != IOT_SUCCESS) {
        printf("Failed to init uart! Err code = %d\n", ret);
        return;
    }
}

/***************************************************************
 * 函数名称: E53ST1Init
 * 说    明: 初始化E53_ST1扩展板
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void E53ST1Init(void)
{
    IoTGpioInit(WIFI_IOT_PWM1_GPIO);                                      // 初始化GPIO
    IoTGpioSetFunc(WIFI_IOT_PWM1_GPIO, WIFI_IOT_IO_FUNC_GPIO_8_PWM1_OUT); // 设置GPIO_8引脚复用功能为PWM
    IoTGpioSetDir(WIFI_IOT_PWM1_GPIO, IOT_GPIO_DIR_OUT);                  // 设置GPIO_8引脚为输出模式
    IoTPwmInit(WIFI_IOT_PWM_PORT_PWM1);                  // 初始化PWM1端口

    IoTGpioInit(WIFI_IOT_GPIO_IDX_7);
    IoTGpioSetFunc(WIFI_IOT_GPIO_IDX_7, WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoTGpioSetDir(WIFI_IOT_GPIO_IDX_7, IOT_GPIO_DIR_OUT); // 设置GPIO_7为输出模式
    IoTGpioSetOutputVal(WIFI_IOT_GPIO_IDX_7, 0);

    GPSInit();
}

/***************************************************\
* 函数名称: NMEA_Comma_Pos
*	函数功能：从buf里面得到第cx个逗号所在的位置
*	输入值：buf和第cx个逗号
*	返回值：0~0xFE，代表逗号所在位置的便宜
*				 	0xFF，代表不存在第cx个逗号
\***************************************************/

uint8_t NMEA_Comma_Pos(uint8_t *buf, uint8_t cx)
{
    uint8_t *p = buf;
    while (cx) {
        if (*buf == '*' || *buf < ' ' || *buf > 'z') {
            return 0xFF;
        }
        if (*buf == ',') {
            cx--;
        }
        buf++;
    }
    return buf - p;
}
/***************************************************\
* 函数名称: NMEA_Pow
*	函数功能：返回m的n次方值
*	输入值：底数m和指数n
*	返回值：m^n
\***************************************************/
uint32_t NMEA_Pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}
/***************************************************\
* 函数名称: NMEA_Str2num
*	函数功能：str数字转换为（int）数字，以','或者'*'结束
*	输入值：buf，数字存储区
*				 	dx，小数点位数，返回给调用函数
*	返回值：转换后的数值
\***************************************************/
int NMEA_Str2num(uint8_t *buf, uint8_t *dx)
{
    uint8_t *p = buf;
    uint32_t ires = 0, fres = 0;
    uint8_t ilen = 0, flen = 0, i;
    uint8_t mask = 0;
    int res;
    while (1) {
        if (*p == '-') {
            mask |= 0x02;
            p++;
        } // 说明有负数
        if (*p == ',' || *p == '*') {
            break; // 遇到结束符
        }
        if (*p == '.') { // 遇到小数点
            mask |= 0x01;
            p++;
        } else if (*p > '9' || (*p < '0')) { // 数字不在0和9之内，说明有非法字符
            ilen = 0;
            flen = 0;
            break;
        }
        if (mask & 0x01)
            flen++; // 小数点的位数
        else
            ilen++; // str长度加一
        p++;        // 下一个字符
    }
    if (mask & 0x02) {
        buf++;                 // 移到下一位，除去负号
    }
    for (i = 0; i < ilen; i++) { // 得到整数部分数据
        ires += NMEA_Pow(L80R_CONSTANT_10, ilen - 1 - i) * (buf[i] - '0');
    }
    if (flen > L80R_CONSTANT_5) {
        flen = L80R_CONSTANT_5; // 最多取五位小数
    }
    *dx = flen;
    for (i = 0; i < flen; i++) { // 得到小数部分数据
        fres += NMEA_Pow(L80R_CONSTANT_10, flen - 1 - i) * (buf[ilen + 1 + i] - '0');
    }
    res = ires * NMEA_Pow(L80R_CONSTANT_10, flen) + fres;
    if (mask & 0x02)
        res = -res;
    return res;
}
/***************************************************\
* 函数名称: NMEA_BDS_GPRMC_Analysis
*	函数功能：解析GPRMC信息
*	输入值：gpsx,NMEA信息结构体
*				 buf：接收到的GPS数据缓冲区首地址
\***************************************************/
void NMEA_BDS_GPRMC_Analysis(gps_msg *gpsmsg, uint8_t *buf)
{
    uint8_t *p4, dx;
    uint8_t posx;
    uint32_t temp;
    float rs;
    p4 = (uint8_t *)strstr((const char *)buf, "$GPRMC"); // "$GPRMC",经常有&和GPRMC分开的情况,故只判断GPRMC.
    if (p4 != NULL) {
        posx = NMEA_Comma_Pos(p4, L80R_CONSTANT_2); // 得到纬度
        if (posx != 0XFF) {
            temp = NMEA_Str2num(p4 + posx, &dx);
            gpsmsg->latitude_bd = temp / NMEA_Pow(L80R_CONSTANT_10, dx + L80R_CONSTANT_2);  // 得到°
            rs = temp % NMEA_Pow(L80R_CONSTANT_10, dx + L80R_CONSTANT_2);                   // 得到'
            gpsmsg->latitude_bd = gpsmsg->latitude_bd * NMEA_Pow(L80R_CONSTANT_10, L80R_CONSTANT_5) +
                (rs * NMEA_Pow(L80R_CONSTANT_10, L80R_CONSTANT_5 - dx)) / L80R_CONSTANT_60; // 转换为°
        }
        posx = NMEA_Comma_Pos(p4, L80R_CONSTANT_4); // 南纬还是北纬
        if (posx != 0XFF) {
            gpsmsg->nshemi_bd = *(p4 + posx);
        }
        posx = NMEA_Comma_Pos(p4, L80R_CONSTANT_5); // 得到经度
        if (posx != 0XFF) {
            temp = NMEA_Str2num(p4 + posx, &dx);
            gpsmsg->longitude_bd = temp / NMEA_Pow(L80R_CONSTANT_10, dx + L80R_CONSTANT_2); // 得到°
            rs = temp % NMEA_Pow(L80R_CONSTANT_10, dx + L80R_CONSTANT_2);                   // 得到'
            gpsmsg->longitude_bd = gpsmsg->longitude_bd * NMEA_Pow(L80R_CONSTANT_10, L80R_CONSTANT_5) +
                (rs * NMEA_Pow(L80R_CONSTANT_10, L80R_CONSTANT_5 - dx)) / L80R_CONSTANT_60; // 转换为°
        }
        posx = NMEA_Comma_Pos(p4, L80R_CONSTANT_6); // 东经还是西经
        if (posx != 0XFF)
            gpsmsg->ewhemi_bd = *(p4 + posx);
    }
}

/***************************************************************
 * 函数名称: E53ST1ReadData
 * 说    明: 获取GPS经纬度信息
 * 参    数: 无
 * 返 回 值: 无
 ***************************************************************/
void E53ST1ReadData(E53ST1Data *ReadData)
{
    // 通过串口1接收数据
    IoTUartRead(WIFI_IOT_UART_IDX_1, gps_uart, L80R_DATA_LEN);
    NMEA_BDS_GPRMC_Analysis(&gpsmsg, (uint8_t *)gps_uart); // 分析字符串
    ReadData->Longitude = (float)((float)gpsmsg.longitude_bd / L80R_COEFFICIENT);
    ReadData->Latitude = (float)((float)gpsmsg.latitude_bd / L80R_COEFFICIENT);
}

/***************************************************************
 * 函数名称: BeepStatusSet
 * 说    明: 蜂鸣器报警与否
 * 参    数: status,ENUM枚举的数据
 *									OFF,蜂鸣器
 *									ON,开蜂鸣器
 * 返 回 值: 无
 ***************************************************************/
void BeepStatusSet(E53ST1Status status)
{
    if (status == ON) {
        IoTPwmStart(WIFI_IOT_PWM_PORT_PWM1, PWM_DUTY, PWM_FREQ); // 输出不同占空比的PWM波
    }

    if (status == OFF) {
        IoTPwmStop(WIFI_IOT_PWM_PORT_PWM1);
    }
}
