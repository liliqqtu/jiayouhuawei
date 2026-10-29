/**
 * @file    drv.h
 * @brief   驱动层：PIR、按键、LED 的最薄封装。
 *
 * 设计取舍：全部用“轮询 + 软件去抖”，不用 GPIO 中断。
 * 原因：中断回调里不能打印日志、不能发网络包，学生调试时很难定位；
 *      1 秒轮询对 PIR 这种秒级信号完全够用，代码也更好读。
 */

#ifndef __DRV_H__
#define __DRV_H__

#include "hi_types_base.h"

/** 初始化三个 GPIO（上电调用一次） */
void Drv_Init(void);

/** 读 PIR 原始电平：1 表示检测到人体活动 */
hi_u8 Drv_PirTriggered(void);

/** 读按键：返回 1 表示“本次轮询刚刚按下”（边沿触发，不会重复） */
hi_u8 Drv_KeyPressedEdge(void);

/** 按键原始电平：1 = 按下（由上层做长按/短按判定） */
hi_u8 Drv_KeyDown(void);

/** 门磁：1 开 / 0 关 / -1 未接（未接时上层必须写 null） */
hi_s8 Drv_DoorOpen(void);

/** 点灯 / 灭灯 */
void Drv_LedSet(hi_u8 on);

/**
 * 蜂鸣器开关（可选外设，CFG_BUZZER_EN=0 时是空实现）。
 * 没接蜂鸣器的板子上调用它不会报错、也不会有声音，只是不假装有。
 */
void Drv_BuzzerSet(hi_u8 on);

#endif /* __DRV_H__ */
