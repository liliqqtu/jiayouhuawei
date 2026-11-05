/*
 * 河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
 * 独居老人居家安全监测与应急辅助系统
 * SYN6288 智能语音合成播报模块驱动及多模态语音设置头文件
 */

#ifndef VOICE_SYN6288_H
#define VOICE_SYN6288_H

#include <stdint.h>
#include <stdbool.h>
#include "elderly_care_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统专属预置告警语音类型
 *        底层全部采用经过严格校验的预置 GBK 帧，避开任何编译器字符集编码差异，确保 100% 稳定出声
 */
typedef enum {
    VOICE_ALERT_BOOT = 0,       // 开机问候: "[sound101]系统启动，守夜人守护已就绪"
    VOICE_ALERT_SOS,            // 紧急求助: "[sound105][v16][s4]紧急求助触发，已通知家属与社区网格员"
    VOICE_ALERT_SOS_REPEAT,     // 紧急呼救复播: "[sound102][v16][s4]紧急呼救中，已通知家属与网格员"
    VOICE_ALERT_NO_MOTION,      // 长时间未活动: "[sound102][v15][s3]长时间未检测到活动，请确认老人安全"
    VOICE_ALERT_DOOR_TIMEOUT,   // 门窗长开未关: "[sound101][v13][s4]门窗长开未关，请及时确认"
    VOICE_ALERT_TEMP_HIGH,      // 高温火险预警: "[sound103][v15][s4]室内温度过高，请注意防火"
    VOICE_ALERT_TEMP_LOW,       // 低温受冻预警: "[sound101][v12][s3]室内温度过低，请注意保暖"
    VOICE_ALERT_SMOKE,          // 浓烟与燃气:   "[sound105][v16][s4]检测到浓烟或燃气泄漏，请立即开窗并撤离"
    VOICE_ALERT_CLEAR,          // 告警解除恢复: "[sound101][v12][s4]告警解除，系统恢复正常守护"
    VOICE_ALERT_VOL_TEST,       // 音量调节反馈: "[sound101]音量已调节"
    VOICE_ALERT_MAX
} voice_alert_id_t;

/**
 * @brief 初始化 SYN6288 语音模块
 *        配置 GPIO_6 为 UART1_TXD, GPIO_5 为 UART1_RXD
 *        初始化串口波特率 9600 8N1，并将音量初始化为 DEFAULT_VOICE_VOLUME
 * @return 0 成功，负数失败
 */
int VoiceSyn6288Init(void);

/**
 * @brief 设置 SYN6288 语音播报音量
 * @param volume 音量值 (0 ~ 16, 0为静音, 16为最大音量)
 */
void VoiceSetVolume(uint8_t volume);

/**
 * @brief 获取当前设置的语音音量
 * @return 当前音量 (0 ~ 16)
 */
uint8_t VoiceGetVolume(void);

/**
 * @brief 播报专属预置报警语音（推荐在主状态机中调用）
 *        采用预编译 GBK 协议帧，含专属警报声、自适应音量与平缓语速
 * @param alert_id 告警类型编号
 * @return 0 成功，负数失败
 */
int VoicePlayPresetAlert(voice_alert_id_t alert_id);

/**
 * @brief 动态通用文本播报
 *        内部自动将 UTF-8 编码转换为 SYN6288 原生 UTF-16BE (Unicode) 协议帧，并严格校准帧长与 ECC 校验
 * @param text 待播报的任意中文/英文文本
 * @return 0 成功，负数失败
 */
int VoicePlayText(const char *text);

/**
 * @brief 调节音量并立即播报一段测试提示音
 * @param volume 新音量 (0 ~ 16)
 * @return 0 成功，负数失败
 */
int VoicePlayVolumeTest(uint8_t volume);

/**
 * @brief 停止当前正在播放的语音
 */
void VoiceStop(void);

/**
 * @brief 检查语音模块是否正处于忙碌播放状态
 * @return true 正在播放中，false 空闲
 */
bool VoiceIsBusy(void);

#ifdef __cplusplus
}
#endif

#endif // VOICE_SYN6288_H
