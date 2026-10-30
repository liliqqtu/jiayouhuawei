/*
 * 主控任务调度与状态判断
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

// 获取当前系统状态
void ElderlyCareGetData(elderly_care_data_t *out_data)
{
    if (out_data != NULL) {
        memcpy(out_data, &g_care_data, sizeof(elderly_care_data_t));
    }
}

// 主循环任务
static void ElderlyCareTask(void *arg)
{
    (void)arg;
    uint32_t ms_accumulator = 0;

    printf("\n[SYS] Elderly care system start\n\n");

    // 初始化外设
    AlarmBeeperInit();
    SensorInfraredInit();
    SensorDoorInit();
    EmergencySosInit();
    SensorEnvironmentInit();
    NetworkServiceInit();

    memset(&g_care_data, 0, sizeof(g_care_data));
    g_care_data.current_level = ALERT_LEVEL_NORMAL;
    g_care_data.current_alert = ALERT_TYPE_NONE;

    while (1) {
        // 50ms按键和声光刷新
        EmergencySosPoll();
        AlarmBeeperTick(50);
        osDelay(5); // 50ms

        ms_accumulator += 50;
        if (ms_accumulator >= 1000) {
            ms_accumulator = 0;
            g_care_data.uptime_seconds++;

            // 1秒读取一次传感器状态
            SensorInfraredTick1s();
            SensorDoorTick1s();
            SensorEnvironmentTick1s();

            g_care_data.motion_active = SensorInfraredGetRealtime();
            g_care_data.seconds_since_last_motion = SensorInfraredGetSecondsSinceLastMotion();
            g_care_data.motion_events_today = SensorInfraredGetTodayCount();

            g_care_data.door_open = SensorDoorIsOpen();
            g_care_data.door_open_duration_sec = SensorDoorGetOpenDuration();

            g_care_data.sos_active = EmergencySosIsActive();
            SensorEnvironmentRead(&g_care_data.temperature, &g_care_data.humidity);

            // 报警状态判定
            alert_level_t new_level = ALERT_LEVEL_NORMAL;
            alert_type_t  new_alert = ALERT_TYPE_NONE;

            // 1. SOS报警
            if (g_care_data.sos_active) {
                new_level = ALERT_LEVEL_CRITICAL;
                new_alert = ALERT_TYPE_SOS;
            }
            // 2. 高温报警
            else if (g_care_data.temperature >= DEFAULT_TEMP_HIGH_ALERT) {
                new_level = ALERT_LEVEL_CRITICAL;
                new_alert = ALERT_TYPE_TEMP_HIGH;
            }
            // 3. 超时无活动(疑似摔倒或异常)
            else if (g_care_data.seconds_since_last_motion >= DEFAULT_NO_MOTION_TIMEOUT_SEC) {
                new_level = ALERT_LEVEL_CRITICAL;
                new_alert = ALERT_TYPE_NO_MOTION;
            }
            // 4. 门开超时
            else if (g_care_data.door_open_duration_sec >= DEFAULT_DOOR_OPEN_TIMEOUT_SEC) {
                new_level = ALERT_LEVEL_WARNING;
                new_alert = ALERT_TYPE_DOOR_TIMEOUT;
            }
            // 5. 低温
            else if (g_care_data.temperature <= DEFAULT_TEMP_LOW_ALERT) {
                new_level = ALERT_LEVEL_WARNING;
                new_alert = ALERT_TYPE_TEMP_LOW;
            }
            else {
                new_level = ALERT_LEVEL_NORMAL;
                new_alert = ALERT_TYPE_NONE;
            }

            g_care_data.current_level = new_level;
            g_care_data.current_alert = new_alert;
            AlarmBeeperSetMode(new_level, new_alert);

            // 打包成json字符串
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

            // 串口打印与网络发送
            printf("%s\n", telemetry_buf);
            NetworkServiceSendTelemetry(telemetry_buf);

            if (new_level == ALERT_LEVEL_CRITICAL) {
                printf("[ALERT] Level=CRITICAL, Type=%u\n", new_alert);
            }
        }
    }
}

// 入口函数
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
        printf("[ERROR] Create ElderlyCareTask failed!\n");
    } else {
        printf("[SUCCESS] ElderlyCareTask created.\n");
    }
}

SYS_RUN(ElderlyCareAppEntry);
