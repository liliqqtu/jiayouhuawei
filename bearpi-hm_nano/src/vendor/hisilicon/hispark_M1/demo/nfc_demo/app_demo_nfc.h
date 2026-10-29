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

#ifndef APP_DEMO_NFC_H
#define APP_DEMO_NFC_H

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
#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define NFC_I2C_IDX 0

typedef struct {
    /* Pointer to the buffer storing data to send */
    unsigned char *sendBuf;
    /* Length of data to send */
    unsigned int sendLen;
    /* Pointer to the buffer for storing data to receive */
    unsigned char *receiveBuf;
    /* Length of data received */
    unsigned int receiveLen;
} IotI2cData;

void EepWritePage(unsigned char *pBuffer, unsigned short WriteAddr, unsigned char datalen);
void Fm11nt081dWriteEeprom(unsigned short baseAddr, unsigned int len, unsigned char *wbuf);
unsigned int WriteRead(unsigned char regHigh8bitCmd, unsigned char regLow8bitCmd,
    unsigned char sendLen, unsigned char readLen);
unsigned int Fm11nt081ReadEep(unsigned short ReadAddr, unsigned short len);

#endif  // AHT20_H