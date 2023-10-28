/*
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef USER_DEFINE_H
#define USER_DEFINE_H

#define FIRM_VER    ("1.00")

/* GPIOアサイン */
#define P_0             (  0 )
#define P_DEBUG         (  1 )
#define P_LCD2_SCK      (  2 )
#define P_LCD2_TX       (  3 )
#define P_LCD2_RX       (  4 )
#define P_LCD2_CS       (  5 )
#define I2C_SDA         (  6 )
#define I2C_SCL         (  7 )
    #define P_LCD_OUT_NUM   (  8 )
    #define P_LCD_OUT_MASK  (  0x0000FF00 )
#define P_LCD_D0        (  8 )
#define P_LCD_D1        (  9 )
#define P_LCD_D2        ( 10 )
#define P_LCD_D3        ( 11 )
#define P_LCD_D4        ( 12 )
#define P_LCD_D5        ( 13 )
#define P_LCD_D6        ( 14 )
#define P_LCD_D7        ( 15 )
#define P_LCD2_CD       ( 16 )

/* pico-extras\work\lcd\CMakeLists.txtで定義*/
    //#define PICO_SD_CLK_PIN ( 17 )
    //#define PICO_SD_CMD_PIN ( 18 )
    //#define PICO_SD_DAT0_PIN ( 19 )

#define P_LCD_SET_NUM   (  4 )
#define P_LCD_CS        ( 23 )
#define P_LCD_RS        ( 24 )
#define P_LCD_WR        ( 25 )
#define P_LCD_RD        ( 26 )

#define P_LCD_RESET     ( 27 )
#define P_LCD2_RESET    ( 28 )
#define P_PWM           ( 29 )

/* ペリフェラル */
#define LCD2_SPI_CH     ( spi0 )
#define I2C_PORT        ( i2c1 )
//#define PWM_PWM_PIO   ( pio0 )
#define PWM_PIO_SM      (    2 )
#define PWM_DMA_CH      (    1 )
//#define PWM_DMA_IRQ   (    1 )
#define LCD_PIO         ( pio0 )
#define LCD_PIO_SM      (    3 )
#define LCD_DMA_CH      (    2 )

//#define SD_PIO   ( pio1 )
//DMA8-11は使用禁止



#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/sd_card.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "audio_pwm.h"
#include "lcd.h"
#include "lcd2.h"
#include "led.h"

#endif
