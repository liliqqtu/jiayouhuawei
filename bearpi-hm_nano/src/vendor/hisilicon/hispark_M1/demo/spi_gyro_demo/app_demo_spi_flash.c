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

#include <hi_early_debug.h>
#include <hi_task.h>
#include <hi_types_base.h>
#include <hi_spi.h>
#include <hi_time.h>
#include <../platform/drivers/spi/spi.h>
#include <hi_gpio.h>
#include "hi_types_base.h"
#include "math.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "app_demo_spi_flash.h"

void GD25Q40C_Write_Read_Reg(hi_spi_idx id, unsigned char *writedata, unsigned char *readdata, unsigned int readdatalen)
{
    int ret = 0;
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE0);  // 使能SPI传输
    ret = memset_s(readdata, readdatalen + 1, 0x0, readdatalen);
    if (ret != EOK) {
        printf("memcpy_s failed, err = %d\n", ret);
    }
    ret = hi_spi_host_writeread(id, writedata, readdata, readdatalen);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi read[%02X] fail! %d", readdata[0], ret);
    }
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE1);  // 使能SPI传输
}

void GD25Q40C_Write_Reg(hi_spi_idx id, unsigned char *writebuff, unsigned int writelen)
{
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE0);  // 使能SPI传输
    int ret = hi_spi_host_write(id, writebuff, writelen);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi write[%02X] fail! %d", writebuff[0], ret);
    }
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE1);  // 使能SPI传输
}

void GD25Q40C_Init(hi_spi_idx id)
{
    unsigned char read_buff[2] = { 0 };
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE0);  // 使能SPI传输
    unsigned char writebuff[4] = {0x90, 0x00, 0x00, 0x00}; // 读取芯片的device ID寄存器分别为0x90, 0x00, 0x00, 0x00
    int ret = hi_spi_host_write(id, writebuff, 4);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi write[%02X] fail! %d", writebuff[0], ret);
    }
    ret = hi_spi_host_read(id, read_buff, 2); // 读取长度为2
    if (ret != HI_ERR_SUCCESS) {
        printf("spi read[%02X] fail! %d", read_buff[0], ret);
    }
    hi_gpio_set_ouput_val(HI_GPIO_IDX_0, HI_GPIO_VALUE1);
    for (int i = 0; i < 2; i++) { // 读取长度为2
        printf("readdata[%d] = %02x\r\n", i, read_buff[i]);
    }
    printf("\r\n");
    if (read_buff[0] != 0xc8 || read_buff[1] != 0x12) { // 芯片device ID 0xc8 和 0x12
        printf("GD25Q40C init failed\r\n");
        return;
    }
    return HI_ERR_SUCCESS;
}

// 读取SPI_FLASH的状态寄存器
// BIT7  6   5   4   3   2   1   0
// SPR   RV  TB BP2 BP1 BP0 WEL BUSY
// SPR:默认0,状态寄存器保护位,配合WP使用
// TB,BP2,BP1,BP0:FLASH区域写保护设置
// WEL:写使能锁定
// BUSY:忙标记位(1,忙;0,空闲)
// 默认:0x00
void GD25Q40C_spi_flash_wait_busy(hi_spi_idx id)
{
    unsigned char ReadStatusReg = 0x05; // 读取状态寄存器地址位0x05
    unsigned char readdata[2] = { 0 }; // 读取数据长度为2
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE0);  // 使能SPI传输
    int ret = hi_spi_host_write(id, &ReadStatusReg, 1);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi write fail! %d", ret);
    }
    do {
        hi_spi_host_read(id, readdata, 2); // 读取数据长度为2
    }  while ((readdata[0] & 0x01) == 0x01); // 判断如果为0x01为繁忙

    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE1);  // 使能SPI传输
}

void GD25Q40C_Write_Enable(hi_spi_idx id)
{
    unsigned char WriteEnable = CMD_WRITE_ENABLE;
    GD25Q40C_Write_Reg(id, &WriteEnable, 1);
}

void GD25Q40C_SPIFLASH_EraseSector(hi_spi_idx id, unsigned int SectorAddr)
{
    /* 发送FLASH写使能命令 */
    GD25Q40C_Write_Enable(id);
    GD25Q40C_spi_flash_wait_busy(id);
    unsigned char addrbuff[4] = { 0 }; // 擦除地址位长度为4
    addrbuff[0] = SECTORERASE;
    addrbuff[1] = (unsigned char)SectorAddr >> 16; // 地址位右移16位
    addrbuff[2] = (unsigned char)SectorAddr >> 8; // addrbuff[2]为地址位右移8位
    addrbuff[3] = (unsigned char)SectorAddr; // addrbuff[3]为地址位
    GD25Q40C_Write_Reg(id, addrbuff, 4); // 写地址位长度为4
    GD25Q40C_spi_flash_wait_busy(id);
}
 
void GD25Q_SPIFLASH_ReadBuffer(hi_spi_idx id, unsigned int ReadAddr)
{
    unsigned char read_buff[16] = { 0 }; // 陀螺仪数据长度为16
    /* 发送读指令，读地址高中低位， 读数据长度 */
    unsigned char addrbuff[4] = { 0 }; // 读取地址位长度为4
    addrbuff[0] = CMD_READ_DATA_BYTES;
    addrbuff[1] = (unsigned char)(ReadAddr) >> 16; // 地址位右移16位
    addrbuff[2] = (unsigned char)(ReadAddr) >> 8; // addrbuff[2]为地址位右移8位
    addrbuff[3] = (unsigned char)(ReadAddr); // addrbuff[3]为地址位
    /* 读取数据 */
    GD25Q40C_Write_Read_Reg(id, addrbuff, read_buff, 16); // 总的数据读取为16
    for (int i = 0; i < 12; i++) { // 陀螺仪有效数据长度为12
        printf("read_buff[%d] = %02x", i + 4, read_buff[i + 4]); // 前4位为地址位，从第五个开始为陀螺仪数据
    }
    printf("\r\n");
}


void GD25Q_SPIFLASH_WritePage(hi_spi_idx id, unsigned char addr, unsigned char *writedata)
{
    /* 发送FLASH写使能命令 */
    GD25Q40C_Write_Enable(id);

    /* 发送页写指令，地址高中低， 数据 */
    unsigned char addrbuff[16] = { 0 }; // 寄存器地址和数据总共有16
    addrbuff[0] = CMD_PAGE_PROGRAM; // 页写指令
    addrbuff[1] = (unsigned char)(addr) >> 16; // addrbuff[1]地址位右移16位
    addrbuff[2] = (unsigned char)(addr) >> 8; // addrbuff[2]地址位右移8位
    addrbuff[3] = (unsigned char)(addr); // addrbuff[3]地址位
    for (int i = 0; i < 12; i++) { // buf数据长度为12
        addrbuff[4 + i] =  writedata[i]; // 前4个数据位写数据寄存器地址位
        printf("writedata[%d] = %02x", i, writedata[i]);
    }
    printf("\r\n");
    GD25Q40C_Write_Reg(id, addrbuff, 16); // 长度为16
    GD25Q40C_spi_flash_wait_busy(id);
}