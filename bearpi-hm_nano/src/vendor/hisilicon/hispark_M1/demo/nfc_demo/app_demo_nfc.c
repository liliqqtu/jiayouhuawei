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
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_stdlib.h"
#include "iot_gpio_ex.h"
#include "hi_time.h"
#include "iot_gpio.h"
#include "iot_watchdog.h"
#include "iot_i2c.h"
#include "iot_errno.h"
#include "hi_i2c.h"
#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "iot_i2c.h"
#include "app_demo_nfc.h"

unsigned char ndefFile[NFC_NDEF_MAX_LEN] = {
    // 微信
    0x03, 0x20,
    0xd4, 0x0f, 0x0e, 0x61, 0x6e, 0x64, 0x72, 0x6f,
    0x69, 0x64, 0x2e, 0x63, 0x6f, 0x6d, 0x3a, 0x70,
    0x6b, 0x67, 0x63, 0x6f, 0x6d, 0x2e, 0x74, 0x65,
    0x6e, 0x63, 0x65, 0x6e, 0x74, 0x2e, 0x6d, 0x6d,
};

static void PullUpCsn(void)
{
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
}

static void PullDownCsn(void)
{
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
}

void gpioInit(void)
{
    /*
     * 初始化I2C设备0，并指定波特率为400k
     * Initialize I2C device 0 and specify the baud rate as 400k
     */
    IoTI2cInit(NFC_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /*
     * 设置I2C设备0的波特率为400k
     * Set the baud rate of I2C device 0 to 400k
     */
    IoTI2cSetBaudrate(NFC_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /*
     * 设置GPIO13的管脚复用关系为I2C0_SDA
     * Set the pin reuse relationship of GPIO13 to I2C0_ SDA
     */
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    /*
     * 设置GPIO14的管脚复用关系为I2C0_SCL
     * Set the pin reuse relationship of GPIO14 to I2C0_ SCL
     */
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
}

static unsigned int C081NfcI2cWrite(unsigned char regHigh8bitCmd,
    unsigned char regLow8bitCmd, unsigned char* recvData, unsigned char sendLen)
{
    unsigned int status = 0;
    IotI2cData c081NfcI2cWriteCmdAddr = { 0 };
    unsigned char sendUserCmd[64] = {regHigh8bitCmd, regLow8bitCmd};

    c081NfcI2cWriteCmdAddr.sendBuf = sendUserCmd;
    c081NfcI2cWriteCmdAddr.sendLen = 2 + sendLen; // 2代表两位地址长度，2 represents the two bit address length
    for (unsigned int i = 0; i < sendLen; i++) {
        sendUserCmd[ 2 + i] = *(recvData + i); // 2代表两位地址长度，2 represents the two bit address length
    }
    status = IoTI2cWrite(NFC_I2C_IDX, C081_NFC_WRITE_ADDR,
                         c081NfcI2cWriteCmdAddr.sendBuf, c081NfcI2cWriteCmdAddr.sendLen);
    if (status != IOT_SUCCESS) {
        printf("I2cWrite(%02X) failed, %u!\n", sendUserCmd[0], status);
        return status;
    }
    return IOT_SUCCESS;
}

/*
 * @berf EEPROM page write
 * @param unsigned char *pBuffer: Send data buff
 * @param unsigned short WriteAddr:Send register address
 * @param unsigned char datalen:Sending data length
 */
void EepWritePage(unsigned char *pBuffer, unsigned short WriteAddr, unsigned char datalen)
{
    unsigned int status;
    if (pBuffer == NULL) {
        printf("eepWritePage buffer is null\r\n");
    }
    PullDownCsn();
    /* 代表地址高8位和低8位，2代表发送的数据长度 */
    /* represents the high and low 8 bits of the address, and 2 represents the length of the data sent */
    status = C081NfcI2cWrite((unsigned char)((WriteAddr & 0xFF00) >> 8),
                             (unsigned char)(WriteAddr & 0x00FF), pBuffer, datalen);
    if (status != IOT_SUCCESS) {
        printf("write failed\r\n");
    }
    TaskMsleep(10); // The delay time must be 10ms
    PullUpCsn();
}

/*
 * @berf write EEPROM
 * @param unsigned short addr:eeprom address
 * @param unsigned int len:Write data length
 * @param unsigned char *wbuf:write data buff
 */
void Fm11nt081dWriteEeprom(unsigned short baseAddr, unsigned int len, unsigned char *wbuf)
{
    unsigned char offset = 0;
    unsigned char *writeBuff = wbuf;
    unsigned int writeLen = len;
    unsigned short addr = baseAddr;
    if (writeBuff == NULL) {
        printf("write ndef is null\r\n");
        return;
    }
    if (addr < FM11_E2_USER_ADDR || addr >= FM11_E2_MAUNF_ADDR) {
        offset = FM11_E2_BLOCK_SIZE - (addr % FM11_E2_BLOCK_SIZE);
        if (writeLen > offset) {
            EepWritePage(writeBuff, addr, offset);
            addr += offset;
            writeBuff += offset;
            writeLen -= offset;
        } else {
            EepWritePage(writeBuff, addr, writeLen);
            writeLen = 0;
        }
    }
    while (writeLen) {
        if (writeLen >= FM11_E2_BLOCK_SIZE) {
            EepWritePage(writeBuff, addr, FM11_E2_BLOCK_SIZE);
            addr += FM11_E2_BLOCK_SIZE;
            writeBuff += FM11_E2_BLOCK_SIZE;
            writeLen -= FM11_E2_BLOCK_SIZE;
        } else {
            EepWritePage(writeBuff, addr, writeLen);
            writeLen = 0;
        }
    }
}

/*
 * @berf i2c read
 * @param unsigned char reg_high_8bit_cmd:Transmit register value 8 bits high
 * @param unsigned char reg_low_8bit_cmd:Transmit register value low 8 bits
 * @param unsigned char* recv_data:Receive data buff
 * @param unsigned char send_len:Sending data length
 * @param unsigned char read_len:Length of received data
 */
unsigned int WriteRead(unsigned char regHigh8bitCmd, unsigned char regLow8bitCmd,
    unsigned char sendLen, unsigned char readLen)
{
    unsigned int status = 0;
    unsigned char recvData[888] = { 0 }; // 888代表recvData长度，888 stands for recvData length
    hi_i2c_data c081NfcI2cWriteCmdAddr = { 0 };
    unsigned char sendUserCmd[2] = {regHigh8bitCmd, regLow8bitCmd};
    memset_s(&recvData, sizeof(recvData), 0x0, sizeof(recvData));

    c081NfcI2cWriteCmdAddr.send_buf = sendUserCmd;
    c081NfcI2cWriteCmdAddr.send_len = sendLen;

    c081NfcI2cWriteCmdAddr.receive_buf = recvData;
    c081NfcI2cWriteCmdAddr.receive_len = readLen;

    status = hi_i2c_writeread(NFC_I2C_IDX, C081_NFC_ADDR | I2C_RD, &c081NfcI2cWriteCmdAddr);
    if (status != IOT_SUCCESS) {
        printf("hi_i2c_writeread failed, %u!\n", status);
        return status;
    }
    return IOT_SUCCESS;
}

/*
 * @berf read EEPROM
 * @param unsigned char *dataBuff:Read data and save it in buff
 * @param unsigned short ReadAddr:Read address
 * @param unsigned short len:Read length
 */
unsigned int Fm11nt081ReadEep(unsigned short ReadAddr, unsigned short len)
{
    unsigned int status;
    /* 左移8位代表地址高8位和低8位，2代表发送的数据长度 */
    /* represents the high and low 8 bits of the address, and 2 represents the length of the data sent */
    status = WriteRead((unsigned char)((ReadAddr & 0xFF00) >> 8), (unsigned char)(ReadAddr & 0x00FF), 2, len);
    if (status != IOT_SUCCESS) {
        return status;
    }
    return  IOT_SUCCESS;
}

/* NFC chip configuration, usually do not call NFC init */
void NFCInit(void)
{
    // Chip default configuration，wbuf len 5
    unsigned char wbuf[5] = {NFC_REGISTER1, NFC_REGISTER2, NFC_REGISTER3, NFC_REGISTER4, NFC_REGISTER5};
    /* The CSN pin is masked when the byte is read and turned on when the EEP is written */
    hi_udelay(100); // 延时100us读写数据,wait 100us read write data
    Fm11nt081dWriteEeprom(NFC_EERROM_ONE_ADDR, 1, &wbuf[1]);
    Fm11nt081dWriteEeprom(NFC_EERROM_TWO_ADDR, 1, &wbuf[3]); // wbuf第3个
    Fm11nt081dWriteEeprom(NFC_EERROM_BASE_ADD, NFC_TOUTIAO_NDEF_LEN, ndefFile);
    PullDownCsn();
    Fm11nt081ReadEep(NFC_EERROM_BASE_ADD, READ_NFC_TOUTIAO_NDEF_LEN);
}

/* app nfc demo */
void NFCDemoTask(void)
{
    ssd1306_Init();
    ssd1306_ClearOLED();
    ssd1306_SetCursor(25, 10); // x轴坐标为25，y轴坐标为10
    ssd1306_DrawString("Wechart", Font_7x10, White);
    ssd1306_UpdateScreen();
    NFCInit();
}

void C081NFCExampleEntry(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "NFCDemoTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 堆栈大小为5*1024，stack size 5*1024
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)NFCDemoTask, NULL, &attr) == NULL) {
        printf("[NFCDemoTask] Falied to create NFCDemoTask!\n");
    }
}

APP_FEATURE_INIT(C081NFCExampleEntry);