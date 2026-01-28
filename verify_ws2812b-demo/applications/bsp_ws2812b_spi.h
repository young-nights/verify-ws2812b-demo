/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-08     18452       the first version
 */
#ifndef APPLICATIONS_WS2812B_SPI_H_
#define APPLICATIONS_WS2812B_SPI_H_


#include "bsp_sys.h"


/* SPI引脚 -- NSS */
#define     WS2812B_NSS_PORT     SPI1_NSS_GPIO_Port
#define     WS2812B_NSS_PIN      SPI1_NSS_Pin


#define     WS2812B_NSS_SET(bit) if(bit) \
                                 HAL_GPIO_WritePin ( WS2812B_NSS_PORT, WS2812B_NSS_PIN , GPIO_PIN_SET );\
                                 else \
                                 HAL_GPIO_WritePin ( WS2812B_NSS_PORT, WS2812B_NSS_PIN , GPIO_PIN_RESET );



extern struct rt_spi_device *ws2812b_spi_dev;

// 函数声明 ------------------------------------------------------------
int WS2812B_SPI_Init(void);

#endif /* APPLICATIONS_WS2812B_SPI_H_ */
