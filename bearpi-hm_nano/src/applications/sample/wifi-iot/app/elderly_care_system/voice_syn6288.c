/*
 * 河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
 * 独居老人居家安全监测与应急辅助系统
 * SYN6288 智能语音合成播报模块驱动及多模态语音设置实现
 */

#include "voice_syn6288.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_uart.h"
#include "iot_errno.h"

#define MAX_VOICE_FRAME_SIZE    256
#define MAX_TEXT_PAYLOAD_SIZE   200

// 全局音量状态，初始为默认值 (0 ~ 16)
static uint8_t      g_voice_volume = DEFAULT_VOICE_VOLUME;
static bool         g_voice_initialized = false;
static osMutexId_t  g_voice_mutex = NULL;

/* 
 * 预置专属告警语音 GBK 协议帧（免转码、零失真、100% 官方协议兼容）
 * 结构：0xFD | 长度高字节 | 长度低字节(=内容长+3) | 0x01(合成命令) | 0x01(GBK格式) | 内容... | 异或校验和
 */

// BOOT: [sound101]系统启动，守夜人守护已就绪
static const uint8_t g_voice_frame_boot[] = {
    0xFD, 0x00, 0x27, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x31, 0x5D, 0xCF, 0xB5, 0xCD, 0xB3, 0xC6, 0xF4, 0xB6, 0xAF, 0xA3,
    0xAC, 0xCA, 0xD8, 0xD2, 0xB9, 0xC8, 0xCB, 0xCA, 0xD8, 0xBB, 0xA4, 0xD2,
    0xD1, 0xBE, 0xCD, 0xD0, 0xF7, 0x8F
};

// SOS: [sound105][v16][s4]紧急求助触发，已通知家属与社区网格员
static const uint8_t g_voice_frame_sos[] = {
    0xFD, 0x00, 0x3A, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x35, 0x5D, 0x5B, 0x76, 0x31, 0x36, 0x5D, 0x5B, 0x73, 0x34, 0x5D,
    0xBD, 0xF4, 0xBC, 0xB1, 0xC7, 0xF3, 0xD6, 0xFA, 0xB4, 0xA5, 0xB7, 0xA2,
    0xA3, 0xAC, 0xD2, 0xD1, 0xCD, 0xA8, 0xD6, 0xAA, 0xBC, 0xD2, 0xCA, 0xF4,
    0xD3, 0xEB, 0xC9, 0xE7, 0xC7, 0xF8, 0xCD, 0xF8, 0xB8, 0xF1, 0xD4, 0xB1,
    0x8D
};

// SOS_REPEAT: [sound102][v16][s4]紧急呼救中，已通知家属与网格员
static const uint8_t g_voice_frame_sos_repeat[] = {
    0xFD, 0x00, 0x34, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x32, 0x5D, 0x5B, 0x76, 0x31, 0x36, 0x5D, 0x5B, 0x73, 0x34, 0x5D,
    0xBD, 0xF4, 0xBC, 0xB1, 0xBA, 0xF4, 0xBE, 0xC8, 0xD6, 0xD0, 0xA3, 0xAC,
    0xD2, 0xD1, 0xCD, 0xA8, 0xD6, 0xAA, 0xBC, 0xD2, 0xCA, 0xF4, 0xD3, 0xEB,
    0xCD, 0xF8, 0xB8, 0xF1, 0xD4, 0xB1, 0xB7
};

// NO_MOTION: [sound102][v15][s3]长时间未检测到活动，请确认老人安全
static const uint8_t g_voice_frame_no_motion[] = {
    0xFD, 0x00, 0x38, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x32, 0x5D, 0x5B, 0x76, 0x31, 0x35, 0x5D, 0x5B, 0x73, 0x33, 0x5D,
    0xB3, 0xA4, 0xCA, 0xB1, 0xBC, 0xE4, 0xCE, 0xB4, 0xBC, 0xEC, 0xB2, 0xE2,
    0xB5, 0xBD, 0xBB, 0xEE, 0xB6, 0xAF, 0xA3, 0xAC, 0xC7, 0xEB, 0xC8, 0xB7,
    0xC8, 0xCF, 0xC0, 0xCF, 0xC8, 0xCB, 0xB0, 0xB2, 0xC8, 0xAB, 0x9D
};

// DOOR_TIMEOUT: [sound101][v13][s4]门窗长开未关，请及时确认
static const uint8_t g_voice_frame_door_timeout[] = {
    0xFD, 0x00, 0x2E, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x31, 0x5D, 0x5B, 0x76, 0x31, 0x33, 0x5D, 0x5B, 0x73, 0x34, 0x5D,
    0xC3, 0xC5, 0xB4, 0xB0, 0xB3, 0xA4, 0xBF, 0xAA, 0xCE, 0xB4, 0xB9, 0xD8,
    0xA3, 0xAC, 0xC7, 0xEB, 0xBC, 0xB0, 0xCA, 0xB1, 0xC8, 0xB7, 0xC8, 0xCF,
    0x82
};

// TEMP_HIGH: [sound103][v15][s4]室内温度过高，请注意防火
static const uint8_t g_voice_frame_temp_high[] = {
    0xFD, 0x00, 0x2E, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x33, 0x5D, 0x5B, 0x76, 0x31, 0x35, 0x5D, 0x5B, 0x73, 0x34, 0x5D,
    0xCA, 0xD2, 0xC4, 0xDA, 0xCE, 0xC2, 0xB6, 0xC8, 0xB9, 0xFD, 0xB8, 0xDF,
    0xA3, 0xAC, 0xC7, 0xEB, 0xD7, 0xA2, 0xD2, 0xE2, 0xB7, 0xC0, 0xBB, 0xF0,
    0xBC
};

// TEMP_LOW: [sound101][v12][s3]室内温度过低，请注意保暖
static const uint8_t g_voice_frame_temp_low[] = {
    0xFD, 0x00, 0x2E, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x31, 0x5D, 0x5B, 0x76, 0x31, 0x32, 0x5D, 0x5B, 0x73, 0x33, 0x5D,
    0xCA, 0xD2, 0xC4, 0xDA, 0xCE, 0xC2, 0xB6, 0xC8, 0xB9, 0xFD, 0xB5, 0xCD,
    0xA3, 0xAC, 0xC7, 0xEB, 0xD7, 0xA2, 0xD2, 0xE2, 0xB1, 0xA3, 0xC5, 0xAF,
    0xE5
};

// SMOKE: [sound105][v16][s4]检测到浓烟或燃气泄漏，请立即开窗并撤离
static const uint8_t g_voice_frame_smoke[] = {
    0xFD, 0x00, 0x3C, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x35, 0x5D, 0x5B, 0x76, 0x31, 0x36, 0x5D, 0x5B, 0x73, 0x34, 0x5D,
    0xBC, 0xEC, 0xB2, 0xE2, 0xB5, 0xBD, 0xC5, 0xA8, 0xD1, 0xCC, 0xBB, 0xF2,
    0xC8, 0xBC, 0xC6, 0xF8, 0xD0, 0xB9, 0xC2, 0xA9, 0xA3, 0xAC, 0xC7, 0xEB,
    0xC1, 0xA2, 0xBC, 0xB4, 0xBF, 0xAA, 0xB4, 0xB0, 0xB2, 0xA2, 0xB3, 0xB7,
    0xC0, 0xEB, 0xB9
};

// CLEAR: [sound101][v12][s4]告警解除，系统恢复正常守护
static const uint8_t g_voice_frame_clear[] = {
    0xFD, 0x00, 0x30, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x31, 0x5D, 0x5B, 0x76, 0x31, 0x32, 0x5D, 0x5B, 0x73, 0x34, 0x5D,
    0xB8, 0xE6, 0xBE, 0xAF, 0xBD, 0xE2, 0xB3, 0xFD, 0xA3, 0xAC, 0xCF, 0xB5,
    0xCD, 0xB3, 0xBB, 0xD6, 0xB8, 0xB4, 0xD5, 0xFD, 0xB3, 0xA3, 0xCA, 0xD8,
    0xBB, 0xA4, 0xAB
};

// VOL_TEST: [sound101]音量已调节
static const uint8_t g_voice_frame_vol_test[] = {
    0xFD, 0x00, 0x17, 0x01, 0x01, 0x5B, 0x73, 0x6F, 0x75, 0x6E, 0x64, 0x31,
    0x30, 0x31, 0x5D, 0xD2, 0xF4, 0xC1, 0xBF, 0xD2, 0xD1, 0xB5, 0xF7, 0xBD,
    0xDA, 0xC1
};

typedef struct {
    const uint8_t *frame_data;
    uint32_t       frame_len;
} preset_voice_entry_t;

static const preset_voice_entry_t g_preset_voice_table[VOICE_ALERT_MAX] = {
    [VOICE_ALERT_BOOT]         = { g_voice_frame_boot,         sizeof(g_voice_frame_boot) },
    [VOICE_ALERT_SOS]          = { g_voice_frame_sos,          sizeof(g_voice_frame_sos) },
    [VOICE_ALERT_SOS_REPEAT]   = { g_voice_frame_sos_repeat,   sizeof(g_voice_frame_sos_repeat) },
    [VOICE_ALERT_NO_MOTION]    = { g_voice_frame_no_motion,    sizeof(g_voice_frame_no_motion) },
    [VOICE_ALERT_DOOR_TIMEOUT] = { g_voice_frame_door_timeout, sizeof(g_voice_frame_door_timeout) },
    [VOICE_ALERT_TEMP_HIGH]    = { g_voice_frame_temp_high,    sizeof(g_voice_frame_temp_high) },
    [VOICE_ALERT_TEMP_LOW]     = { g_voice_frame_temp_low,     sizeof(g_voice_frame_temp_low) },
    [VOICE_ALERT_SMOKE]        = { g_voice_frame_smoke,        sizeof(g_voice_frame_smoke) },
    [VOICE_ALERT_CLEAR]        = { g_voice_frame_clear,        sizeof(g_voice_frame_clear) },
    [VOICE_ALERT_VOL_TEST]     = { g_voice_frame_vol_test,     sizeof(g_voice_frame_vol_test) },
};

int VoiceSyn6288Init(void)
{
    if (g_voice_mutex == NULL) {
        g_voice_mutex = osMutexNew(NULL);
    }

    // 1. 配置引脚复用:
    // GPIO_6 复用为 UART1_TXD (连接 SYN6288 RXD)
    IoTGpioInit(PIN_VOICE_TXD);
    IoTGpioSetFunc(PIN_VOICE_TXD, IOT_GPIO_FUNC_GPIO_6_UART1_TXD);

    // GPIO_5 复用为 UART1_RXD (连接 SYN6288 TXD 或 BUSY)
    IoTGpioInit(PIN_VOICE_RXD);
    IoTGpioSetFunc(PIN_VOICE_RXD, IOT_GPIO_FUNC_GPIO_5_UART1_RXD);
    IoTGpioSetPull(PIN_VOICE_RXD, IOT_GPIO_PULL_UP);

    // 2. 初始化 UART1 (波特率 9600, 8位数据, 1位停止, 无奇偶校验)
    IotUartAttribute uart_attr = {
        .baudRate = 9600,
        .dataBits = IOT_UART_DATA_BIT_8,
        .stopBits = IOT_UART_STOP_BIT_1,
        .parity = IOT_UART_PARITY_NONE,
        .rxBlock = IOT_UART_BLOCK_STATE_NONE_BLOCK,
        .txBlock = IOT_UART_BLOCK_STATE_NONE_BLOCK,
        .pad = 0
    };

    unsigned int ret = IoTUartInit(UART_VOICE_PORT, &uart_attr);
    if (ret != IOT_SUCCESS) {
        printf("[VOICE] Error: IoTUartInit(port=%d) failed, code=%u\n", UART_VOICE_PORT, ret);
        return -1;
    }

    g_voice_volume = DEFAULT_VOICE_VOLUME;
    g_voice_initialized = true;

    printf("[VOICE] SYN6288 TTS initialized (UART1, 9600 8N1, default_vol=%d)\n", g_voice_volume);
    return 0;
}

void VoiceSetVolume(uint8_t volume)
{
    uint8_t target_vol = volume;

    if (target_vol > 16) {
        if (target_vol <= 100) {
            target_vol = (uint8_t)((target_vol * 16 + 50) / 100);
        } else {
            target_vol = 16;
        }
    }

    if (g_voice_mutex != NULL) {
        osMutexAcquire(g_voice_mutex, osWaitForever);
    }
    g_voice_volume = target_vol;
    if (g_voice_mutex != NULL) {
        osMutexRelease(g_voice_mutex);
    }

    printf("[VOICE] Hardware volume updated to: %u (range 0~16)\n", (unsigned int)g_voice_volume);
}

uint8_t VoiceGetVolume(void)
{
    return g_voice_volume;
}

int VoicePlayPresetAlert(voice_alert_id_t alert_id)
{
    if (!g_voice_initialized || alert_id >= VOICE_ALERT_MAX) {
        return -1;
    }

    const preset_voice_entry_t *entry = &g_preset_voice_table[alert_id];
    if (entry->frame_data == NULL || entry->frame_len == 0) {
        return -1;
    }

    if (g_voice_mutex != NULL) {
        osMutexAcquire(g_voice_mutex, osWaitForever);
    }

    int written = IoTUartWrite(UART_VOICE_PORT, entry->frame_data, entry->frame_len);

    if (g_voice_mutex != NULL) {
        osMutexRelease(g_voice_mutex);
    }

    printf("[VOICE] Preset Alert %d triggered, written=%d/%u bytes\n",
           alert_id, written, entry->frame_len);
    return (written == (int)entry->frame_len) ? 0 : -1;
}

/* 
 * 将 UTF-8 字符串转换为 UTF-16BE (Unicode) 大端字节序
 * SYN6288 支持 0x03 (Unicode 格式)，编码范围 0x4E00~0x9FFF
 */
static int Utf8ToUtf16Be(const char *utf8, uint8_t *utf16_out, int max_out_bytes)
{
    const uint8_t *p = (const uint8_t *)utf8;
    int out_len = 0;

    while (*p && (out_len + 2 <= max_out_bytes)) {
        uint32_t cp = 0;
        if ((*p & 0x80) == 0) {
            // 1-byte ASCII (0x00 ~ 0x7F)
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            // 2-byte UTF-8
            cp = (*p++ & 0x1F) << 6;
            if ((*p & 0xC0) != 0x80) break;
            cp |= (*p++ & 0x3F);
        } else if ((*p & 0xF0) == 0xE0) {
            // 3-byte UTF-8 (常见中文)
            cp = (*p++ & 0x0F) << 12;
            if ((*p & 0xC0) != 0x80) break;
            cp |= (*p++ & 0x3F) << 6;
            if ((*p & 0xC0) != 0x80) break;
            cp |= (*p++ & 0x3F);
        } else if ((*p & 0xF8) == 0xF0) {
            // 4-byte UTF-8
            cp = (*p++ & 0x07) << 18;
            if ((*p & 0xC0) != 0x80) break;
            cp |= (*p++ & 0x3F) << 12;
            if ((*p & 0xC0) != 0x80) break;
            cp |= (*p++ & 0x3F) << 6;
            if ((*p & 0xC0) != 0x80) break;
            cp |= (*p++ & 0x3F);
        } else {
            p++;
            continue;
        }

        // 大端模式写入
        utf16_out[out_len++] = (uint8_t)((cp >> 8) & 0xFF);
        utf16_out[out_len++] = (uint8_t)(cp & 0xFF);
    }
    return out_len;
}

int VoicePlayText(const char *text)
{
    if (!g_voice_initialized || text == NULL || strlen(text) == 0) {
        return -1;
    }

    if (g_voice_mutex != NULL) {
        osMutexAcquire(g_voice_mutex, osWaitForever);
    }

    // 格式化文本
    char formatted_payload[MAX_TEXT_PAYLOAD_SIZE];
    if (text[0] == '[' && text[1] == 'v') {
        strncpy(formatted_payload, text, sizeof(formatted_payload) - 1);
        formatted_payload[sizeof(formatted_payload) - 1] = '\0';
    } else {
        snprintf(formatted_payload, sizeof(formatted_payload),
                 "[v%u]%s", (unsigned int)g_voice_volume, text);
    }

    uint8_t unicode_buf[MAX_VOICE_FRAME_SIZE];
    int unicode_len = Utf8ToUtf16Be(formatted_payload, unicode_buf, sizeof(unicode_buf));
    if (unicode_len <= 0) {
        if (g_voice_mutex != NULL) {
            osMutexRelease(g_voice_mutex);
        }
        return -1;
    }

    // 官方协议规范：数据区长度 = 1字节命令 + 1字节参数 + 内容长度 + 1字节校验和 = N + 3
    int data_len = unicode_len + 3;

    uint8_t frame[MAX_VOICE_FRAME_SIZE];
    frame[0] = 0xFD;                         // 帧头
    frame[1] = (uint8_t)((data_len >> 8) & 0xFF); // 长度高字节
    frame[2] = (uint8_t)(data_len & 0xFF);        // 长度低字节
    frame[3] = 0x01;                         // 合成播放命令
    frame[4] = 0x03;                         // 文本格式: 0x03 (Unicode UTF-16BE)

    memcpy(&frame[5], unicode_buf, unicode_len);

    int total_frame_len = 5 + unicode_len + 1;

    // 计算从 0xFD 到内容末尾的逐字节异或校验码
    uint8_t ecc = 0;
    for (int i = 0; i < total_frame_len - 1; i++) {
        ecc ^= frame[i];
    }
    frame[total_frame_len - 1] = ecc;

    int written = IoTUartWrite(UART_VOICE_PORT, frame, total_frame_len);

    if (g_voice_mutex != NULL) {
        osMutexRelease(g_voice_mutex);
    }

    printf("[VOICE] Dynamic Text sent: written=%d/%d bytes\n", written, total_frame_len);
    return (written == total_frame_len) ? 0 : -1;
}

int VoicePlayVolumeTest(uint8_t volume)
{
    VoiceSetVolume(volume);
    return VoicePlayPresetAlert(VOICE_ALERT_VOL_TEST);
}

void VoiceStop(void)
{
    if (!g_voice_initialized) {
        return;
    }

    const uint8_t stop_frame[] = { 0xFD, 0x00, 0x02, 0x02, 0x00, 0xFD };
    IoTUartWrite(UART_VOICE_PORT, stop_frame, sizeof(stop_frame));
    printf("[VOICE] Playback stopped.\n");
}

bool VoiceIsBusy(void)
{
    IotGpioValue val = IOT_GPIO_VALUE1;
    if (IoTGpioGetInputVal(PIN_VOICE_RXD, &val) == IOT_SUCCESS) {
        return (val == IOT_GPIO_VALUE0);
    }
    return false;
}
