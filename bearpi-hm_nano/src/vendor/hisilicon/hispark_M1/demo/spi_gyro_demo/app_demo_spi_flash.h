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

#ifndef APP_DEMO_SPI_FLASH_H
#define APP_DEMO_SPI_FLASH_H

#define SECTORERASE    0X20
#define CMD_PAGE_PROGRAM    0x02
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_DATA_BYTES    0x03

void GD25Q40C_Init(hi_spi_idx id);
void GD25Q40C_Write_Reg(hi_spi_idx id, unsigned char *writebuff, unsigned int writelen);
void GD25Q40C_Write_Read_Reg(hi_spi_idx id, unsigned char *writedata, unsigned char *readdata,
                             unsigned int readdatalen);

#endif
