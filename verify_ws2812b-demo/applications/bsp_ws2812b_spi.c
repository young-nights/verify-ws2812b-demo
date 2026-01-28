/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-08     18452       the first version
 */
#include <bsp_ws2812b_spi.h>
#include <rtdbg.h>


/* 宏定义SPI名称 */
#define     WS2812B_SPI_NAME    "ws2812b_spi1"
/* 宏定义SPI总线 */
#define     WS2812B_SPI_BUS     "spi1"
/* 创建SPI设备句柄 */
struct rt_spi_device *ws2812b_spi_dev = RT_NULL;



int WS2812B_SPI_Init(void)
{
    //--------------------------------------------------------------------------------------
    /* 将SPI设备名挂载到总线 */
    if (rt_hw_spi_device_attach(WS2812B_SPI_BUS, WS2812B_SPI_NAME, WS2812B_NSS_PORT, WS2812B_NSS_PIN) != RT_EOK)
    {
        LOG_E("Failed to attach SPI device %s.", WS2812B_SPI_NAME);
        return -RT_ERROR;
    }
    /* 查找SPI设备 */
    ws2812b_spi_dev = (struct rt_spi_device *)rt_device_find(WS2812B_SPI_NAME);
    if(ws2812b_spi_dev == NULL){
        LOG_E("LOG:%d. ws2812b spi device is not found!",Record.ulog_cnt++);
        return -RT_ERROR;
    }
    else{
        LOG_I("LOG:%d. ws2812b spi device is successfully!",Record.ulog_cnt++);
    }

    /***
     *! 配置SPI结构体参数
     *! data_width: spi传输数据长度为8bits
     *! max_hz: spi的最大工作频率；
     *! mode-> RT_SPI_MASTER: 主机模式；
     *! mode-> RT_SPI_MODE_0: 工作模式0（ CPOL:0 -- 空闲状态为低电平 ， CPHA:0 -- 第一个边沿采集数据 ）
     *! mode-> RT_SPI_MSB: 通讯数据高位在前低位在后
     * */
    struct rt_spi_configuration ws2812b_spi_cfg = { 0 };

    ws2812b_spi_cfg.data_width = 8;
    ws2812b_spi_cfg.max_hz = 6*1000*1000; /* 10M,SPI max 10MHz,lora 4-wire spi */
    ws2812b_spi_cfg.mode = RT_SPI_MASTER | RT_SPI_MODE_3 | RT_SPI_MSB;
    rt_spi_configure(ws2812b_spi_dev, &ws2812b_spi_cfg); /* 使能参数 */

    //--------------------------------------------------------------------------------------

    return RT_EOK;
}









