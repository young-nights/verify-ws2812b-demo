/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-01-27     Administrator       the first version
 */
#ifndef APPLICATIONS_MACSYS_BSP_WS2812B_H_
#define APPLICATIONS_MACSYS_BSP_WS2812B_H_


#include "bsp_sys.h"

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "stm32f1xx_hal.h"

#define LED_COUNT       30          // LED数量，根据需要修改
#define PWM_PERIOD      89          // TIM周期值 (ARR = 89, 72MHz / 90 ≈ 800kHz)
#define PWM_HIGH_0      32          // '0'高电平ticks ≈0.4μs (32/256 * 1.25μs)
#define PWM_HIGH_1      64          // '1'高电平ticks ≈0.8μs (64/256 * 1.25μs)
#define RESET_PRE_MIN   10          // 复位前最小LED周期 (参考文件: >280us ≈10 cycles)
#define RESET_POST_MIN  8           // 复位后最小LED周期
#define LEDS_PER_DMA_IRQ 4          // 每个DMA中断处理的LED数 (参考文件: 4, 平衡中断频率)

// 缓冲区：双缓冲 (HT/TC)，每个部分 LEDS_PER_DMA_IRQ * 24 个 uint16_t
extern uint16_t ws2812_buffer[2 * LEDS_PER_DMA_IRQ * 24];

// 函数声明
void ws2812b_init(void);
void ws2812b_set_color(uint16_t index, uint8_t g, uint8_t r, uint8_t b);
void ws2812b_set_all(uint8_t g, uint8_t r, uint8_t b);
rt_err_t ws2812b_update(void);          // 非阻塞更新，返回 -RT_EBUSY 如果正在传输
void update_sequence(uint8_t is_tc);    // HT/TC 更新逻辑
void ws2812b_demo_effects(void);       // 演示效果函数



#endif /* APPLICATIONS_MACSYS_BSP_WS2812B_H_ */
