/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-01-28     18452       the first version
 * 2026-04-14     Improved    添加演示效果和错误处理
 */
#include "bsp_sys.h"
#include "bsp_ws2812b.h"

void WS2812B_Thread_entry(void* parameter)
{
    LOG_I("WS2812B 任务线程启动");
    
    ws2812b_init();
    
    while(1)
    {
//        ws2812b_demo_effects();  // 运行演示效果
        rt_thread_mdelay(500);    // 50ms循环一次
        ws2812b_update();
    }
}

/**
 * @brief  WS2812B 任务线程初始化
 * @retval int
 */
rt_thread_t WS2812B_Task_Handle = RT_NULL;
int WS2812B_Thread_Init(void)
{
    // [FIX] 问题4: 线程栈从4096扩大到8192，防止DMA操作+效果函数溢出
WS2812B_Task_Handle = rt_thread_create("WS2812B_Thread", WS2812B_Thread_entry, RT_NULL, 8192, 9, 100);
    
    if(WS2812B_Task_Handle != RT_NULL)
    {
        LOG_I("WS2812B 任务线程创建成功");
        rt_thread_startup(WS2812B_Task_Handle);
    }
    else 
    {
        LOG_E("WS2812B 任务线程创建失败");
    }

    return RT_EOK;
}
INIT_APP_EXPORT(WS2812B_Thread_Init);
