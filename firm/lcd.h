
/*
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LCD_H
#define LCD_H

#define LCD_HEIGHT (400)
#define LCD_WIDTH  (240)

enum eLCD_STATE
{
    LCD_STATE_POWER_ON,
    LCD_STATE_RESET,
    LCD_STATE_INIT,
    LCD_STATE_DRAW,
    LCD_STATE_MAX
};

extern void lcd_main( void );
extern void lcd_timer_inc( void );
extern uint16_t lcd_get_state( void );
extern bool lcd_is_ready( void );
extern void __time_critical_func(lcd_trans_image)(uint32_t *image_data, uint32_t count);

#endif
