/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-08     18452       the first version
 */
#include "bsp_ws2812b_driver.h"
#include "math.h"
#include <string.h>

/* 已经编码好的0/1波形，MOSI可以直接输出的时序位流 */
static uint8_t ws2812b_buffer[WS2812B_LED_NUMS * WS2812B_RGB_BITS] = {0};
/* 原始 24-bit 颜色值（R8G8B8，未编码、未 gamma） */
static uint32_t ws2812b_colors[WS2812B_LED_NUMS] = {0};
/* Gamma 2.8 查找表，下标是 0-255，返回值是 gamma 校正后的 0-255 */
static uint8_t gamma_table[256];
static uint8_t global_brightness = 255;


/***
 * @brief 预先算出 Gamma 2.8 校正曲线的 256 点查表
 * @note  人眼对亮度是非线性的，把PWM占空比从0调到50%，看起来会远不到一半亮，使用Gamma 2.8表对亮度进行校正
 */
static void ws2812b_generate_gamma_table(void)
{
    for (int i = 0; i < 256; i++)
    {
        gamma_table[i] = (uint8_t)(powf((float)i / 255.0f, 2.8f) * 255.0f + 0.5f);
    }
}


/***
 * @brief 把用户传入的颜色值乘以当前的全局亮度值，做线性压缩后再进行Gamma校正，再转变成8-bit数据传到指定指针地址上
 * @note
 */
static void ws2812b_apply_brightness(uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = gamma_table[(*r * global_brightness) >> 8];
    *g = gamma_table[(*g * global_brightness) >> 8];
    *b = gamma_table[(*b * global_brightness) >> 8];
}


/***
 * @brief 把经过 “亮度设置 + Gamma校正” 后的 R、G、B字节按照WS2812B的位顺序要求，展开成24-bit数据位并存入缓冲区中
 */
static void ws2812b_encode_color(uint8_t r, uint8_t g, uint8_t b, uint8_t *buffer)
{
    uint8_t bits[WS2812B_RGB_BITS] = {0};

    for (int i = 0; i < 8; i++)
        bits[7 - i] = (g & (1 << i)) ? WS2812B_CODE_1 : WS2812B_CODE_0;
    for (int i = 0; i < 8; i++)
        bits[15 - i] = (r & (1 << i)) ? WS2812B_CODE_1 : WS2812B_CODE_0;
    for (int i = 0; i < 8; i++)
        bits[23 - i] = (b & (1 << i)) ? WS2812B_CODE_1 : WS2812B_CODE_0;

    memcpy(buffer, bits, WS2812B_RGB_BITS);
}


/***
 * @brief 设置全局亮度值
 * @param brightness: 0~255
 */
void ws2812b_set_brightness(uint8_t brightness)
{
    if (brightness > 255)
        brightness = 255;
    global_brightness = brightness;

    for (uint16_t i = 0; i < WS2812B_LED_NUMS; i++)
    {
        ws2812b_set_color(i, ws2812b_colors[i]);
    }
}


/***
 * @brief 把第index颗灯设置成指定颜色
 * @param index --> 要设置的灯序号
 *        color --> 要设置的颜色
 */
void ws2812b_set_color(uint16_t index, uint32_t color)
{
    if (index >= WS2812B_LED_NUMS)
    {
        LOG_E("LED index %d out of range.", index);
        return;
    }

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Gamma补偿
    ws2812b_apply_brightness(&r, &g, &b);
    // 生成24-bit时序位
    ws2812b_encode_color(r, g, b, &ws2812b_buffer[index * WS2812B_RGB_BITS]);
    // 备份颜色
    ws2812b_colors[index] = color;
}


/***
 * @brief 把当前缓冲区里的波形一次性甩到灯带，点亮 LED
 */
void ws2812b_show(void)
{
    rt_spi_send(ws2812b_spi_dev, ws2812b_buffer, sizeof(ws2812b_buffer));
}


/***
 * @brief 全灯熄灭，并把显存也清零
 */
void ws2812b_clear(void)
{
    memset(ws2812b_buffer, WS2812B_CODE_0, sizeof(ws2812b_buffer));
    memset(ws2812b_colors, 0, sizeof(ws2812b_colors));
    ws2812b_show();
}


/***
 * @brief 把整根灯带设成同一种颜色，但不点亮
 */
void ws2812b_set_all(uint32_t color)
{
    for (uint16_t i = 0; i < WS2812B_LED_NUMS; i++)
    {
        ws2812b_set_color(i, color);
    }
}



/***
 * @brief 初始化 编码波形数组、颜色数组以及Gamma校正列表
 */
void ws2812b_table_init(void)
{
    memset(ws2812b_buffer, WS2812B_CODE_0, sizeof(ws2812b_buffer));
    memset(ws2812b_colors, 0, sizeof(ws2812b_colors));

    ws2812b_generate_gamma_table();
}




// WS2812B的测试函数 --------------------------------------------------------------------------------

/* 颜色表，使用宏定义 */
static uint32_t color_table[] = {
    WS2812B_COLOR_RED,    // 红色
    WS2812B_COLOR_ORANGE, // 橙色
    WS2812B_COLOR_YELLOW, // 黄色
    WS2812B_COLOR_GREEN,  // 绿色
    WS2812B_COLOR_CYAN,   // 青色
    WS2812B_COLOR_BLUE,   // 蓝色
    WS2812B_COLOR_PURPLE, // 紫色
    WS2812B_COLOR_WHITE   // 白色
};




/* 全部灯切换颜色 */
void ws2812b_full_color_test(void)
{
    for (uint8_t i = 0; i < sizeof(color_table) / sizeof(color_table[0]); i++)
    {
        ws2812b_set_all(color_table[i]);
        ws2812b_show();
        rt_thread_mdelay(1000);
    }
}


/* 流水灯效果 */
void ws2812b_waterfall_light_test(void)
{
    ws2812b_clear();
    for (uint16_t i = 0; i < WS2812B_LED_NUMS; i++)
    {
        ws2812b_clear();
        ws2812b_set_color(i, WS2812B_COLOR_BLUE);
        ws2812b_show();
        rt_thread_mdelay(50);
    }
}

/* 亮度渐变测试 */
void ws2812b_brightness_gradient_test(void)
{
    ws2812b_set_all(WS2812B_COLOR_GREEN);
    ws2812b_show();

    for (uint8_t brightness = 255; brightness > 10; brightness -= 5)
    {
        ws2812b_set_brightness(brightness);
        ws2812b_show();
        rt_thread_mdelay(50);
    }

    for (uint8_t brightness = 10; brightness < 255; brightness += 5)
    {
        ws2812b_set_brightness(brightness);
        ws2812b_show();
        rt_thread_mdelay(50);
    }
}

/* 呼吸灯效果 */
void ws2812b_breathing_light_test(void)
{
    for (uint8_t brightness = 0; brightness < 255; brightness += 5)
    {
        ws2812b_set_brightness(brightness);
        ws2812b_show();
        rt_thread_mdelay(20);
    }

    for (uint8_t brightness = 255; brightness > 0; brightness -= 5)
    {
        ws2812b_set_brightness(brightness);
        ws2812b_show();
        rt_thread_mdelay(20);
    }
}



/* demo启动函数 */
static void ws2812b_demo_start(void)
{
    /* 确保 ws2812b_init 只执行一次 */
    static rt_bool_t initialized = RT_FALSE;

    if (!initialized)
    {
        WS2812B_SPI_Init();
        ws2812b_table_init();
        initialized = RT_TRUE;
    }

    /* 亮度渐变 */
    ws2812b_brightness_gradient_test();

    /* 全部灯切换颜色 */
    ws2812b_full_color_test();

    /* 流水灯效果 */
    ws2812b_waterfall_light_test();

    /* 呼吸灯效果 */
    ws2812b_breathing_light_test();
}

/* msh命令：启动WS2812B Demo */
int cmd1(void)
{
//    ws2812b_demo_start();
    WS2812B_SPI_Init();
    ws2812b_table_init();
    ws2812b_clear();
    ws2812b_show();
    ws2812b_set_brightness(255);
    rt_thread_mdelay(1000);
    ws2812b_set_all(WS2812B_COLOR_ORANGE);
//    ws2812b_set_color(7,WS2812B_COLOR_ORANGE);
    ws2812b_show();

    return RT_EOK;
}
MSH_CMD_EXPORT(cmd1, WS2812B_demo);




