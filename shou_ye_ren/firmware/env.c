/**
 * @file    env.c
 * @brief   SHT30 / BH1750 读取（Hi3861 I2C0）
 *
 * 依赖：hi_i2c.h、hi_io.h
 *
 * 说明：
 *  - E53 扩展座默认 I2C0：GPIO13=SDA、GPIO14=SCL（以你的板子原理图为准）
 *  - 上电探测一次器件地址：有就周期读，没有就永久置 null，避免每秒刷错误日志
 *  - 不校验 CRC：演示够用；若现场干扰大，把 CRC 校验补上
 */

#include <stdio.h>
#include <string.h>

#include "hi_i2c.h"
#include "hi_io.h"
#include "hi_time.h"

#include "env.h"

#define I2C_SDA_FUNC   HI_IO_FUNC_GPIO_13_I2C0_SDA
#define I2C_SCL_FUNC   HI_IO_FUNC_GPIO_14_I2C0_SCL

static int g_has_th = 0;
static int g_has_lux = 0;

static int i2c_write_bytes(hi_u8 addr, hi_u8 *buf, hi_u32 len)
{
    hi_i2c_data data;
    data.send_buf = buf;
    data.send_len = len;
    data.receive_buf = NULL;
    data.receive_len = 0;
    return (hi_i2c_write(HI_I2C_IDX_0, addr, &data) == HI_ERR_SUCCESS) ? 0 : -1;
}

static int i2c_read_bytes(hi_u8 addr, hi_u8 *buf, hi_u32 len)
{
    hi_i2c_data data;
    data.send_buf = NULL;
    data.send_len = 0;
    data.receive_buf = buf;
    data.receive_len = len;
    return (hi_i2c_read(HI_I2C_IDX_0, addr, &data) == HI_ERR_SUCCESS) ? 0 : -1;
}

int Env_Init(void)
{
    hi_io_set_func(HI_IO_NAME_GPIO_13, I2C_SDA_FUNC);
    hi_io_set_func(HI_IO_NAME_GPIO_14, I2C_SCL_FUNC);
    if (hi_i2c_init(HI_I2C_IDX_0, 400000) != HI_ERR_SUCCESS) {
        printf("[env] i2c init failed\r\n");
        return -1;
    }

    /* 探测 SHT30：发一条单次测量命令，能写成功即认为存在 */
    hi_u8 cmd[2] = {0x2C, 0x06};
    g_has_th = (i2c_write_bytes(ENV_SHT30_ADDR, cmd, 2) == 0) ? 1 : 0;

    /* 探测 BH1750：上电 + 连续高分辨率模式 */
    hi_u8 on = 0x01;
    g_has_lux = 0;
    if (i2c_write_bytes(ENV_BH1750_ADDR, &on, 1) == 0) {
        hi_u8 mode = 0x10;
        if (i2c_write_bytes(ENV_BH1750_ADDR, &mode, 1) == 0) {
            g_has_lux = 1;
        }
    }

    printf("[env] sht30=%d bh1750=%d\r\n", g_has_th, g_has_lux);
    return (g_has_th || g_has_lux) ? 0 : -1;
}

int Env_HasTempHum(void)
{
    return g_has_th;
}

int Env_HasLux(void)
{
    return g_has_lux;
}

int Env_ReadTempHum(float *temp, float *hum)
{
    if (!g_has_th || temp == NULL || hum == NULL) {
        return -1;
    }

    hi_u8 cmd[2] = {0x2C, 0x06};       /* 高重复性、无时钟拉伸 */
    if (i2c_write_bytes(ENV_SHT30_ADDR, cmd, 2) != 0) {
        return -1;
    }
    hi_udelay(20000);                   /* 最大转换时间 15ms，留余量 */

    hi_u8 buf[6] = {0};
    if (i2c_read_bytes(ENV_SHT30_ADDR, buf, 6) != 0) {
        return -1;
    }

    unsigned int raw_t = ((unsigned int)buf[0] << 8) | buf[1];
    unsigned int raw_h = ((unsigned int)buf[3] << 8) | buf[4];

    *temp = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    *hum = 100.0f * (float)raw_h / 65535.0f;

    /* 明显越界的读数视为异常，不写出去 */
    if (*temp < -40.0f || *temp > 125.0f || *hum < 0.0f || *hum > 100.0f) {
        return -1;
    }
    return 0;
}

int Env_ReadLux(float *lux)
{
    if (!g_has_lux || lux == NULL) {
        return -1;
    }
    hi_u8 buf[2] = {0};
    if (i2c_read_bytes(ENV_BH1750_ADDR, buf, 2) != 0) {
        return -1;
    }
    unsigned int raw = ((unsigned int)buf[0] << 8) | buf[1];
    *lux = (float)raw / 1.2f;
    return 0;
}
