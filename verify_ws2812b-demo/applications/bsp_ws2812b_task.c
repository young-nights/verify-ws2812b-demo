/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-01-28     18452       the first version
 */
#include "bsp_sys.h"



void WS2812B_Thread_entry(void* parameter)
{



    for(;;)
    {


        rt_thread_mdelay(500);
    }

}



/**
  * @brief  This is a Initialization for nRF24L01
  * @retval int
  */
rt_thread_t WS2812B_Task_Handle = RT_NULL;
int WS2812B_Thread_Init(void)
{
    WS2812B_Task_Handle = rt_thread_create("WS2812B_Thread_entry", WS2812B_Thread_entry, RT_NULL, 4096, 9, 100);
    /* 检查是否创建成功,成功就启动线程 */
    if(WS2812B_Task_Handle != RT_NULL)
    {
        LOG_I("LOG:%d. WS2812B_Thread_entry is Succeed.",Record.ulog_cnt++);
        rt_thread_startup(WS2812B_Task_Handle);
    }
    else {
        LOG_E("LOG:%d. WS2812B_Thread_entry is Failed",Record.ulog_cnt++);
    }

    return RT_EOK;
}
INIT_APP_EXPORT(WS2812B_Thread_Init);





