/*
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LCD2_H
#define LCD2_H

#define LCD2_HEIGHT (240)
#define LCD2_WIDTH  (280)

#define LCD2_PIC_HEIGHT         (150)
#define LCD2_PIC_HEIGHT_OFFSET   (45)
#define LCD2_PIC_WIDTH          (270)
#define LCD2_PIC_WIDTH_OFFSET     (5)

enum eLCD2_STATE
{
    LCD2_STATE_POWER_ON,
    LCD2_STATE_NORMAL,
    LCD2_STATE_MAX
};

extern void Lcd2Main( void );
extern void Lcd2TimerInc( void );
extern uint16_t Lcd2_get_state( void );
extern bool lcd2_is_ready( void );

#endif
