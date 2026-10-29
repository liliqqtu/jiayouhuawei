/**
 * @file    config.h
 * @brief   全局配置：只改这一个文件就能换网络、换引脚、换阈值。
 *
 * 注意：以下三项必须在烧录前按现场情况修改，否则设备连不上、也看不到数据。
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* ---------------- 网络 ---------------- */
/* 电脑（网关）的局域网 IP：网关启动时会打印 http://192.168.x.x:8780 */
#define CFG_GATEWAY_IP      "192.168.43.100"
#define CFG_GATEWAY_PORT    8780

/* 与电脑同一个 Wi-Fi 的账号密码（只支持 2.4G，Hi3861 不支持 5G） */
#define CFG_WIFI_SSID       "your-wifi-ssid"
#define CFG_WIFI_PASSWORD   "your-wifi-password"

/* ---------------- 设备 ---------------- */
#define CFG_DEVICE_ID       "bearpi-001"
#define CFG_FW_VERSION      "v0.1.0"

/* ---------------- 引脚（以原理图/官方例程为准，不符只改这里） ---------------- */
/* E53-IS1 人体红外：通过 E53 扩展座接入，官方例程使用 GPIO11 */
#define CFG_PIR_GPIO        HI_IO_NAME_GPIO_11
/* 板载用户按键：作为“紧急求助”演示输入（模拟输入，界面上会标注） */
#define CFG_KEY_GPIO        HI_IO_NAME_GPIO_12
/* 板载 LED：本地光告警 */
#define CFG_LED_GPIO        HI_IO_NAME_GPIO_2

/* ---------------- 业务参数（演示值，不是医疗标准） ---------------- */
/* 连续无活动多久生成“疑似长时间无活动”告警（毫秒）。
 * 演示建议 2~5 分钟；正式使用应显著拉长，并在 APP 端可配置。 */
#define CFG_NO_MOTION_MS    (3 * 60 * 1000)

/* 采样周期：PIR 状态轮询间隔（毫秒） */
#define CFG_SAMPLE_MS       1000

/* 心跳上报周期（毫秒）。1 秒一帧太费流量，5 秒足够。 */
#define CFG_UPLOAD_MS       5000

/* 按键去抖（毫秒） */
#define CFG_KEY_DEBOUNCE_MS 50

/* ---------------- 断网缓存 ---------------- */
/* 断网时最多缓存多少条快照，恢复后按顺序补传 */
#define CFG_CACHE_DEPTH     16

/* ---------------- 门磁（可选） ----------------
 * 0 = 未接：doorOpen 恒为 null（不伪造）
 * 1 = 接了干簧管：读 CFG_DOOR_GPIO
 * 2 = 没接硬件：长按板载键 2 秒切换“门磁开/关”，该帧 source 标 test
 */
#define CFG_DOOR_MODE       0
#define CFG_DOOR_GPIO       HI_IO_NAME_GPIO_10
#define CFG_DOOR_FUNC       HI_IO_FUNC_GPIO_10_GPIO

/* 传感器探测重试周期（毫秒）：模块没插时不要每秒刷错误日志 */
#define CFG_PROBE_MS        (60 * 1000)

/* ---------------- 板端本地阈值判定（赛题四·功能1） ----------------
 * 赛题要求“开发板…通过基础阈值算法识别异常状态”，所以判定不能只放在网关：
 * 板端自己也要判一遍，断网时本地声光照样生效。
 *
 * 下面这些数值与 gateway/config.py 的 Thresholds 是同一套口径，
 * 改一边必须同步改另一边 —— scripts/check_thresholds.py 会做一致性校验，
 * 不一致时直接报错，避免“板和网关各判各的”。
 */
#define CFG_T_WARN          28.0f   /* 偏热提醒（L1） */
#define CFG_T_HIGH          35.0f   /* 温度过高（L2） */
#define CFG_T_FIRE          55.0f   /* 疑似火情·绝对高温（L3） */
#define CFG_T_RISE           8.0f   /* 疑似火情·短时升温幅度（℃，L3） */
#define CFG_T_RISE_MS     (180 * 1000)  /* 升温统计窗口 */
#define CFG_T_LOW           16.0f   /* 温度过低（L2） */

#define CFG_H_HIGH          80.0f   /* 湿度过高（L2） */
#define CFG_H_WARN_HIGH     70.0f   /* 湿度偏高（L1） */
#define CFG_H_LOW           25.0f   /* 湿度过低（L2） */
#define CFG_H_WARN_LOW      35.0f   /* 湿度偏低（L1） */

/* 门磁持续开启多久提醒（毫秒，L1） */
#define CFG_DOOR_OPEN_MS  (10 * 60 * 1000)

/* 环境量采样周期（毫秒）：I2C 总线由 SensorTask 独占，不要在别的任务里读 */
#define CFG_ENV_SAMPLE_MS   2000

/* 防抖：越限持续多久才认定；恢复正常多久才解除。
 * 传感器有噪声，没有这两个时间窗就会“响一下停一下”，现场很难看清。 */
#define CFG_ENV_CONFIRM_MS  (30 * 1000)
#define CFG_ENV_CLEAR_MS    (60 * 1000)

/* ---------------- 下行确认（家属处理结果回传板端） ----------------
 * 板端每隔 CFG_ACK_POLL_MS 问一次网关“我这条告警还有人没处理吗”：
 *   还有人待确认 -> LED 按等级闪烁（L1 慢 / L2 中 / L3 快）
 *   已被确认     -> LED 常亮（表示“已经有人接手”，不再闪、不再响）
 *   没有告警     -> 灭
 * 单色 LED 分不出“绿”，所以用“闪烁 / 常亮 / 灭”三态表达，不假装能亮绿灯。
 */
#define CFG_ACK_POLL_MS     (10 * 1000)

/* ---------------- 蜂鸣器（可选） ----------------
 * 0 = 没接：只做光告警（大多数 E53-IS1 套件不带蜂鸣器，默认就是 0）
 * 1 = 接了：按下面引脚输出高低电平
 * 板端不假装有声音：没接就只闪灯，声音由网关侧模拟或外接语音模块负责。
 */
#define CFG_BUZZER_EN       0
#define CFG_BUZZER_GPIO     HI_IO_NAME_GPIO_9
#define CFG_BUZZER_FUNC     HI_IO_FUNC_GPIO_9_GPIO
#define CFG_BUZZER_ACTIVE_LOW 0

#endif /* __CONFIG_H__ */
