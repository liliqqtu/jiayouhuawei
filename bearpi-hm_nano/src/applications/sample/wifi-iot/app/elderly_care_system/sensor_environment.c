/*
 * 温湿度传感器采集驱动 (SHT30 I2C)
 */

#include "sensor_environment.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_i2c.h"
#include "iot_i2c_ex.h"

// I2C 引脚与总线定义 (BearPi-HM Nano: GPIO_0 为 SDA, GPIO_1 为 SCL)
#define I2C1_SDA_GPIO       0
#define I2C1_SCL_GPIO       1
#define SHT30_I2C_IDX       1

// I2C 速率 100kHz
#define SHT30_I2C_BAUDRATE  100000

#define SHT30_ADDR_PRIMARY   0x44
#define SHT30_ADDR_SECONDARY 0x45

static float g_current_temp = 24.5f;
static float g_current_humi = 53.0f;
static uint32_t g_env_step  = 0;
static uint8_t  g_is_injected = 0;

static uint8_t  g_sht30_online = 0;
static uint16_t g_active_addr = SHT30_ADDR_PRIMARY;
static uint32_t g_read_fail_count = 0;

// CRC-8 校验函数 (多项式 0x31: x^8 + x^5 + x^4 + 1)
static uint8_t SHT3xCheckCrc(const uint8_t data[], uint8_t nbrOfBytes, uint8_t checksum)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < nbrOfBytes; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return (crc == checksum) ? 0 : 1;
}

// 初始化 I2C 总线 GPIO
static void I2cBusInit(void)
{
    IoTGpioInit(I2C1_SDA_GPIO);
    IoTGpioSetFunc(I2C1_SDA_GPIO, 6); // GPIO_0 -> I2C1_SDA
    IoTGpioSetPull(I2C1_SDA_GPIO, IOT_GPIO_PULL_UP);

    IoTGpioInit(I2C1_SCL_GPIO);
    IoTGpioSetFunc(I2C1_SCL_GPIO, 6); // GPIO_1 -> I2C1_SCL
    IoTGpioSetPull(I2C1_SCL_GPIO, IOT_GPIO_PULL_UP);

    IoTI2cInit(SHT30_I2C_IDX, SHT30_I2C_BAUDRATE);
}

// 探测指定 I2C 地址的 SHT30
static int SHT30Probe(uint16_t addr)
{
    uint8_t cmd[2] = { 0x24, 0x00 };
    uint32_t ret = IoTI2cWrite(SHT30_I2C_IDX, (addr << 1) | 0x00, cmd, sizeof(cmd));
    return (ret == 0) ? 0 : -1;
}

// SHT30 单次测量
static int SHT30ReadSingleShot(uint16_t addr, float *temp, float *humi)
{
    // 发送触发测量命令
    uint8_t cmd[2] = { 0x24, 0x00 };
    uint32_t ret = IoTI2cWrite(SHT30_I2C_IDX, (addr << 1) | 0x00, cmd, sizeof(cmd));
    if (ret != 0) {
        return -1;
    }

    usleep(20000); // 延时 20ms 等待转换完成

    // 读取 6 字节数据: [T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC]
    uint8_t raw[6] = {0};
    ret = IoTI2cRead(SHT30_I2C_IDX, (addr << 1) | 0x01, raw, sizeof(raw));
    if (ret != 0) {
        return -2;
    }

    // CRC 校验
    if (SHT3xCheckCrc(&raw[0], 2, raw[2]) != 0) {
        return -3;
    }
    if (SHT3xCheckCrc(&raw[3], 2, raw[5]) != 0) {
        return -4;
    }

    // 换算温度与相对湿度
    uint16_t raw_t = ((uint16_t)raw[0] << 8) | raw[1];
    raw_t &= ~0x0003;
    *temp = -45.0f + 175.0f * (float)raw_t / 65535.0f;

    uint16_t raw_h = ((uint16_t)raw[3] << 8) | raw[4];
    raw_h &= ~0x0003;
    *humi = 100.0f * (float)raw_h / 65535.0f;

    if (*temp < -40.0f) *temp = -40.0f;
    if (*temp > 125.0f) *temp = 125.0f;
    if (*humi < 0.0f)   *humi = 0.0f;
    if (*humi > 100.0f) *humi = 100.0f;

    return 0;
}

// I2C 总线自检与芯片探针
static void ProbeSensors(void)
{
    printf("[I2C_SCAN] Probing I2C1 bus for E53_IA1 SHT30 (GPIO_0=SDA, GPIO_1=SCL)...\n");

    // 优先尝试 0x44 (SHT30 默认地址)
    if (SHT30Probe(SHT30_ADDR_PRIMARY) == 0) {
        g_active_addr = SHT30_ADDR_PRIMARY;
        g_sht30_online = 1;
        printf("[I2C_SCAN] >>> SUCCESS: Physical SHT30 found at address 0x44! <<<\n");
        return;
    }

    // 备用尝试 0x45 (SHT30 备选地址)
    if (SHT30Probe(SHT30_ADDR_SECONDARY) == 0) {
        g_active_addr = SHT30_ADDR_SECONDARY;
        g_sht30_online = 1;
        printf("[I2C_SCAN] >>> SUCCESS: Physical SHT30 found at address 0x45! <<<\n");
        return;
    }

    g_sht30_online = 0;
    printf("[I2C_SCAN] [WARN] SHT30 not responding at 0x44 or 0x45!\n");
    printf("[I2C_SCAN] [TIP] Please check jumper wires: 3V3->3.3V, GND->GND, SDA->GPIO_0, SCL->GPIO_1.\n");
}

void SensorEnvironmentInit(void)
{
    g_current_temp = 24.5f;
    g_current_humi = 53.0f;
    g_env_step = 0;
    g_is_injected = 0;
    g_sht30_online = 0;
    g_read_fail_count = 0;

    I2cBusInit();
    ProbeSensors();
}

void SensorEnvironmentRead(float *temperature, float *humidity)
{
    if (temperature != NULL) {
        *temperature = g_current_temp;
    }
    if (humidity != NULL) {
        *humidity = g_current_humi;
    }
}

void SensorEnvironmentTick1s(void)
{
    if (g_is_injected) {
        return;
    }

    // 硬件在线则读取
    if (g_sht30_online) {
        float hw_t = 0.0f;
        float hw_h = 0.0f;
        int ret = SHT30ReadSingleShot(g_active_addr, &hw_t, &hw_h);
        if (ret == 0) {
            g_current_temp = hw_t;
            g_current_humi = hw_h;
            g_read_fail_count = 0;
            printf("[SHT30] Temp: %.1f C, Humi: %.1f %%\n", hw_t, hw_h);
            return;
        } else {
            g_read_fail_count++;
            printf("[SHT30] Read error=%d, fail=%u\n", ret, g_read_fail_count);
            if (g_read_fail_count >= 3) {
                g_sht30_online = 0;
                I2cBusInit();
            }
        }
    } else {
        // 离线每3秒重试探测
        g_env_step++;
        if (g_env_step % 3 == 0) {
            ProbeSensors();
        }
    }

    if (g_env_step % 5 == 0) {
        printf("[WARN] SHT30 offline\n");
    }
}

void SensorEnvironmentInjectAlert(float temp, float humi)
{
    g_is_injected = 1;
    g_current_temp = temp;
    g_current_humi = humi;
    printf("[ENV] Injected environment alert: Temp=%.1f C, Humi=%.1f %%\n", temp, humi);
}

void SensorEnvironmentResetNormal(void)
{
    g_is_injected = 0;
    g_current_temp = 24.5f;
    g_current_humi = 53.0f;
    printf("[ENV] Reset environment monitor to natural state.\n");
}
