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
#include <stdlib.h>
#include <memory.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "iot_i2c.h"
#include "iot_errno.h"
#include "hi_errno.h"
#include "hi_i2c.h"
#include "iot_gpio_ex.h"
#include "iot_watchdog.h"
#include "iot_gpio.h"
#include "hi_time.h"
#include "motor_control.h"
#include "pca9555.h"

#define PCA5555_I2C_IDX 0
#define IOT_I2C_IDX_BAUDRATE         400000 // 400k
#define PCA9555_INT_PIN_NAME    (IOT_IO_NAME_GPIO_11)
#define PCA9555_INT_PIN_FUNC    (IOT_IO_FUNC_GPIO_11_GPIO)
#define PCA_REG_IO0_STATE       (0x00)

uint8_t g_ext_io_input = 0;
uint8_t g_ext_io_input_d = 0;
uint8_t g_ext_io_output = 0;
int g_intLowFlag = 0;
uint32_t g_StartTick = 0;
static volatile int g_ext_io_int_valid = 0;

uint32_t PCA_WriteReg(uint8_t reg_addr, uint8_t reg_val)
{
    uint8_t buffer[2];
    
    buffer[0] = reg_addr;
    buffer[1] = reg_val;

    uint32_t retval = IoTI2cWrite(PCA5555_I2C_IDX, PCA9555_WRITE_ADDR, buffer, 2);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite failed\n");
        return retval;
    }
    return IOT_SUCCESS;
}

/**
 * @berf i2c read
 * @param hi_u8 reg_high_8bit_cmd:Transmit register value 8 bits high
 * @param hi_u8 reg_low_8bit_cmd:Transmit register value low 8 bits
 * @param hi_u8* recv_data:Receive data buff
 * @param hi_u8 send_len:Sending data length
 * @param hi_u8 read_len:Length of received data
*/
static uint32_t PCA95555_WriteRead(uint8_t reg_high_8bit_cmd, uint8_t send_len, uint8_t read_len)
{
    uint32_t status = 0;
    uint32_t ret = 0;
    uint8_t recvData[888] = {0};
    hi_i2c_data c081nfc_i2c_write_cmd_addr = { 0 };
    uint8_t send_user_cmd[1] = {reg_high_8bit_cmd};
    memset(recvData, 0x0, sizeof(recvData));
    c081nfc_i2c_write_cmd_addr.send_buf = send_user_cmd;
    c081nfc_i2c_write_cmd_addr.send_len = send_len;

    c081nfc_i2c_write_cmd_addr.receive_buf = recvData;
    c081nfc_i2c_write_cmd_addr.receive_len = read_len;

    status = hi_i2c_writeread(PCA5555_I2C_IDX, PCA9555_READ_ADDR, &c081nfc_i2c_write_cmd_addr);
    if (status != HI_ERR_SUCCESS) {
        return status;
    }
    ret = recvData[0];
    return ret;
}

static uint32_t PCA95555_Write(uint8_t* buffer, uint32_t buffLen)
{
    uint32_t retval = IoTI2cWrite(PCA5555_I2C_IDX, PCA9555_WRITE_ADDR, buffer, buffLen);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    return IOT_SUCCESS;
}

void PCA_Gpio_Config(uint8_t addr, uint8_t buffer, uint32_t buffLen)
{
    uint8_t write[WRITELEN] = {addr, buffer};
    PCA95555_Write(write, buffLen);
}

void LeftLED(void)
{
    PCA_WriteReg(PCA9555_REG_OUT1, LEFT_LED); /* IO1 012345低电平 */
}

void RightLed(void)
{
    PCA_WriteReg(PCA9555_REG_OUT1, RIGHT_LED); /* IO1 012345低电平 */
}

void LedOff(void)
{
    PCA_WriteReg(PCA9555_REG_OUT1, LED_OFF); /* IO1 012345低电平 */
}

void PressToRestore(void)
{
    IotGpioValue value = 0;
    uint8_t status;
    status = IoTGpioGetInputVal(IOT_IO_NAME_GPIO_11, &value);
    if (status != IOT_SUCCESS) {
        printf("status = %d\r\n", status);
    }
    if (value == 1) {
        g_intLowFlag = 0;
    } else {
        if (g_intLowFlag == 0) {
            g_StartTick = hi_get_milli_seconds();
            g_intLowFlag = 1;
        } else {
            if ((hi_get_milli_seconds() - g_StartTick) > 2) { // 2ms复位pca9555
                g_ext_io_input = PCA95555_WriteRead(PCA9555_REG_IN0, 1, 1);
                g_intLowFlag = 0;
            }
        }
    }
}

void PCA9555_int_proc(void)
{
    uint8_t diff;
    if (g_ext_io_int_valid == 1) {
        g_ext_io_input = PCA95555_WriteRead(PCA9555_REG_IN0, 1, 1);
        diff = g_ext_io_input ^ g_ext_io_input_d;
        /* ext io 0 - 1: lighting sensor */
        if (diff & 0x01) {
            if (g_ext_io_input & 0x01) { // 0x01左边大灯
                printf("left lighten\n");
                LeftLED();
            } else {
                printf("left darken\n");
                LedOff();
            }
        }
        if (diff & 0x02) {
            if (g_ext_io_input & 0x02) { // 0x02右边大灯
                printf("right lighten\n");
                RightLed();
            } else {
                printf("right darken\n");
                LedOff();
            }
        }
        g_ext_io_int_valid = 0;
        g_ext_io_input_d = g_ext_io_input;
    } else {
        PressToRestore();
    }
}


void PCA95555TestTask(void)
{
    PCA_Gpio_Config(PCA9555_REG_CFG0, 0x1F, 2);     /* 0x1f,2长度按键加编码器 */
    PCA_Gpio_Config(PCA9555_REG_CFG1, 0x00, 2);     /* IO1 012345输出 */
    PCA_Gpio_Config(PCA9555_REG_OUT1, LED_OFF, 2);  /* IO1 012345低电平 */
    while (1) {
        PCA9555_int_proc();
        static int time_stamp = 0;
        if ((hi_get_milli_seconds() - time_stamp) > 100) { // 100ms判断一次寻迹模块
            time_stamp = hi_get_milli_seconds();
            if ((g_ext_io_input & 0x03) == 0x02) { // 与0x03,等于0x02代表左轮在黑线
                car_left();
            } else if ((g_ext_io_input & 0x03) == 0x01) { // 与0x03,等于0x01代表右轮在黑线
                car_right();
            } else if ((g_ext_io_input & 0x03) == 0x03) { // 与0x03,等于0x03代表两轮在黑线
                car_stop();
            } else {
                car_forward();
            }
        }
    }
}

void OnExtIoTriggered(char *arg)
{
    (void) arg;
    g_ext_io_int_valid = 1;
}

void PCA95555GpioInit(void)
{
    IoTI2cInit(0, IOT_I2C_IDX_BAUDRATE); /* baudrate: 400000 */
    IoTI2cSetBaudrate(0, IOT_I2C_IDX_BAUDRATE);
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);

    IoTGpioInit(IOT_IO_NAME_GPIO_11);
    IoSetFunc(IOT_IO_NAME_GPIO_11, IOT_IO_FUNC_GPIO_11_GPIO);
    IoTGpioSetDir(IOT_IO_NAME_GPIO_11, IOT_GPIO_DIR_IN);
    IoSetPull(IOT_IO_NAME_GPIO_11, IOT_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(IOT_IO_NAME_GPIO_11, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnExtIoTriggered, NULL);
}

void TraceDemoTest(void)
{
    osThreadAttr_t attr;
    PCA95555GpioInit();
    GA12N20Init();
    IoTWatchDogDisable();
    attr.name = "PCA95555Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 任务栈大小5*1024
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)PCA95555TestTask, NULL, &attr) == NULL) {
        printf("[robot_car_demo] Failed to create Aht20TestTask!\n");
    }
}
APP_FEATURE_INIT(TraceDemoTest);