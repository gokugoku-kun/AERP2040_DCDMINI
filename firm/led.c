/*
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "user_define.h"

enum MCP23017_BANK0_ADR
{
    ADR_IODIRA,
    ADR_IODIRB,
    ADR_IPOLA,
    ADR_IPOLB,
    ADR_GPINTENA,
    ADR_GPINTENB,
    ADR_DEFVALA,
    ADR_DEFVALB,
    ADR_INTCONA,
    ADR_INTCONB,
    ADR_IOCON,
    ADR_IOCON_,
    ADR_GPPUA,
    ADR_GPPUB,
    ADR_INTFA,
    ADR_INTFB,
    ADR_INTCAPA,
    ADR_INTCAPB,
    ADR_GPIOA,
    ADR_GPIOB,
    ADR_OLATA,
    ADR_OLATB,
    ADR_MAX
};

#define MCP23017_SADR   ( 0x20 ) //0x2X

#define INIT_CMD_MAX ( 3 )
const uint8_t MCP23017_INIT_CMD[ INIT_CMD_MAX ][ 2 ] =
{
    { ADR_IOCON , 0x00 }, // default
    { ADR_IODIRA, 0x00 }, // all out
    { ADR_GPIOA , 0x00 }, // all low
};

static uint8_t led_bit_pattern_A = 0x00;
static uint16_t led_time_1ms = 0;

typedef struct LED_BLINK
{
    uint16_t time;
    uint8_t bit_pattern_A;
}LED_BLINK_T;

#define LED_BLINK_TABLE_MAX ( 19 )
const static LED_BLINK_T LED_BLINK_TABLE[ LED_BLINK_TABLE_MAX ]=
{
   { 800, 0b00001001 },// R
   { 200, 0b00000000 },
   { 800, 0b00010010 },//  Y
   { 200, 0b00000000 },
   { 800, 0b00100100 },//    G
   { 200, 0b00000000 },
   { 800, 0b00011011 },// RY
   { 200, 0b00000000 },
   { 800, 0b00110110 },//  YG
   { 200, 0b00000000 },
   { 800, 0b00101101 },// R G
   { 200, 0b00000000 },
   { 800, 0b00111111 },// RYG
   { 200, 0b00000000 },
   { 800, 0b00111111 },// RYG
   { 200, 0b00000000 },
   { 800, 0b00111111 },// RYG
   { 200, 0b00000000 },

};
static uint8_t led_blink_table_idx = 0;

void led_init( void )
{
    for ( uint8_t i = 0; i < INIT_CMD_MAX; i++ )
    {
        i2c_write_blocking( I2C_PORT, MCP23017_SADR, &MCP23017_INIT_CMD[ i ][ 0 ], 2, false );
    }
}

void led_main( void )
{
    if ( led_time_1ms >= LED_BLINK_TABLE[ led_blink_table_idx ].time )
    {
        led_blink_table_idx++;
        if ( led_blink_table_idx >= LED_BLINK_TABLE_MAX )
        {
            led_blink_table_idx = 0;
        }

        {
            uint8_t buf[ 2 ];
            buf[ 0 ] = ADR_GPIOA;
            buf[ 1 ] = LED_BLINK_TABLE[ led_blink_table_idx ].bit_pattern_A;
            i2c_write_blocking( I2C_PORT, MCP23017_SADR, &buf[ 0 ], 2, false );
        }

        led_time_1ms = 0;
    }

}

void led_timer_inc( void )
{
    if ( led_time_1ms < 0xFFFF )
    {
        led_time_1ms++;
    }
}
