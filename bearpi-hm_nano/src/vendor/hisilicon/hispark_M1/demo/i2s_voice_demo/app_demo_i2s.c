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

#include <ohos_init.h>
#include <cmsis_os2.h>
#include "hi_io.h"
#include "hi_gpio.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "iot_i2c.h"
#include "app_demo_i2s.h"

#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define SSD1306_I2C_IDX 0

audio_map g_audio_map[3] = {  /* 2 flash fields */
    {0x001A1000, 100*1024}, /* audio file size: 400 * 1024(100K) */
    {0x001CE000, 204800},     /* recod size: 200K */
    {0x1F0000, 0xC000},
};

unsigned char es8311_register_buff[] = {
    ES8311_RESET_REG00, ES8311_CLK_MANAGER_REG01, ES8311_CLK_MANAGER_REG02, ES8311_CLK_MANAGER_REG03,
    ES8311_CLK_MANAGER_REG04, ES8311_CLK_MANAGER_REG05, ES8311_CLK_MANAGER_REG06,
    ES8311_CLK_MANAGER_REG07, ES8311_CLK_MANAGER_REG08, ES8311_SDPIN_REG09,
    ES8311_SDPOUT_REG0A, ES8311_SYSTEM_REG0B, ES8311_SYSTEM_REG0C, ES8311_SYSTEM_REG0D,
    ES8311_SYSTEM_REG0E, ES8311_SYSTEM_REG0F, ES8311_SYSTEM_REG10, ES8311_SYSTEM_REG11,
    ES8311_SYSTEM_REG12, ES8311_SYSTEM_REG13, ES8311_SYSTEM_REG14, ES8311_ADC_REG15,
    ES8311_ADC_REG16, ES8311_ADC_REG17, ES8311_ADC_REG18, ES8311_ADC_REG19, ES8311_ADC_REG1A,
    ES8311_ADC_REG1B, ES8311_ADC_REG1C, ES8311_DAC_REG31, ES8311_DAC_REG32, ES8311_DAC_REG33,
    ES8311_DAC_REG34, ES8311_DAC_REG35, ES8311_DAC_REG37, ES8311_GPIO_REG44, ES8311_GP_REG45,
    ES8311_CHD1_REGFD, ES8311_CHD2_REGFE, ES8311_CHVER_REGFF, ES8311_MAX_REGISTER,
};

unsigned int g_audio_event_test;
unsigned int g_audio_task_id_test;
test_audio_attr g_audio_test_demo;
unsigned char g_record_data[AUDIO_RECORD_BUF_SIZE] = { 0 };

unsigned int es8311_codec_init_test(const hi_codec_attribute *codec_attr)
{
    unsigned int ret;
    if (codec_attr == HI_NULL) {
        return -1;
    }
    if (ret != HI_ERR_SUCCESS) {
        printf("==ERROR== hi_i2c_init, err = %u\n", ret);
        return -1;
    }
    ret = hi_codec_init_test(codec_attr);
    if (ret != HI_ERR_SUCCESS) {
        printf("==ERROR== Failed to init codec!! err = %u\n", ret);
        return -1;
    } else {
        printf("init codec success!\n");
    }
    return 0;
}

unsigned int audio_play_test(unsigned int map_index)
{
    unsigned int ret;
    unsigned int play_addr = g_audio_map[map_index].flash_start_addr;
    unsigned int total_play_len = g_audio_map[map_index].data_len;
    unsigned int time_out = HI_SYS_WAIT_FOREVER;

    /* apply memory */
    g_audio_test_demo.play_buf = (unsigned char *) hi_malloc(HI_MOD_ID_DRV, AUDIO_PLAY_BUF_SIZE);
    if (g_audio_test_demo.play_buf == HI_NULL) {
        hi_i2s_deinit();
        printf("==ERROR== play buf malloc fail!!!\n");
        return -1;
    }
    memset_s(g_audio_test_demo.play_buf, AUDIO_PLAY_BUF_SIZE, 0, AUDIO_PLAY_BUF_SIZE);

    while (total_play_len > 0) {
        hi_u32 send_len = hi_min(total_play_len, AUDIO_PLAY_BUF_SIZE);
        ret = hi_flash_read(play_addr, send_len, g_audio_test_demo.play_buf);
        if (ret != HI_ERR_SUCCESS) {
            printf("==ERROR== hi_flash_read fail, err = %u\n", ret);
            return -1;
        }

        ret = hi_i2s_write(g_audio_test_demo.play_buf, send_len, time_out);
        if (ret != HI_ERR_SUCCESS) {
            printf("hi_i2s_write fail, err = %u\n", ret);
            return -1;
        }

        play_addr += send_len;
        total_play_len -= send_len;
    }

    printf("Play over....\n");

    hi_free(HI_MOD_ID_DRV, g_audio_test_demo.play_buf);
    return 0;
}

unsigned int audio_record_func_test(unsigned int map_index)
{
    unsigned int ret;
    unsigned int record_addr = g_audio_map[map_index].flash_start_addr;
    unsigned int total_record_len = g_audio_map[map_index].data_len;

    ret = hi_flash_erase(record_addr, total_record_len);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to erase flash, err = %u\n", ret);
        return -1;
    }

    while (total_record_len > 0) {
        unsigned int len = hi_min(AUDIO_RECORD_BUF_SIZE, total_record_len);
        ret = hi_i2s_read(g_audio_test_demo.record_buf, len, 400); // 超时400ms
        if (ret != HI_ERR_SUCCESS) {
            printf("Failed to hi_i2s_read, err = %u\n", ret);
            return -1;
        }
        if (memcpy_s(g_record_data, sizeof(g_record_data), g_audio_test_demo.record_buf, len) != EOK) {
            return -1;
        }
        hi_event_send(g_audio_event_test, AUDIO_RECORD_FINISH_BIT);
        record_addr += len;
        total_record_len -= len;
    }

    hi_event_send(g_audio_event_test, ALL_AUDIO_RECORD_FINISH_BIT);
    return 0;
}

void record_n_play_test_task(void)
{
    unsigned int ret;
    unsigned int event_bit = 0;
    unsigned int record_addr = g_audio_map[AUDIO_RECORD_AND_PLAY_MODE].flash_start_addr;
    unsigned int total_record_len = g_audio_map[AUDIO_RECORD_AND_PLAY_MODE].data_len;
    unsigned int len;

    while (1) {
        hi_event_wait(g_audio_event_test, AUDIO_RECORD_FINISH_BIT | ALL_AUDIO_RECORD_FINISH_BIT, &event_bit,
            HI_SYS_WAIT_FOREVER, HI_EVENT_WAITMODE_OR | HI_EVENT_WAITMODE_CLR);
        if (event_bit & ALL_AUDIO_RECORD_FINISH_BIT) {
            break;
        }
        len = hi_min(AUDIO_RECORD_BUF_SIZE, total_record_len);

        ret = hi_flash_write(record_addr, len, g_record_data, HI_FALSE);
        if (ret != HI_ERR_SUCCESS) {
            printf("==ERROR== hi_flash_write, err = %u\n", ret);
        }
        record_addr += len;
        total_record_len -= len;
    }
    ssd1306_ClearOLED();
    ssd1306_SetCursor(25, 10); // x轴坐标为25，y轴坐标为10
    ssd1306_DrawString("Record success!", Font_7x10, White);
    ssd1306_UpdateScreen();
    printf("Record success!...\n");
    TaskMsleep(1000); /* 1000ms: delay 1s */
    audio_play_test(AUDIO_RECORD_AND_PLAY_MODE);
    printf("Play record audio success!...\n");
    return HI_NULL;
}


void audio_record_play_test(unsigned int map_index)
{
    unsigned int ret;
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "I2STask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = I2S_TEST_TASK_STAK_SIZE; // 任务栈大小为5 *1024
    attr.priority = I2S_TEST_TASK_PRIORITY;
    if (osThreadNew((osThreadFunc_t)record_n_play_test_task, NULL, &attr) == NULL) {
        printf("[I2STask] Failed to create BalanceTask!\n");
    }

    /* apply memory */
    g_audio_test_demo.record_buf = (unsigned char *)hi_malloc(HI_MOD_ID_DRV, AUDIO_RECORD_BUF_SIZE);
    if (g_audio_test_demo.record_buf == HI_NULL) {
        hi_i2s_deinit();
        printf("==ERROR== record buf malloc fail!!!\n");
        return;
    }
    memset_s(g_audio_test_demo.record_buf, AUDIO_RECORD_BUF_SIZE, 0, AUDIO_RECORD_BUF_SIZE);
    ssd1306_SetCursor(25, 10); // x轴坐标为25，y轴坐标为10
    ssd1306_DrawString("start record", Font_7x10, White);
    ssd1306_UpdateScreen();
    printf("==start record== please say somerthing~~\n");
    audio_record_func_test(map_index);
}

void i2sGpioint(void)
{
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_I2S0_MCLK);
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_I2S0_TX);
    IoSetFunc(IOT_IO_NAME_GPIO_11, IOT_IO_FUNC_GPIO_11_I2S0_RX);
    IoSetFunc(IOT_IO_NAME_GPIO_12, IOT_IO_FUNC_GPIO_12_I2S0_BCLK);
    IoSetFunc(IOT_IO_NAME_GPIO_8, IOT_IO_FUNC_GPIO_8_I2S0_WS);

    /* BCLK */
    IoTGpioSetDir(IOT_IO_NAME_GPIO_12, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE1);
    /*
     * 初始化I2C设备0，并指定波特率为400k
     * Initialize I2C device 0 and specify the baud rate as 400k
     */
    IoTI2cInit(SSD1306_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /*
     * 设置I2C设备0的波特率为400k
     * Set the baud rate of I2C device 0 to 400k
     */
    IoTI2cSetBaudrate(SSD1306_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
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
    ssd1306_Init();
    ssd1306_ClearOLED();
}

/*
 * i2s_demo: a simple demo implement paly audio file and record audio then play them back function.
 * -note: If it is in play mode, user need to burn the audio file to the specified location of flash in advance.
 *        max size of audio file: 100K bytes
 *        burn command:           burn 1A1000 19000
 */
void i2s_demo_test(void)
{
    unsigned int ret;
    i2sGpioint();
    ret = hi_flash_init();
    if (ret == HI_ERR_FLASH_RE_INIT) {
        printf("Flash has already been initialized!\n");
    } else if (ret != HI_ERR_SUCCESS) {
        printf("Falied to init flash, err = %u\n", ret);
    }

    /* create I2S record event */
    ret = hi_event_create(&g_audio_event_test);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to init g_audio_event_test! err = %u\n", ret);
        return;
    }

    hi_codec_attribute codec_cfg = {
        .sample_rate = HI_CODEC_SAMPLE_RATE_8K,
        .resolution = HI_CODEC_RESOLUTION_16BIT,
    };
    hi_i2s_attribute i2s_cfg = {
        .sample_rate = HI_I2S_SAMPLE_RATE_8K,
        .resolution = HI_I2S_RESOLUTION_16BIT,
    };
    es8311_codec_init_test(&codec_cfg);
    ret = hi_i2s_init(&i2s_cfg);
    if (ret != HI_ERR_SUCCESS) {
        printf("Failed to init i2s %u!\n", ret);
        return;
    }
    printf("I2s init success!\n");
    audio_record_play_test(AUDIO_RECORD_AND_PLAY_MODE);
}

APP_FEATURE_INIT(i2s_demo_test);