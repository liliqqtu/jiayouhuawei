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

#include <hi_isr.h>
#include <hi_early_debug.h>
#include <hi_types_base.h>
#include <hi_errno.h>
#include <hi3861.h>
#include <hi_task.h>
#include <hi_clock.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_watchdog.h"

#define SHIFT_BYTE              (8)
#define TIMER_INTERVAL          (3000)
#define STACK_SIZE              (1024)
#define TIMER_BASE_ADDRESS      (0x40050000)
#define TIMER_OFFSET            (0x14)
#define TIMER_ID                (2)
#define TIMER_FREQ_24M          (24 * 1000000)
#define TIMER_FREQ_40M          (40 * 1000000)

#define TIMER_REGISTER          (0x40010030)

// 定时器寄存器
// Timer register
#define TEST_TIMER_CONTROLREG   (0x008)
#define TEST_TIMER_LOADCOUNT    (0x000)
#define TEST_TIMER_EOI          (0x00C)
#define TEST_TIMER_INTSTATUS    (0x010)

unsigned int g_timer_cnt_cb = 0;
unsigned int tmp = 0;
unsigned int uvIntSave;

// 定时器计数，到达后触发中断函数
// Timer count, trigger interrupt function upon arrival
void timer2_isr_trigger(unsigned char id, unsigned int period)
{
    unsigned int temp;
    unsigned short reg_val;
    reg_val = 0;
    hi_reg_read16(TIMER_REGISTER, reg_val);     // 读取定时器2寄存器
                                                // Read Timer 2 Register
    reg_val |= (1 << SHIFT_BYTE);               // 寄存器赋值
                                                // register assignment
    hi_reg_write16(TIMER_REGISTER, reg_val);    // 写入寄存器
                                                // Write register
    hi_reg_write32(TIMER_BASE_ADDRESS + id * TIMER_OFFSET + TEST_TIMER_CONTROLREG, 0);  // 取消使能
    hi_reg_write32(TIMER_BASE_ADDRESS + id * TIMER_OFFSET + TEST_TIMER_LOADCOUNT, period);   // 设置间隔初始值
    
    /*
     * 时钟模式: 32bit [bit 1 设置为 1]
     * 时钟滴答 1/1 时钟频率 [bit 3 设置为 0, bit 2 设置为 0]
     * 时钟循环 [bit 1 设置为 1]
     * Clock mode: 32bit [bit 1 is set to 1]
     * Clock ticks 1/1 clock frequency [bit 3 is set to 0, bit 2 is set to 0]
     * Clock cycle [bit 1 is set to 1]
    */
    temp = (1U << 0) | (1U << 1);       // 1: 可循环, 0: 使能
                                        // 1: Circulable, 0: enable
    hi_reg_write32(TIMER_BASE_ADDRESS + id * TIMER_OFFSET + TEST_TIMER_CONTROLREG, temp);
    g_timer_cnt_cb = 0;                 // 清除寄存器
                                        // Clear register
    printf("The clock is over\r\n");
}

// 清除中断
// Clear Interrupt
void timer_clear(unsigned char id)
{
    hi_u32 reg_val = 0;
    hi_reg_read32(TIMER_BASE_ADDRESS + id * TIMER_OFFSET + TEST_TIMER_EOI, reg_val);
    if (reg_val != HI_ERR_SUCCESS) {
        printf("clear timer interrupt failed, reg_val=%d\r\n", reg_val);
    }
}

// 时间中断函数
// Time interrupt function
void timer2_irq_handle(unsigned int irqv)
{
    (void)irqv;
    g_timer_cnt_cb++;
    dprintf("\n into the func timer2_irq_handle\n");
    timer_clear(TIMER_ID);  // 清中断
                            // close the interrupt
}

// 获取晶振频率
// get the frequence of XTAL
unsigned int GetXTALClock(void)
{
    unsigned int xtal = 0;
    unsigned int timer_freq = 0;
    xtal = hi_get_xtal_clock(); // 获取外部晶振频率
                                // get the frequence of extern XTAL
    if (xtal == HI_XTAL_CLOCK_24M) {
        timer_freq = TIMER_FREQ_24M;
    } else if (xtal == HI_XTAL_CLOCK_40M) {
        timer_freq = TIMER_FREQ_40M;
    } else if (xtal == HI_XTAL_CLOCK_MAX) {
        timer_freq = TIMER_FREQ_40M;
        printf("Crystal frequency invalid\r\n");
    }
    return timer_freq;
}

// 计时器中断回调函数
// the callback function of timer interruption
void TimerIrqTask(void)
{
    unsigned int irq_idx = TIMER_2_IRQ;
    unsigned int timer_freq;
    timer_freq = GetXTALClock();
    timer2_isr_trigger(TIMER_ID, timer_freq);   // 开启计时器1s后触发中断
                                                // Trigger the interrupt 1 s after starting the timer
    uvIntSave = hi_int_lock();                  // 关闭所有中断
                                                // Turn off all interrupts
    // 注册中断函数
    // Register interrupt function
    unsigned int ret = hi_irq_request(TIMER_2_IRQ, HI_IRQ_FLAG_PRI1, timer2_irq_handle, 0);
    if (ret != HI_ERR_SUCCESS) {
        printf("request example irq fail:%u\n", ret);
        return;
    }
    if (g_timer_cnt_cb != 0) {
        dprintf("\n [hi_int_lock] failed\n");
    }
    dprintf("\n [hi_int_lock] success\n\n -Restore the state before shutting down the interrupt-\n");
    hi_int_restore(uvIntSave);  // 中断前完成相关处理
                                // Complete relevant processing before interruption
    if (g_timer_cnt_cb != 0) {
        dprintf("\n [hi_int_restore] failed, timer cnt cb = %d\n", g_timer_cnt_cb);
    }

    tmp = g_timer_cnt_cb;
    ret = hi_irq_enable(irq_idx);   // 使能中断
                                    // Enable interrupt
    if (ret != HI_ERR_SUCCESS) {
        dprintf("failed to hi_irq_enable func ,ret = %u\r\n", ret);
    }
    hi_sleep(TIMER_INTERVAL);       // 进入3s的中断函数
                                    // Interrupt function entering 3s

    if (g_timer_cnt_cb > tmp) {
        dprintf("[timer2_irq_handle]success, timer cnt cb=%d\r\n", g_timer_cnt_cb);
    } else {
        dprintf("[timer2_irq_handle]failed, timer cnt cb=%d\r\n", g_timer_cnt_cb);
    }
    tmp = g_timer_cnt_cb;
    hi_irq_disable(irq_idx);    // 关闭中断
                                // close the interrupt
    if (g_timer_cnt_cb == tmp) {
        dprintf("[hi_irq_disable]success\r\n");
    } else {
        dprintf("[hi_irq_disable]failed, timer cnt cb=%d\r\n", g_timer_cnt_cb);
    }
    hi_irq_free(irq_idx);   // 释放中断
                            // relase the interrupt
}

void ExampleTimerIrqEntry(void)
{
    IoTWatchDogDisable();
    osThreadAttr_t attr;
    attr.name = "TimerIrqTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    // 创建一个线程，并注册一个回调函数 TimerThread，控制红色LED灯每隔1秒钟闪烁一次
    // Create a thread, register a callback function TimerThread, and control the red LED to flash once every 1 second
    if (osThreadNew((osThreadFunc_t)TimerIrqTask, NULL, &attr) == NULL) {
        printf("[TimerIrqTask] osThreadNew Falied to create TimerIrqTask!\n");
    }
}

APP_FEATURE_INIT(ExampleTimerIrqEntry);