/*
 * 室内微气候与温湿度环境监测子系统
 * 支持物理 SHT30 (E53_IA1 I2C1 飞线) 自适应读取与智能微气候自愈
 */

#include "sensor_environment.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_i2c.h"
#include "iot_i2c_ex.h"

#define SHT30_I2C_IDX       1
#define SHT30_I2C_ADDR      0x44
#define SHT30_I2C_BAUDRATE  400000

#define I2C1_SDA_GPIO       0
#define I2C1_SCL_GPIO       1

static float g_current_temp = 24.5f;
static float g_current_humi = 53.0f;
static uint32_t g_env_step  = 0;
static uint8_t  g_is_injected = 0;
static uint8_t  g_hardware_sht30_ready = 0;

// 精简周期微扰波表 (避免依赖 libm 的 sinf/cosf)
static const int8_t s_wave_table[16] = {
    0, 3, 6, 8, 10, 8, 6, 3, 0, -3, -6, -8, -10, -8, -6, -3
};

// SHT30 CRC8 校验算法
static uint8_t SHT3xCheckCrc(uint8_t data[], uint8_t nbrOfBytes, uint8_t checksum)
{
    uint8_t crc = 0xFF;
    uint8_t bit = 0;
    uint8_t byteCtr;
    const int16_t POLYNOMIAL = 0x131;
    for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr) {
        crc ^= (data[byteCtr]);
        for (bit = 8; bit > 0; --bit) {
            if (crc & 0x80)
                crc = (crc << 1) ^ POLYNOMIAL;
            else
                crc = (crc << 1);
        }
    }
    return (crc == checksum) ? 0 : 1;
}

// 物理 SHT30 硬件初始化
static int SHT30InitHw(void)
{
    // 配置 GPIO_0 为 I2C1_SDA, GPIO_1 为 I2C1_SCL
    IoTGpioInit(I2C1_SDA_GPIO);
    IoTGpioSetFunc(I2C1_SDA_GPIO, 6);
    IoTGpioInit(I2C1_SCL_GPIO);
    IoTGpioSetFunc(I2C1_SCL_GPIO, 6);
    IoTI2cInit(SHT30_I2C_IDX, SHT30_I2C_BAUDRATE);

    // 软复位 SHT30 (0x30, 0xA2)
    uint8_t reset_cmd[2] = { 0x30, 0xA2 };
    int ret = IoTI2cWrite(SHT30_I2C_IDX, (SHT30_I2C_ADDR << 1) | 0x00, reset_cmd, sizeof(reset_cmd));
    if (ret != 0) {
        return -1;
    }
    usleep(20000); // 延时 20ms 等待芯片复位完成

    // 启动周期测量模式 (0x22, 0x36: 中重复度, 2mps)
    uint8_t start_cmd[2] = { 0x22, 0x36 };
    ret = IoTI2cWrite(SHT30_I2C_IDX, (SHT30_I2C_ADDR << 1) | 0x00, start_cmd, sizeof(start_cmd));
    return (ret == 0) ? 0 : -1;
}

// 物理 SHT30 数据读取
static int SHT30ReadHw(float *temp, float *humi)
{
    uint8_t raw[6] = {0};
    uint8_t fetch_cmd[2] = { 0xE0, 0x00 };
    IotI2cData i2c_data = { 0 };
    i2c_data.sendBuf = fetch_cmd;
    i2c_data.sendLen = sizeof(fetch_cmd);
    i2c_data.receiveBuf = raw;
    i2c_data.receiveLen = sizeof(raw);

    int ret = IoTI2cWriteread(SHT30_I2C_IDX, (SHT30_I2C_ADDR << 1) | 0x00, &i2c_data);
    if (ret != 0) {
        return -1;
    }

    // 校验并计算温度: T = -45 + 175 * raw / 65535
    if (SHT3xCheckCrc(raw, 2, raw[2]) == 0) {
        uint16_t raw_t = ((uint16_t)raw[0] << 8) | raw[1];
        raw_t &= ~0x0003;
        *temp = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    } else {
        return -1;
    }

    // 校验并计算相对湿度: RH = 100 * raw / 65535
    if (SHT3xCheckCrc(&raw[3], 2, raw[5]) == 0) {
        uint16_t raw_h = ((uint16_t)raw[3] << 8) | raw[4];
        raw_h &= ~0x0003;
        *humi = 100.0f * (float)raw_h / 65535.0f;
    } else {
        return -1;
    }

    return 0;
}

void SensorEnvironmentInit(void)
{
    g_current_temp = 24.5f;
    g_current_humi = 53.0f;
    g_env_step = 0;
    g_is_injected = 0;
    g_hardware_sht30_ready = 0;

    // 优先尝试探测物理 SHT30 (E53_IA1 I2C 飞线接入)
    if (SHT30InitHw() == 0) {
        g_hardware_sht30_ready = 1;
        printf("[ENV] Physical SHT30 sensor DETECTED on I2C1 (GPIO_0/SDA, GPIO_1/SCL)! Real sensor online.\n");
    } else {
        g_hardware_sht30_ready = 0;
        printf("[ENV] Physical SHT30 not detected on I2C1. Running in adaptive micro-climate fallback mode.\n");
    }
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

    // 1. 如果物理 SHT30 已连接，优先读取真实传感器数据
    if (g_hardware_sht30_ready) {
        float hw_temp = 0.0f;
        float hw_humi = 0.0f;
        if (SHT30ReadHw(&hw_temp, &hw_humi) == 0) {
            g_current_temp = hw_temp;
            g_current_humi = hw_humi;
            return;
        } else {
            // 物理采样单次失败，尝试自动重新初始化芯片
            SHT30InitHw();
        }
    }

    // 2. 兜底自适应微气候模型
    g_env_step++;
    int idx_t = (g_env_step) % 16;
    int idx_h = (g_env_step + 4) % 16;
    float delta_t = (float)s_wave_table[idx_t] * 0.05f; // ±0.5 ℃
    float delta_h = (float)s_wave_table[idx_h] * 0.15f; // ±1.5 %RH
    g_current_temp = 24.5f + delta_t;
    g_current_humi = 53.0f + delta_h;
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
