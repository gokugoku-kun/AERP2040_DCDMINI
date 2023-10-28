/*
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

//ControllerIC NV3030B
#include "user_define.h"
#include "lcd2_data.h"

static uint8_t lcd2_state = LCD2_STATE_POWER_ON;
volatile static uint8_t lcd2_timer_1ms = 0;

static void Lcd2StateEntryPowerOn( void );
static void Lcd2StateEntryNormal( void );
static void Lcd2StateDoPowerOn( void );
static void Lcd2StateDoNormal( void );
static void Lcd2StateExitPowerOn( void );
static void Lcd2StateExitNormal( void );
static uint8_t Lcd2GetNewState( uint8_t now_state );
static void Lcd2Send( uint8_t is_data, uint8_t data );
    #define LCD2_SEND_CMD     (0)
    #define LCD2_SEND_DATA    (1)
static void LCD_SetPos(uint16_t Xstart,uint16_t Ystart,uint16_t Xend,uint16_t Yend);
static void ClearScreen(uint16_t bColor);
void (* const Lcd2StateEntry[ LCD2_STATE_MAX ])( void ) =
{
    &Lcd2StateEntryPowerOn,
    &Lcd2StateEntryNormal,
};

void (* const Lcd2StateDo[ LCD2_STATE_MAX ])( void ) =
{
    &Lcd2StateDoPowerOn,
    &Lcd2StateDoNormal,
};

void (* const Lcd2StateExit[ LCD2_STATE_MAX ])( void ) =
{
    &Lcd2StateExitPowerOn,
    &Lcd2StateExitNormal,
};

void Lcd2Main( void )
{
    uint8_t new_state;

    /* 定常処理 */
    Lcd2StateDo[ lcd2_state ]();

    /* 次状態決定 */
    new_state = Lcd2GetNewState( lcd2_state );

    if ( lcd2_state != new_state )
    {
        /* 退場処理 */
        Lcd2StateExit[ lcd2_state ]();
        /* 入場処理 */
        lcd2_state = new_state;
        Lcd2StateEntry[ lcd2_state ]();
    }
}

static uint8_t Lcd2GetNewState( uint8_t now_state )
{
    uint8_t new_state;

    new_state = now_state;

    switch ( now_state )
    {
        case LCD2_STATE_POWER_ON:
            /* 無条件遷移 */
            new_state = LCD2_STATE_NORMAL;
            break;
        
        case LCD2_STATE_NORMAL:
            new_state = LCD2_STATE_NORMAL;
            break;

        default:
            new_state = LCD2_STATE_POWER_ON;
            break;
    }

    return ( new_state );
}

static void Lcd2StateEntryPowerOn( void )
{

}

static void Lcd2StateEntryNormal( void )
{

}

static void Lcd2StateDoPowerOn( void )
{

}

static void Lcd2StateDoNormal( void )
{

}

static void Lcd2StateExitPowerOn( void )
{
    // ハードウェアRESET
    {
        gpio_put( P_LCD2_RESET   , 1 );
        lcd2_timer_1ms = 0;
        while ( lcd2_timer_1ms < 100 ){}
        gpio_put( P_LCD2_RESET   , 0 );
        lcd2_timer_1ms = 0;
        while ( lcd2_timer_1ms < 100 ){}
        gpio_put( P_LCD2_RESET   , 1 );
        lcd2_timer_1ms = 0;
        while ( lcd2_timer_1ms < 100 ){}
    }

    /* ポート初期化 */
    {
        gpio_put( P_LCD2_CS   , 1 );
        gpio_put( P_LCD2_CS   , 0 );
    }

    // 初期化
    {
        Lcd2Send(  LCD2_SEND_CMD, 0x11); // exit sleep
        lcd2_timer_1ms = 0;
        while ( lcd2_timer_1ms < 120 ){}

        Lcd2Send(  LCD2_SEND_CMD, 0xfd);//private_access
        Lcd2Send( LCD2_SEND_DATA, 0x06);
        Lcd2Send( LCD2_SEND_DATA, 0x08);

    // 	Lcd2Send(  LCD2_SEND_CMD, 0x60);//
    //	Lcd2Send( LCD2_SEND_DATA, 0x01);//osc_user_adj[3:0] 07
    //	Lcd2Send( LCD2_SEND_DATA, 0x01);//
    //	Lcd2Send( LCD2_SEND_DATA, 0x01);//01

        Lcd2Send(  LCD2_SEND_CMD, 0x61);//add
        Lcd2Send( LCD2_SEND_DATA, 0x07);//
        Lcd2Send( LCD2_SEND_DATA, 0x04);//

    //bias
        Lcd2Send(  LCD2_SEND_CMD, 0x62);//bias setting
        Lcd2Send( LCD2_SEND_DATA, 0x00);//01
        Lcd2Send( LCD2_SEND_DATA, 0x44);//04 44
        Lcd2Send( LCD2_SEND_DATA, 0x40);//44 65 40
    //	Lcd2Send( LCD2_SEND_DATA, 0x01);//06 vref_adj

        Lcd2Send(  LCD2_SEND_CMD, 0x63);//
        Lcd2Send( LCD2_SEND_DATA, 0x41);//
        Lcd2Send( LCD2_SEND_DATA, 0x07);//
        Lcd2Send( LCD2_SEND_DATA, 0x12);//
        Lcd2Send( LCD2_SEND_DATA, 0x12);//

        Lcd2Send(  LCD2_SEND_CMD, 0x64);//
        Lcd2Send( LCD2_SEND_DATA, 0x37);//

    //VSP
        Lcd2Send(  LCD2_SEND_CMD, 0x65);//Pump1=4.7MHz //PUMP1 VSP
        Lcd2Send( LCD2_SEND_DATA, 0x09);//D6-5:pump1_clk[1:0] clamp 28 2b
        Lcd2Send( LCD2_SEND_DATA, 0x10);//6.26
        Lcd2Send( LCD2_SEND_DATA, 0x21);

    //VSN
        Lcd2Send(  LCD2_SEND_CMD, 0x66); //pump=2 AVCL
        Lcd2Send( LCD2_SEND_DATA, 0x09); //clamp 08 0b 09
        Lcd2Send( LCD2_SEND_DATA, 0x10); //10
        Lcd2Send( LCD2_SEND_DATA, 0x21);

    //add source_neg_time
        Lcd2Send(  LCD2_SEND_CMD, 0x67);//pump_sel
        Lcd2Send( LCD2_SEND_DATA, 0x20);//21 20
        Lcd2Send( LCD2_SEND_DATA, 0x40);

    //gamma vap/van
        Lcd2Send(  LCD2_SEND_CMD, 0x68);//gamma vap/van
        Lcd2Send( LCD2_SEND_DATA, 0x90);//90
        Lcd2Send( LCD2_SEND_DATA, 0x4c);//
        Lcd2Send( LCD2_SEND_DATA, 0x7c);//VCOM
        Lcd2Send( LCD2_SEND_DATA, 0x06);//66

        Lcd2Send(  LCD2_SEND_CMD, 0xb1);//frame rate
        Lcd2Send( LCD2_SEND_DATA, 0x0F);//0x0f fr_h[5:0] 0F
        Lcd2Send( LCD2_SEND_DATA, 0x02);//0x02 fr_v[4:0] 02
        Lcd2Send( LCD2_SEND_DATA, 0x01);//0x04 fr_div[2:0] 03

        Lcd2Send(  LCD2_SEND_CMD, 0xB4);
        Lcd2Send( LCD2_SEND_DATA, 0x01); //01:1dot 00:column
    ////porch
        Lcd2Send(  LCD2_SEND_CMD, 0xB5);
        Lcd2Send( LCD2_SEND_DATA, 0x02);//0x02 vfp[6:0]
        Lcd2Send( LCD2_SEND_DATA, 0x02);//0x02 vbp[6:0]
        Lcd2Send( LCD2_SEND_DATA, 0x0a);//0x0A hfp[6:0]
        Lcd2Send( LCD2_SEND_DATA, 0x14);//0x14 hbp[6:0]

        Lcd2Send(  LCD2_SEND_CMD, 0xB6);
        Lcd2Send( LCD2_SEND_DATA, 0x04);//
        Lcd2Send( LCD2_SEND_DATA, 0x01);//
        Lcd2Send( LCD2_SEND_DATA, 0x9f);//
        Lcd2Send( LCD2_SEND_DATA, 0x00);//
        Lcd2Send( LCD2_SEND_DATA, 0x02);//

    ////gamme sel
        Lcd2Send(  LCD2_SEND_CMD, 0xdf);//
        Lcd2Send( LCD2_SEND_DATA, 0x11);//gofc_gamma_en_sel=1
    ////gamma_test1 A1#_wangly
    //3030b_gamma_new_
    //GAMMA---------------------------------/////////////

    //GAMMA---------------------------------/////////////
        Lcd2Send(  LCD2_SEND_CMD, 0xE2);	
        Lcd2Send( LCD2_SEND_DATA, 0x03);//vrp0[5:0]	V0
        Lcd2Send( LCD2_SEND_DATA, 0x00);//vrp1[5:0]	V1  
        Lcd2Send( LCD2_SEND_DATA, 0x00);//vrp2[5:0]	V2 
        Lcd2Send( LCD2_SEND_DATA, 0x26);//vrp3[5:0]	V61 
        Lcd2Send( LCD2_SEND_DATA, 0x27);//vrp4[5:0]	V62
        Lcd2Send( LCD2_SEND_DATA, 0x3f);//vrp5[5:0]	V63

        Lcd2Send(  LCD2_SEND_CMD, 0xE5);	
        Lcd2Send( LCD2_SEND_DATA, 0x3f);//vrn0[5:0]	V63
        Lcd2Send( LCD2_SEND_DATA, 0x27);//vrn1[5:0]	V62	
        Lcd2Send( LCD2_SEND_DATA, 0x26);//vrn2[5:0]	V61 
        Lcd2Send( LCD2_SEND_DATA, 0x00);//vrn3[5:0]	V2 
        Lcd2Send( LCD2_SEND_DATA, 0x00);//vrn4[5:0]	V1	
        Lcd2Send( LCD2_SEND_DATA, 0x03);//vrn5[5:0]  V0

        Lcd2Send(  LCD2_SEND_CMD, 0xE1);	
        Lcd2Send( LCD2_SEND_DATA, 0x00);//prp0[6:0]	V15
        Lcd2Send( LCD2_SEND_DATA, 0x57);//prp1[6:0]	V51 

        Lcd2Send(  LCD2_SEND_CMD, 0xE4);	
        Lcd2Send( LCD2_SEND_DATA, 0x58);//prn0[6:0]	V51 
        Lcd2Send( LCD2_SEND_DATA, 0x00);//prn1[6:0]  V15

        Lcd2Send(  LCD2_SEND_CMD, 0xE0);
        Lcd2Send( LCD2_SEND_DATA, 0x01);//pkp0[4:0]	V3 
        Lcd2Send( LCD2_SEND_DATA, 0x03);//pkp1[4:0]	V7  
        Lcd2Send( LCD2_SEND_DATA, 0x0d);//pkp2[4:0]	V21 
        Lcd2Send( LCD2_SEND_DATA, 0x0e);//pkp3[4:0]	V29 //
        Lcd2Send( LCD2_SEND_DATA, 0x0e);//pkp4[4:0]	V37 
        Lcd2Send( LCD2_SEND_DATA, 0x0c);//pkp5[4:0]	V45 
        Lcd2Send( LCD2_SEND_DATA, 0x15);//pkp6[4:0]	V56 
        Lcd2Send( LCD2_SEND_DATA, 0x19);//pkp7[4:0]	V60 

        Lcd2Send(  LCD2_SEND_CMD, 0xE3);	
        Lcd2Send( LCD2_SEND_DATA, 0x1a);//pkn0[4:0]	V60 
        Lcd2Send( LCD2_SEND_DATA, 0x16);//pkn1[4:0]	V56 
        Lcd2Send( LCD2_SEND_DATA, 0x0c);//pkn2[4:0]	V45 
        Lcd2Send( LCD2_SEND_DATA, 0x0f);//pkn3[4:0]	V37 
        Lcd2Send( LCD2_SEND_DATA, 0x0e);//pkn4[4:0]	V29 //
        Lcd2Send( LCD2_SEND_DATA, 0x0d);//pkn5[4:0]	V21 
        Lcd2Send( LCD2_SEND_DATA, 0x02);//pkn6[4:0]	V7  
        Lcd2Send( LCD2_SEND_DATA, 0x01);//pkn7[4:0]	V3 
    //GAMMA---------------------------------/////////////

    //source
        Lcd2Send(  LCD2_SEND_CMD, 0xE6);
        Lcd2Send( LCD2_SEND_DATA, 0x00);
        Lcd2Send( LCD2_SEND_DATA, 0xff);//SC_EN_START[7:0] f0

        Lcd2Send(  LCD2_SEND_CMD, 0xE7);
        Lcd2Send( LCD2_SEND_DATA, 0x01);//CS_START[3:0] 01
        Lcd2Send( LCD2_SEND_DATA, 0x04);//scdt_inv_sel cs_vp_en
        Lcd2Send( LCD2_SEND_DATA, 0x03);//CS1_WIDTH[7:0] 12
        Lcd2Send( LCD2_SEND_DATA, 0x03);//CS2_WIDTH[7:0] 12
        Lcd2Send( LCD2_SEND_DATA, 0x00);//PREC_START[7:0] 06
        Lcd2Send( LCD2_SEND_DATA, 0x12);//PREC_WIDTH[7:0] 12

        Lcd2Send(  LCD2_SEND_CMD, 0xE8); //source
        Lcd2Send( LCD2_SEND_DATA, 0x00); //VCMP_OUT_EN 81-
        Lcd2Send( LCD2_SEND_DATA, 0x70); //chopper_sel[6:4]
        Lcd2Send( LCD2_SEND_DATA, 0x00); //gchopper_sel[6:4] 60
    ////gate
        Lcd2Send(  LCD2_SEND_CMD, 0xEc);
        Lcd2Send( LCD2_SEND_DATA, 0x52);//52

        Lcd2Send(  LCD2_SEND_CMD, 0xF1);
        Lcd2Send( LCD2_SEND_DATA, 0x01);//te_pol tem_extend 00 01 03
        Lcd2Send( LCD2_SEND_DATA, 0x01);
        Lcd2Send( LCD2_SEND_DATA, 0x02);

        Lcd2Send(  LCD2_SEND_CMD, 0xF6);//
        Lcd2Send( LCD2_SEND_DATA, 0x09);//
        Lcd2Send( LCD2_SEND_DATA, 0x10);//
        Lcd2Send( LCD2_SEND_DATA, 0x00);//
        Lcd2Send( LCD2_SEND_DATA, 0x00);//40 3ﾏﾟ2ﾍｨｵﾀ

        Lcd2Send(  LCD2_SEND_CMD, 0xfd);
        Lcd2Send( LCD2_SEND_DATA, 0xfa);
        Lcd2Send( LCD2_SEND_DATA, 0xfc);

        Lcd2Send(  LCD2_SEND_CMD, 0x3a);
        Lcd2Send( LCD2_SEND_DATA, 0x05);//SH 0x66

        Lcd2Send(  LCD2_SEND_CMD, 0x35);
        Lcd2Send( LCD2_SEND_DATA, 0x00);

        Lcd2Send(  LCD2_SEND_CMD, 0x36 );
        Lcd2Send( LCD2_SEND_DATA, 0x08 );

        Lcd2Send(  LCD2_SEND_CMD, 0x21); 

        Lcd2Send(  LCD2_SEND_CMD, 0x29); // display on
        lcd2_timer_1ms = 0;
        while ( lcd2_timer_1ms < 10 ){}

        ClearScreen(0x0000);//allblack
        ClearScreen(0x0000);//allblack
    }

    {
        {
            uint32_t i;
            uint8_t data;
            LCD_SetPos( LCD2_PIC_HEIGHT_OFFSET, LCD2_PIC_WIDTH_OFFSET, LCD2_PIC_HEIGHT_OFFSET+LCD2_PIC_HEIGHT-1, LCD2_PIC_WIDTH_OFFSET+LCD2_PIC_WIDTH-1);
            for ( i = 0; i < (LCD2_PIC_WIDTH * LCD2_PIC_HEIGHT); i++ )
            {
                data = (uint8_t)( LCD2_PIXEL_DATA[ i ] >> 8 );
                Lcd2Send(  LCD2_SEND_DATA, data );
                data = (uint8_t)( LCD2_PIXEL_DATA[ i ] & 0x00FF );
                Lcd2Send(  LCD2_SEND_DATA, data );
            }
        }
        gpio_put( P_LCD2_CS   , 1 );
    }
    printf("LCD2 ready\n");
}

static void Lcd2StateExitNormal( void )
{

}

void Lcd2TimerInc( void )
{
    if ( lcd2_timer_1ms < 0xFF )
    {
        lcd2_timer_1ms++;
    }
}

static void Lcd2Send( uint8_t is_data, uint8_t data )
{
    gpio_put( P_LCD2_CD   , is_data );
    {
        uint8_t buf[1];
        buf[0]=data;
        spi_write_blocking( LCD2_SPI_CH, buf, 1 );
    }

}

static void LCD_SetPos(uint16_t Xstart,uint16_t Ystart,uint16_t Xend,uint16_t Yend)
{
   Xstart+= 0; Xend+= 0;
   Ystart+=20; Yend+=20;

	Lcd2Send(  LCD2_SEND_CMD, 0x2a);   
	Lcd2Send( LCD2_SEND_DATA, Xstart>>8);
	Lcd2Send( LCD2_SEND_DATA, Xstart);
 	Lcd2Send( LCD2_SEND_DATA, Xend>>8);
	Lcd2Send( LCD2_SEND_DATA, Xend);

	Lcd2Send(  LCD2_SEND_CMD, 0x2b);   
	Lcd2Send( LCD2_SEND_DATA, Ystart>>8);
	Lcd2Send( LCD2_SEND_DATA, Ystart);
	Lcd2Send( LCD2_SEND_DATA, Yend>>8);
	Lcd2Send( LCD2_SEND_DATA, Yend);
	Lcd2Send(  LCD2_SEND_CMD, 0x2C);//LCD_WriteCMD(GRAMWR);
}

//clear screen  
static void ClearScreen(uint16_t bColor)
{
    unsigned int i,j;
    LCD_SetPos( 0, 0, LCD2_HEIGHT-1, LCD2_WIDTH-1);
    for ( i = 0; i < LCD2_WIDTH; i++)
    {
        for ( j = 0; j < LCD2_HEIGHT; j++ )
        {
            Lcd2Send(  LCD2_SEND_DATA, bColor>>8 );
            Lcd2Send(  LCD2_SEND_DATA, bColor&0xFF );
        }
    }
}
uint16_t Lcd2_get_state( void )
{
    return ( lcd2_state );
}

bool lcd2_is_ready( void )
{
    if ( lcd2_state == LCD2_STATE_NORMAL )
    {
        return ( true );
    }
    else
    {
        return ( false );
    }
}
