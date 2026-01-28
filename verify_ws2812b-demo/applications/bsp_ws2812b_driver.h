/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-08     18452       the first version
 */
#ifndef APPLICATIONS_BSP_WS2812B_DRIVER_H_
#define APPLICATIONS_BSP_WS2812B_DRIVER_H_

#include "bsp_sys.h"


#ifndef WS2812B_LED_NUMS
#define WS2812B_LED_NUMS 77
#endif

#define WS2812B_RGB_BITS 24 // 每颗灯 24 位数据（GRB 各 8 位）
#define WS2812B_CODE_0 0xC0 // 用 SPI 发 0b11000000，模拟 0 码（高 220 ns）
#define WS2812B_CODE_1 0xF0 // 用 SPI 发 0b11110000，模拟 1 码（高 580 ns）


#define WS2812B_COLOR_BLACK     0x000000    // 熄灭
#define WS2812B_COLOR_RED       0xFF0000    // 正红
#define WS2812B_COLOR_ORANGE    0xF08784    // 暖橙
#define WS2812B_COLOR_YELLOW    0xFF7F27    // 金黄
#define WS2812B_COLOR_GREEN     0x7FFF00    // 翠绿
#define WS2812B_COLOR_CYAN      0x00FFFF    // 青色
#define WS2812B_COLOR_BLUE      0x0000FF    // 正蓝
#define WS2812B_COLOR_PURPLE    0x8B00FF    // 紫色
#define WS2812B_COLOR_WHITE     0xFFFFFF    // 正白


// 函数声明 -------------------------------------------------------
void ws2812b_table_init(void);
void ws2812b_set_brightness(uint8_t brightness);
void ws2812b_set_color(uint16_t index, uint32_t color);
void ws2812b_show(void);
void ws2812b_clear(void);
void ws2812b_set_all(uint32_t color);


// 测试函数------------------------------------------------------
void ws2812b_full_color_test(void);
void ws2812b_waterfall_light_test(void);
void ws2812b_brightness_gradient_test(void);
void ws2812b_breathing_light_test(void);


#endif /* APPLICATIONS_BSP_WS2812B_DRIVER_H_ */
