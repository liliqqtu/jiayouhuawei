/*
 * 河南省信息技术应用创新大赛（开源鸿蒙） - 赛题四
 * 独居老人居家安全监测与应急辅助系统
 * 主任务调度与状态机核心控制引擎
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cmsis_os2.h"
#include "ohos_init.h"

#include "elderly_care_config.h"
#include "alarm_beeper.h"
#include "sensor_infrared.h"
#include "sensor_door.h"
#include "emergency_sos.h"
#include "sensor_environment.h"
#include "network_service.h"

#define TASK_STACK_SIZE     (1024 * 8)
#define TASK_PRIORITY       osPriorityNormal

static elderly_care_data_t g_care_data;

// 获取系统全局状态快照
void ElderlyCareGetData(elderly_care_data_t *out_data)
{
    if (out_data != NULL) {
        memcpy(out_data, &g_care_data, sizeof(elderly_care_data_t));
    }
}

// 核心主循环线程
static void ElderlyCareTask(void *arg)
{
    (void)arg;
    uint32_t ms_accumulator = 0;

    printf("\n======================================================\n");
    printf("   OpenHarmony Smart Elderly Care System Started!     \n");
    printf("   河南信创大赛·赛题四: 独居老人居家安全与应急辅助系统   \n");
    printf("======================================================\n\n");

    // 1. 初始化所有硬件模块
    AlarmBeeperInit();
    SensorInfraredInit();
    SensorDoorInit();
    EmergencySosInit();
    SensorEnvironmentInit();
    NetworkServiceInit();

    memset(&g_care_data, 0, sizeof(g_care_data));
    g_care_data.current_level = ALERT_LEVEL_NORMAL;
    g_care_data.current_alert = ALERT_TYPE_NONE;

    // 2. 状态机调度主循环
    while (1) {
        // 50ms 高频任务: 按键扫描与声光模式状态机
        EmergencySosPoll();
        AlarmBeeperTick(50);
        osDelay(5); // 50ms 延时

        ms_accumulator += 50;
        if (ms_accumulator >= 1000) {
            ms_accumulator = 0;
            g_care_data.uptime_seconds++;

            // 1秒周期任务: 各传感器更新计时驱动
            SensorInfraredTick1s();
            SensorDoorTick1s();
            SensorEnvironmentTick1s();

            // 收集最新传感器数据
            g_care_data.motion_active = SensorInfraredGetRealtime();
            g_care_data.seconds_since_last_motion = SensorInfraredGetSecondsSinceLastMotion();
            g_care_data.motion_events_today = SensorInfraredGetTodayCount();

            g_care_data.door_open = SensorDoorIsOpen();
            g_care_data.door_open_duration_sec = SensorDoorGetOpenDuration();

            g_care_data.sos_active = EmergencySosIsActive();
            SensorEnvironmentRead(&g_care_data.temperature, &g_care_data.humidity);

            // ================= 综合决策与告警仲裁引擎 =================
            alert_level_t new_level = ALERT_LEVEL_NORMAL;
            alert_type_t  new_alert = ALERT_TYPE_NONE;

            // 优先级 1: 老人主动按下紧急求助键 (最高危机等级)
            if (g_care_data.sos_active) {
                new_level = ALERT_LEVEL_CRITICAL;
                new_alert = ALERT_TYPE_SOS;
            }
            // 优先级 2: 室内严重高温异常 (火灾/过热隐患)
            else if (g_care_data.temperature >= DEFAULT_TEMP_HIGH_ALERT) {
                new_level = ALERT_LEVEL_CRITICAL;
                new_alert = ALERT_TYPE_TEMP_HIGH;
            }
            // 优先级 3: 长时间无活动判定 (疑似老人突发疾病跌倒或昏迷)
            else if (g_care_data.seconds_since_last_motion >= DEFAULT_NO_MOTION_TIMEOUT_SEC) {
                new_level = ALERT_LEVEL_CRITICAL;
                new_alert = ALERT_TYPE_NO_MOTION;
            }
            // 优先级 4: 门窗超时长开未关
            else if (g_care_data.door_open_duration_sec >= DEFAULT_DOOR_OPEN_TIMEOUT_SEC) {
                new_level = ALERT_LEVEL_WARNING;
                new_alert = ALERT_TYPE_DOOR_TIMEOUT;
            }
            // 优先级 5: 室内温度过低 (采暖故障/极寒受冻风险)
            else if (g_care_data.temperature <= DEFAULT_TEMP_LOW_ALERT) {
                new_level = ALERT_LEVEL_WARNING;
                new_alert = ALERT_TYPE_TEMP_LOW;
            }
            // 正常状态
            else {
                new_level = ALERT_LEVEL_NORMAL;
                new_alert = ALERT_TYPE_NONE;
            }

            g_care_data.current_level = new_level;
            g_care_data.current_alert = new_alert;
            AlarmBeeperSetMode(new_level, new_alert);

            // ================= 格式化遥测输出 (标准 JSON 流) =================
            char telemetry_buf[256];
            snprintf(telemetry_buf, sizeof(telemetry_buf),
                     "{\"type\":\"telemetry\",\"deviceId\":\"bearpi-001\",\"uptime\":%u,\"motion\":%u,\"no_motion_sec\":%u,"
                     "\"door\":%u,\"door_sec\":%u,\"temp\":%.1f,\"humi\":%.1f,\"sos\":%u,"
                     "\"alert\":%u,\"level\":%u,\"count\":%u}",
                     g_care_data.uptime_seconds,
                     g_care_data.motion_active,
                     g_care_data.seconds_since_last_motion,
                     g_care_data.door_open,
                     g_care_data.door_open_duration_sec,
                     g_care_data.temperature,
                     g_care_data.humidity,
                     g_care_data.sos_active,
                     (uint32_t)g_care_data.current_alert,
                     (uint32_t)g_care_data.current_level,
                     g_care_data.motion_events_today);

            // 通道 1: 串口输出（USB 有线调试模式）
            printf("%s\n", telemetry_buf);

            // 通道 2: 无线 Wi-Fi UDP 遥测 + 离线断网环形缓存（独立供电脱机模式）
            NetworkServiceSendTelemetry(telemetry_buf);

            if (new_level == ALERT_LEVEL_CRITICAL) {
                printf("[ALERT_BROADCAST] CRITICAL ALARM! Type=%u (1:SOS, 2:NoMotion, 4:TempHigh)\n", new_alert);
            }
        }
    }
}

// 应用程序入口
static void ElderlyCareAppEntry(void)
{
    osThreadAttr_t attr;
    attr.name = "ElderlyCareTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = TASK_STACK_SIZE;
    attr.priority = TASK_PRIORITY;

    if (osThreadNew(ElderlyCareTask, NULL, &attr) == NULL) {
        printf("[ERROR] Failed to create ElderlyCareTask thread!\n");
    } else {
        printf("[SUCCESS] ElderlyCareTask thread created.\n");
    }
}

SYS_RUN(ElderlyCareAppEntry);
