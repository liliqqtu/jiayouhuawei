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

/* 我们通过两个PID控制环路实现平衡车功能
 * 内环路是PD直立环, 输入为指定的直立角度, 反馈为当前车身的直立角度, 输出为电机的前后转动.
 * 外环路是PI速度环, 输入为目标速度, 反馈为当前车的移动速度, 用编码器固定时间内的增量代替, 输出为直立环角度的调整量.
 * 更多平衡车自动控制的理论知识, 可以搜索相关技术文章和视频进行学习
 *
 * We realize the balance car function through two PID control loops
 * The inner loop is a PD vertical loop. The input is the specified vertical angle, the
 * feedback is the current body vertical angle, and the output is the forward and backward
 * rotation of the motor
 * The outer loop is a PI speed loop. The input is the target speed, and the feedback is
 * the moving speed of the current vehicle. It is replaced by the increment within the
 * fixed time of the encoder. The output is the adjustment amount of the vertical loop
 * angle.
 * For more theoretical knowledge on automatic control of balance car, you can search
 * relevant technical articles and videos for learning
 */

#include "ctrl_algo.h"

// pid param
#define LIM_ERR_SUM             (99)
#define LIM_ERR                 (5)
#define USE_PI                  (0)
// #define LIMIT_ABS(val, lim)     val = ((val) > (lim)) ? (lim) : (((val) < -(lim)) ? (-(lim)) : (val))

// [0]:P   [1]:I     [2]:D

#define DEFAULT_VELO_KP         (0.1) // (0.1)
#define DEFAULT_VELO_KI         (0.002) // (0.001)
#define DEFAULT_STAND_KP        (-1)
#define DEFAULT_STAND_KD        (-2)
#define DEFAULT_DIFF_KP         (2)
#define DEFAULT_DIFF_KI         (0.01)

CTRL_PID_STRUCT ctrl_pid_velocity = {0};
CTRL_PID_STRUCT ctrl_pid_stand = {0};

void init_ctrl_algo(void)
{
    /* 直立环 */
    memset(&ctrl_pid_stand, 0, sizeof(CTRL_PID_STRUCT));
    ctrl_pid_stand.type = CTRL_PID_TYPE_MASK_K | CTRL_PID_TYPE_MASK_D;
    ctrl_pid_stand.kp = DEFAULT_STAND_KP;
    ctrl_pid_stand.kd = DEFAULT_STAND_KD;
    ctrl_pid_stand.limit_err = 45; // 限制_错 误45
    ctrl_pid_stand.limit_exec = 99; // 限制执行 99

    /* 速度环 */
    memset(&ctrl_pid_velocity, 0, sizeof(CTRL_PID_STRUCT));
    ctrl_pid_velocity.type = CTRL_PID_TYPE_MASK_K | CTRL_PID_TYPE_MASK_I;
    ctrl_pid_velocity.kp = DEFAULT_VELO_KP;
    ctrl_pid_velocity.ki = DEFAULT_VELO_KI;
    /* 实测100减速比的电机,全速跑, 100Hz间隔测到的velo 为27~28 左右 */
    ctrl_pid_velocity.limit_err = 60; // 限制_错误 60
    ctrl_pid_velocity.limit_sum = 1000; // 极限_总和 1000
    ctrl_pid_velocity.limit_exec = 45; // 限制执行 45
}

float LIMIT_ABS(float val, float lim)
{
    float ret;
    ret = ((val) > (lim)) ? (lim) : (((val) < -(lim)) ? (-(lim)) : (val));
    return ret;
}

float ctrl_pid_algo(float target, float feedback, CTRL_PID_STRUCT *param)
{
    float err;
    float exec;
    float ret;

    err = target - feedback;
    ret = LIMIT_ABS(err, param->limit_err);
    err = ret;
    /* p */
    exec = err * param->kp;

    /* i */
    if (param->type | CTRL_PID_TYPE_MASK_I) {
        param->err_sum += err;
        ret = LIMIT_ABS(param->err_sum, LIM_ERR_SUM);
        param->err_sum = ret;
        exec += param->ki * param->err_sum;
    }

    /* d */
    if (param->type | CTRL_PID_TYPE_MASK_D) {
        exec += param->kd * (err - param->err_last);
        param->err_last = err;
    }
    ret = LIMIT_ABS(exec, param->limit_exec);
    exec = ret;
    return exec;
}

CTRL_PID_STRUCT GetVelocity(void)
{
    return ctrl_pid_velocity;
}

CTRL_PID_STRUCT GetStand(void)
{
    return ctrl_pid_stand;
}