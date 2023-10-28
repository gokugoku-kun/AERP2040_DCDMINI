/*
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

//ControllerIC R61509V
#include "user_define.h"
#include "lcd.pio.h"

#define LCD_HORIZON_ADDRES_START     (0)
#define LCD_HORIZON_ADDRES_END         (240)
#define LCD_VERTICAL_ADDRES_START   (0)
#define LCD_VERTICAL_ADDRES_END       (399)
typedef struct
{
    uint16_t time_1ms;
    bool level;
} lcd_reset_data;

#define LCD_RESET_DATA_TABLE_MAX     ( 3 )
static const lcd_reset_data LCD_RESET_DATA_TABLE[ LCD_RESET_DATA_TABLE_MAX ] =
{
    {  1, 1 },
    {  1, 0 },
    {  1, 1 },
};

typedef struct
{
    uint16_t time_1ms;
    uint32_t data;
} lcd_init_data;

#define LCD_INIT_DATA_TABLE_MAX     ( 41 )
static const lcd_init_data LCD_INIT_DATA_TABLE[ LCD_INIT_DATA_TABLE_MAX ] =
{
    { 0, 0x00000000 },
    { 0, 0x00000000 },
    { 1, 0x00000000 },
    { 0, 0x0400E200 },
    { 0, 0x00080808 },
    { 0, 0x03000C00 },
    { 0, 0x03015A0B },
    { 0, 0x03020906 },
    { 0, 0x03031017 },
    { 0, 0x03042300 },
    { 0, 0x03051700 },
    { 0, 0x03066309 },
    { 0, 0x03070C09 },
    { 0, 0x0308010C },
    { 0, 0x03092232 },
    { 0, 0x00100010 },
    { 0, 0x00110101 },
    { 0, 0x00120000 },
    { 0, 0x00130001 },
    { 0, 0x01000330 },
    { 0, 0x01010336 },
    { 0, 0x01031000 },
    { 0, 0x02806100 },
    { 1, 0x0102BBB4 },
    { 0, 0x00010000 },
    { 0, 0x00020000 },
    { 0, 0x00031000 },
    { 0, 0x00090001 },
    { 0, 0x000C0000 },
    { 0, 0x00900800 },
    { 0, 0x000F0000 },
    { 0, 0x02100000 },
    { 0, 0x021100EF },
    { 0, 0x02120000 },
    { 0, 0x0213018F },
    { 0, 0x05000000 },
    { 0, 0x05010000 },
    { 0, 0x0502005F },
    { 0, 0x04010001 },
    { 1, 0x04040000 },
    { 0, 0x00070100 },
};

static uint16_t lcd_idx = 0;
static uint16_t lcd_idx_old = 0xFF;
static bool lcd_is_finish = false;
static volatile uint16_t lcd_time_1ms = 0;

static uint16_t lcd_state = LCD_STATE_POWER_ON;

static PIO lcd_pio = LCD_PIO;
static uint lcd_pio_offset;
static uint lcd_pio_sm = LCD_PIO_SM;

static int lcd_dma_chan = LCD_DMA_CH;
static dma_channel_config lcd_dma_config;
#define LCD_READ_BLOCK  (25)
static uint8_t lcd_data_buff[2][SD_SECTOR_SIZE*LCD_READ_BLOCK];
static uint8_t lcd_data_buuf_idx=0;
static uint32_t block_idx=0;
static void lcd_dma_handler();


static void lcd_entry_power_on( void );
static void lcd_entry_reset( void );
static void lcd_entry_init( void );
static void lcd_entry_draw( void );

static void lcd_do_power_on( void );
static void lcd_do_reset( void );
static void lcd_do_init( void );
static void lcd_do_draw( void );

static void lcd_exit_power_on( void );
static void lcd_exit_reset( void );
static void lcd_exit_init( void );
static void lcd_exit_draw( void );

static uint16_t lcd_get_new_state( uint16_t now );
static void lcd_send( bool is_data, uint16_t data );
    #define LCD_SEND_CMD     ( 0 )
    #define LCD_SEND_DATA    ( 1 )


/* 入場処理 */
void ( * const lcd_entry[ LCD_STATE_MAX ] )( void ) =
{
    &lcd_entry_power_on,
    &lcd_entry_reset,
    &lcd_entry_init,
    &lcd_entry_draw,
};

/* 定常処理 */
void ( * const lcd_do[ LCD_STATE_MAX ] )( void ) =
{
    &lcd_do_power_on,
    &lcd_do_reset,
    &lcd_do_init,
    &lcd_do_draw,
};

/* 退場処理 */
void ( * const lcd_exit[ LCD_STATE_MAX ] )( void ) =
{
    &lcd_exit_power_on,
    &lcd_exit_reset,
    &lcd_exit_init,
    &lcd_exit_draw,
};

void lcd_main( void )
{
    uint16_t new_state;

    /* 定常処理 */
    lcd_do[ lcd_state ]();

    /* 次状態決定 */
    new_state = lcd_get_new_state( lcd_state );

    if ( lcd_state != new_state )
    {
        /* 退場処理 */
        lcd_exit[ lcd_state ]();
        /* 入場処理 */
        lcd_state = new_state;
        lcd_entry[ lcd_state ]();
    }
}

static uint16_t lcd_get_new_state( uint16_t now )
{
    uint16_t new = now;

    switch ( now )
    {
    case LCD_STATE_POWER_ON :
        new = LCD_STATE_RESET;
        break;

    case LCD_STATE_RESET :
        /* リセット完了 */
        if ( lcd_is_finish != false )
        {
            new = LCD_STATE_INIT;
        }
        break;

    case LCD_STATE_INIT :
        /* 初期化完了 */
        if ( lcd_is_finish != false )
        {
            new = LCD_STATE_DRAW;
        }
        break;

    case LCD_STATE_DRAW :
        /* DO NOTHING */
        break;

    default:
        new = LCD_STATE_POWER_ON;
        break;
    }

    return ( new );
}

static void lcd_entry_power_on( void )
{

}

static void lcd_entry_reset( void )
{
    lcd_idx = 0;
    lcd_idx_old = 0xFF;
    lcd_is_finish = false;
    lcd_time_1ms = 0;
}

static void lcd_entry_init( void )
{
    gpio_put( P_LCD_CS   , 0 );
    lcd_idx = 0;
    lcd_idx_old = 0xFF;
    lcd_is_finish = false;
    lcd_time_1ms = 0;
}

static void lcd_entry_draw( void )
{
    gpio_put( P_LCD_CS   , 0 );
    lcd_idx = 0;
    lcd_idx_old = 0xFF;
    lcd_is_finish = false;
    lcd_time_1ms = 0;
    lcd_send( LCD_SEND_CMD , 0x0200 );
    lcd_send( LCD_SEND_DATA, LCD_HORIZON_ADDRES_START );
    lcd_send( LCD_SEND_CMD , 0x0201 );
    lcd_send( LCD_SEND_DATA, LCD_VERTICAL_ADDRES_START );
    lcd_send( LCD_SEND_CMD , 0x0202 );

    {
        lcd_pio_offset = pio_add_program( lcd_pio, &pio_lcd_program );
            /* 指定したPIOのインストラクションメモリにプログラムをロードする */
            /* プログラムの先頭アドレス(オフセット)が戻り値として返ってくる */
        pio_lcd_program_init( lcd_pio, lcd_pio_sm, lcd_pio_offset, P_LCD_D0, P_LCD_OUT_NUM, P_LCD_CS, P_LCD_SET_NUM );
            /* PIOの初期化を実施する */
    }

    {
        //lcd_dma_chan = dma_claim_unused_channel(true);
        dma_channel_claim(lcd_dma_chan);
        lcd_dma_config = dma_channel_get_default_config(lcd_dma_chan);
        channel_config_set_transfer_data_size(&lcd_dma_config, DMA_SIZE_16);
        channel_config_set_read_increment(&lcd_dma_config, true);
        channel_config_set_dreq(&lcd_dma_config, DREQ_PIO0_TX0+lcd_pio_sm);
        dma_channel_configure(
            lcd_dma_chan,
            &lcd_dma_config,
            &lcd_pio->txf[lcd_pio_sm], // Write address (only need to set this once)
            NULL,             // Don't provide a read address yet
            0, // Write the same value many times, then halt and interrupt
            false             // Don't start yet
        );
    }
}


static void lcd_do_power_on( void )
{
    gpio_put( P_LCD_RESET, 0 );
    gpio_put( P_LCD_CS   , 1 );
    gpio_put( P_LCD_RS   , 0 );
    gpio_put( P_LCD_WR   , 1 );
    gpio_put( P_LCD_RD   , 1 );
    gpio_put( P_LCD_D0   , 0 );
    gpio_put( P_LCD_D1   , 0 );
    gpio_put( P_LCD_D2   , 0 );
    gpio_put( P_LCD_D3   , 0 );
    gpio_put( P_LCD_D4   , 0 );
    gpio_put( P_LCD_D5   , 0 );
    gpio_put( P_LCD_D6   , 0 );
    gpio_put( P_LCD_D7   , 0 );
}

static void lcd_do_reset( void )
{
    if ( lcd_idx != lcd_idx_old )
    {
        gpio_put( P_LCD_RESET, LCD_RESET_DATA_TABLE[ lcd_idx ].level );
        lcd_idx_old = lcd_idx;
    }

    if ( lcd_time_1ms >= LCD_RESET_DATA_TABLE[ lcd_idx ].time_1ms )
    {
        lcd_idx++;
        lcd_time_1ms = 0;
    }

    /* リセットシーケンス完了 */
    if ( lcd_idx >= LCD_RESET_DATA_TABLE_MAX )
    {
        lcd_is_finish = true;
    }
}

static void lcd_do_init( void )
{
    if ( lcd_idx != lcd_idx_old )
    {
        uint16_t cmd;
        uint16_t data;
        cmd = ( uint16_t )( LCD_INIT_DATA_TABLE[ lcd_idx ].data >> 16 );
        lcd_send( LCD_SEND_CMD, cmd );
        data = ( uint16_t )( LCD_INIT_DATA_TABLE[ lcd_idx ].data & 0xFFFF );
        lcd_send( LCD_SEND_DATA, data );
        lcd_idx_old = lcd_idx;
    }

    if ( lcd_time_1ms >= LCD_INIT_DATA_TABLE[ lcd_idx ].time_1ms )
    {
        lcd_idx++;
        lcd_time_1ms = 0;
    }

    /* 初期化シーケンス完了 */
    if ( lcd_idx >= LCD_INIT_DATA_TABLE_MAX )
    {
        lcd_is_finish = true;
    }
}
static uint16_t tmp[512/2]={0x00F8};
static void lcd_do_draw( void )
{

}


static void lcd_exit_power_on( void )
{

}

static void lcd_exit_reset( void )
{

}

static void lcd_exit_init( void )
{
    printf("LCD ready\n");
}

static void lcd_exit_draw( void )
{

}

static void lcd_send( bool is_data, uint16_t data )
{
    gpio_put( P_LCD_RS, is_data );
    gpio_put( P_LCD_WR, 0       );
    {
        uint8_t data_8bit = ( data & 0xFF00 ) >> 8;
        uint32_t value = ( data_8bit ) << P_LCD_D0;
        gpio_put_masked( P_LCD_OUT_MASK, value );
    } 
    gpio_put( P_LCD_WR, 1       );
    gpio_put( P_LCD_WR, 1       );
    gpio_put( P_LCD_WR, 0       );
    {
        uint8_t data_8bit = ( data & 0x00FF );
        uint32_t value = ( data_8bit ) << P_LCD_D0;
        gpio_put_masked( P_LCD_OUT_MASK, value );
    }
    gpio_put( P_LCD_WR, 1       );
    gpio_put( P_LCD_WR, 1       );
}

/* 1ms割り込みでコールすること */
void lcd_timer_inc( void )
{
    if ( lcd_time_1ms < 0xFFFF )
    {
        lcd_time_1ms++;
    }
}

uint16_t lcd_get_state( void )
{
    return ( lcd_state );
}

bool lcd_is_ready( void )
{
    if ( lcd_state == LCD_STATE_DRAW )
    {
        return ( true );
    }
    else
    {
        return ( false );
    }
}

void __time_critical_func(lcd_trans_image)(uint32_t *image_data, uint32_t count)
{
    dma_channel_transfer_from_buffer_now(lcd_dma_chan, image_data, count);
}
