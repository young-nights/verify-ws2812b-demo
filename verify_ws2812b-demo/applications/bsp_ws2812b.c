/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-01-27     Administrator       the first version
 */
#include "bsp_ws2812b.h"


// 外部句柄（从CubeMX生成）
extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_tim3_ch3;

// 内部宏定义
#define BYTES_PER_LED   3           // RGB=3, RGBW=4
#define BITS_PER_LED    24          // 8 bits per color
#define DMA_BUFF_LEN    (2 * LEDS_PER_DMA_IRQ * BITS_PER_LED)  // 双缓冲总长度
#define DMA_HALF_LEN    (DMA_BUFF_LEN / 2)                     // 半长
#define BITS_PER_IRQ    (LEDS_PER_DMA_IRQ * BITS_PER_LED)      // 每个中断处理的位数

// 数据缓冲区：uint16_t (HAL PWM DMA用 HalfWord)
// [FIX] 问题7: 添加aligned(4)确保DMA对齐
__attribute__((aligned(4))) uint16_t ws2812_buffer[DMA_BUFF_LEN] = {0};

// 控制变量
static volatile uint8_t is_updating = 0;    // 传输中标志
static volatile uint16_t led_index = 0;     // [FIX3-2] 当前已处理的LED周期计数
rt_sem_t dma_complete_sem = RT_NULL; // [FIX2] 改为全局可见，供 ws2812b_demo_effects() 使用

// 应用颜色缓冲区 (RGB, 用户修改此数组)
static uint8_t leds_color_data[BYTES_PER_LED * LED_COUNT] = {0};

// [FIX] 问题1: 删除HAL_TIM_PWM_PulseFinishedCallback，避免与DMA1_Channel2_IRQHandler竞争
// DMA完成逻辑统一在update_sequence()的完成分支中处理

// 前向声明
static void fill_led_pwm_data(uint16_t ledx, uint16_t *ptr);

// 初始化
void ws2812b_init(void)
{
    LOG_I("WS2812B 初始化开始");

    // [FIX] 问题3: 显式使能TIM3和DMA1时钟
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // RT-Thread信号量
    dma_complete_sem = rt_sem_create("ws_sem", 0, RT_IPC_FLAG_FIFO);
    RT_ASSERT(dma_complete_sem != RT_NULL);

    // NVIC中断启用
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    
    LOG_I("WS2812B 初始化完成");
}

// 设置单个LED颜色 (GRB顺序)
void ws2812b_set_color(uint16_t index, uint8_t g, uint8_t r, uint8_t b)
{
    if (index >= LED_COUNT) {
        LOG_W("LED索引超出范围: %d (最大: %d)", index, LED_COUNT - 1);
        return;
    }

    leds_color_data[index * BYTES_PER_LED + 0] = g;
    leds_color_data[index * BYTES_PER_LED + 1] = r;
    leds_color_data[index * BYTES_PER_LED + 2] = b;
}

// 设置所有LED同一颜色
void ws2812b_set_all(uint8_t g, uint8_t r, uint8_t b)
{
    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
        ws2812b_set_color(i, g, r, b);
    }
}

// 启动更新 (非阻塞)
rt_err_t ws2812b_update(void)
{
    if (is_updating) {
        LOG_W("WS2812B 正在更新中，跳过本次");
        return -RT_EBUSY;
    }

    is_updating = 1;
    led_index = 0;

    HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_3);  // 确保干净启动

    memset(ws2812_buffer, 0, sizeof(ws2812_buffer));

    if (HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint16_t *)ws2812_buffer, DMA_BUFF_LEN) != HAL_OK)
    {
        LOG_E("DMA启动失败");
        is_updating = 0;
        return -RT_ERROR;
    }

    LOG_D("WS2812B 更新启动成功");
    return RT_EOK;
}

// [FIX3-2] 更新序列 (重构：用led_index替代led_cycles_cnt，语义更清晰)
void update_sequence(uint8_t is_tc)
{
    if (!is_updating) return;

    uint16_t *buf_ptr = is_tc ? &ws2812_buffer[DMA_HALF_LEN] : ws2812_buffer;

    // 填充下一半缓冲区
    for (uint16_t i = 0; i < LEDS_PER_DMA_IRQ; i++)
    {
        if (led_index < RESET_PRE_MIN)
        {
            // 前复位：全0
            memset(&buf_ptr[i * BITS_PER_LED], 0, BITS_PER_LED * sizeof(uint16_t));
        }
        else if (led_index < RESET_PRE_MIN + LED_COUNT)
        {
            // 数据区
            uint16_t led_idx = led_index - RESET_PRE_MIN;
            fill_led_pwm_data(led_idx, &buf_ptr[i * BITS_PER_LED]);
        }
        else
        {
            // 后复位：全0
            memset(&buf_ptr[i * BITS_PER_LED], 0, BITS_PER_LED * sizeof(uint16_t));
        }
        led_index++;
    }

    // 全部发送完成 + 足够复位后停止
    if (led_index >= RESET_PRE_MIN + LED_COUNT + RESET_POST_MIN + 20)  // 多加20个周期确保复位
    {
        HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_3);
        // HAL_TIM_Base_Stop(&htim3);   // 可选

        is_updating = 0;
        led_index = 0;
        rt_sem_release(dma_complete_sem);
        LOG_D("WS2812B 更新完成");
    }
}

// [FIX3-6] HT/TC 中断处理（移除HAL_DMA_IRQHandler避免它清除HT/TC标志）
void DMA1_Channel2_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(&hdma_tim3_ch3, DMA_FLAG_HT2))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_tim3_ch3, DMA_FLAG_HT2);
        update_sequence(0);  // HT
    }
    if (__HAL_DMA_GET_FLAG(&hdma_tim3_ch3, DMA_FLAG_TC2))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_tim3_ch3, DMA_FLAG_TC2);
        update_sequence(1);  // TC
    }
}

// 填充单个LED PWM数据 (GRB, 参考文件适配)
static void fill_led_pwm_data(uint16_t ledx, uint16_t *ptr)
{
    if (ledx >= LED_COUNT) return;

    uint8_t g = leds_color_data[ledx * BYTES_PER_LED + 0];
    uint8_t r = leds_color_data[ledx * BYTES_PER_LED + 1];
    uint8_t b = leds_color_data[ledx * BYTES_PER_LED + 2];

    // GRB顺序，MSB先
    for (uint8_t i = 0; i < 8; i++)
    {
        ptr[i] = (g & (1 << (7 - i))) ? PWM_HIGH_1 : PWM_HIGH_0;
        ptr[8 + i] = (r & (1 << (7 - i))) ? PWM_HIGH_1 : PWM_HIGH_0;
        ptr[16 + i] = (b & (1 << (7 - i))) ? PWM_HIGH_1 : PWM_HIGH_0;
    }
}

// 演示效果函数
void ws2812b_demo_effects(void)
{
    static uint8_t demo_step = 0;
    static uint32_t last_time = 0;
    uint32_t current_time = rt_tick_get();
    
    // 每500ms切换一次效果
    if (current_time - last_time >= 500)
    {
        last_time = current_time;
        
        switch (demo_step)
        {
            case 0: // 红色全亮
                ws2812b_set_all(255, 0, 0);
                ws2812b_update();
                if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                break;
            case 1: // 绿色全亮
                ws2812b_set_all(0, 255, 0);
                ws2812b_update();
                if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                break;
            case 2: // 蓝色全亮
                ws2812b_set_all(0, 0, 255);
                ws2812b_update();
                if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                break;
            case 3: // 白色全亮
                ws2812b_set_all(255, 255, 255);
                ws2812b_update();
                if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                break;
            case 4: // 流水灯效果
                for (int i = 0; i < LED_COUNT; i++)
                {
                    ws2812b_set_color(i, 255, 255, 0);  // 黄色
                    ws2812b_update();
                    if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                    rt_thread_mdelay(50);
                    ws2812b_set_color(i, 0, 0, 0);      // 关闭
                    ws2812b_update();
                    if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                }
                break;
            case 5: // 呼吸灯效果
                for (int brightness = 0; brightness < 255; brightness += 5)
                {
                    ws2812b_set_all(brightness, brightness, brightness);
                    ws2812b_update();
                    if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                    rt_thread_mdelay(20);
                }
                for (int brightness = 255; brightness > 0; brightness -= 5)
                {
                    ws2812b_set_all(brightness, brightness, brightness);
                    ws2812b_update();
                    if (rt_sem_take(dma_complete_sem, 100) != RT_EOK) LOG_W("WS2812B DMA 超时，跳过本次更新"); // [FIX3-5] 超时保护
                    rt_thread_mdelay(20);
                }
                break;
            default:
                demo_step = 0;
                break;
        }
        
        demo_step = (demo_step + 1) % 6;
    }
}










