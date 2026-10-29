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
#include "pca9555.h"
#include "iot_errno.h"
#include "hi_i2c.h"

#define NFC_EERROM_BASE_ADD              (0x0010)
#define NFC_EERROM_FINALY_ADDR           (0x0384)
#define NFC_EERROM_ONE_ADDR              (0x0381)
#define NFC_EERROM_TWO_ADDR              (0x0385)
#define NFC_CMD_LEN                      (2)
#define NFC_WECHAT_NDEF_LEN              (34)
#define READ_NFC_WECHAT_NDEF_LEN         (50)
#define NFC_TOUTIAO_NDEF_LEN             (47)
#define READ_NFC_TOUTIAO_NDEF_LEN        (63)
#define NFC_EERROM_READ_BUFF_LEN_MAX     (888)
#define C081_NFC_ADDR                    (0xAE)
#define I2C_WR                           (0x00)
#define I2C_RD                           (0x01)
#define C081_NFC_WRITE_ADDR              (C081_NFC_ADDR | I2C_WR)
#define C081_NFC_READ_ADDR               (C081_NFC_ADDR | I2C_RD)
#define FM11_E2_USER_ADDR                (0x0010)
#define FM11_E2_MAUNF_ADDR               (0x03FF)
#define FM11_E2_BLOCK_SIZE               (16)
#define DEFAULT_MD_LEN                   (128)
#define RIGHR_MOVE_8_BIT                 (8)
#define NFC_NDEF_MAX_LEN                 (888)
#define OLED_I2C_BAUDRATE                (400*1000)
#define NFC_REGISTER1                    (0x05)
#define NFC_REGISTER2                    (0x78)
#define NFC_REGISTER3                    (0xF7)
#define NFC_REGISTER4                    (0x90)
#define NFC_REGISTER5                    (0x02)
#define NFC_CS_HIGH                      (0x01)
#define NFC_CS_LOW                       (0x00)

static volatile int g_buttonState = 0;

unsigned char ndefFile[NFC_NDEF_MAX_LEN] = {
    // 淘宝 TaoBao
    0x03, 0x23,
    0xd4, 0x0f, 0x11, 0x61, 0x6e, 0x64, 0x72, 0x6f,
    0x69, 0x64, 0x2e, 0x63, 0x6f, 0x6d, 0x3a, 0x70,
    0x6b, 0x67, 0x63, 0x6f, 0x6d, 0x2e, 0x74, 0x61,
    0x6f, 0x62, 0x61, 0x6f, 0x2e, 0x74, 0x61, 0x6f,
    0x62, 0x61, 0x6f,
};

static void PullUpCsn(void)
{
    SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, NFC_CS_HIGH);
}

static void PullDownCsn(void)
{
    SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, NFC_CS_LOW);
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
    status = IoTI2cWrite(PCA9555_I2C_IDX, C081_NFC_WRITE_ADDR,
                         c081NfcI2cWriteCmdAddr.sendBuf, c081NfcI2cWriteCmdAddr.sendLen);
    if (status != IOT_SUCCESS) {
        printf("I2cWrite(%02X) failed, %0X!\n", sendUserCmd[0], status);
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
        printf("offset = %d, writeLen = %d\r\n", offset, writeLen);
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
    (void)memset_s(&recvData, sizeof(unsigned char), 0x0, sizeof(recvData));

    c081NfcI2cWriteCmdAddr.send_buf = sendUserCmd;
    c081NfcI2cWriteCmdAddr.send_len = sendLen;

    c081NfcI2cWriteCmdAddr.receive_buf = recvData;
    c081NfcI2cWriteCmdAddr.receive_len = readLen;

    status = hi_i2c_writeread(PCA9555_I2C_IDX, C081_NFC_ADDR | I2C_RD, &c081NfcI2cWriteCmdAddr);
    if (status != IOT_SUCCESS) {
        printf("hi_i2c_writeread failed, %0X!\n", status);
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

void OnNFCRead(char* arg)
{
    (void) arg;
    g_buttonState = 1;
}

void GetFunKeyState(void)
{
    uint8_t ext_io_state = 0;
    uint8_t ext_io_state_d = 0;
    uint8_t status;

    while (1) {
        if (g_buttonState == 1) {
            uint8_t diff;
            status = PCA9555I2CReadByte(&ext_io_state);
            if (status != IOT_SUCCESS) {
                printf("i2c error!\r\n");
                ext_io_state = 0;
                ext_io_state_d = 0;
                g_buttonState = 0;
                continue;
            }

            diff = ext_io_state ^ ext_io_state_d;
            if (diff == 0) {
            }
            /*
             * 0x40代表NFC INIT接在IO扩展芯片IO01_6
             * 0x40 represents that NFC INIT is connected to IO expansion chip IO01_ six
             */
            if ((diff & 0x40) && ((ext_io_state & 0x40) == 0)) {
                /*
                 * 当有设备读取NFC时，此时会触发NFC的中断，打印 read nfc。
                 * When a device reads the NFC, the NFC interrupt will be triggered and the read nfc will be printed
                 */
                printf("read nfc \r\n");
            }
            status = PCA9555I2CReadByte(&ext_io_state);
            ext_io_state_d = ext_io_state;
            g_buttonState = 0;
        }
        TaskMsleep(20); // 每隔20ms读取一次，Read every 20ms
    }
}

/* app nfc demo */
void NFCDemoTask(void)
{
    PCA9555Init();
    /*
     * 配置IO扩展芯片的part1的所有管脚为输出,0x00所有管脚输出
     * Configure all pins of part1 of IO expansion chip as output, and 0x00 as output
     */
    SetPCA9555GpioValue(PCA9555_PART1_IODIR, 0x00);
    /*
     * 配置左右三色车灯全灭
     * Configured with left and right tricolor lights all off
     */
    SetPCA9555GpioValue(PCA9555_PART1_OUTPUT, LED_OFF);
    /*
     * 0x40代表配置IO0_6方向设置为输入，1为输入，0为输出
     * 0x40 represents IO0 configuration_ 6 direction is set as input, 1 is input, 0 is output
     */
    SetPCA9555GpioValue(PCA9555_PART0_IODIR, 0x40);
    /*
     * 使能GPIO11的中断功能, OnNFCRead 为中断的回调函数
     * Enable the interrupt function of GPIO11. OnNFCRead is the interrupt callback function
     */
    IoTGpioRegisterIsrFunc(IOT_IO_NAME_GPIO_11, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_FALL_LEVEL_LOW, OnNFCRead, NULL);
    NFCInit();
    GetFunKeyState();
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