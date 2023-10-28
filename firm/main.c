/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * This source file is the result of prototyping work. It is not yet very examplary.
 * In the future it will be cleaned up and commented for a blog post.
 * In the meanwhile enter at your own risk.
 */

#include "user_define.h"

#define IMAGE_DATA_K 188
#define AUDIO_BUFFER_B 512*3

#define IMAGE_DATA_WORDS (IMAGE_DATA_K * 256)
// todo see where we write off the end of this (hence need for + 128)
uint32_t image_data[
        128 + IMAGE_DATA_K * 1024 / 4] = {1}; // force into data not BSS

#define NUM_AUDIO_BUFFERS 2
uint32_t audio_buffer[AUDIO_BUFFER_B * NUM_AUDIO_BUFFERS / 4] = {1};
uint32_t *const audio_buffer_start[NUM_AUDIO_BUFFERS] = {
        audio_buffer,
        audio_buffer + AUDIO_BUFFER_B * 1 / 4,
#if NUM_AUDIO_BUFFERS > 2
        audio_buffer  + AUDIO_BUFFER_B * 2 / 4
#endif
};

struct audio_buffer *audio_buffers[NUM_AUDIO_BUFFERS];
struct audio_buffer_pool *audio_buffer_pool;

// + 1 for null end marker, and +1 for split buffer during wrap
static uint32_t scatter[(PICO_SD_MAX_BLOCK_COUNT + 2) * 4];

static uint32_t frame_header_sector[128];

#define PLATYPUS_MAGIC (('T'<<24)|('A'<<16)|('L'<<8)|'P')

struct frame_header {
    uint32_t mark0;
    uint32_t mark1;
    uint32_t magic;
    uint8_t major, minior, debug, spare;
    uint32_t sector_number; // relative to start of stream
    uint32_t frame_number;
    uint8_t hh, mm, ss, ff; // good old CD days (bcd)
    uint32_t header_words;
    uint16_t width;
    uint16_t height;
    uint32_t image_words;
    uint32_t audio_words; // just to confirm really
    uint32_t audio_freq;
    uint8_t audio_channels; // always assume 16 bit
    uint8_t pad[3];
    uint32_t unused[4]; // little space for expansion
    uint32_t total_sectors;
    uint32_t last_sector;
    // 1, 2, 4, 8 frame increments
    uint32_t forward_frame_sector[4];
    uint32_t backward_frame_sectors[4];
    // h/2 + 1 row_offsets, last one should == image_words
    uint16_t row_offsets[];
} __attribute__((packed));

static struct decoder_state_state {
    enum {
        INIT,
        NEW_FRAME,
        NEED_FRAME_HEADER_SECTOR,
        READING_FRAME_HEADER_SECTOR,
        NEED_VIDEO_SECTORS,
        PREPPING_VIDEO_SECTORS,
        READING_VIDEO_SECTORS,
        AWAIT_AUDIO_BUFFER,
        NEED_AUDIO_SECTORS,
        READING_AUDIO_SECTORS,
        AUDIO_BUFFER_READY,
        POST_PROCESSING_AUDIO_SECTORS,
        ERROR,
        FRAME_READY,
        HIT_END,
    } state;
    struct {
        uint32_t sector_base;
        uint16_t frame_base_row;
        uint16_t write_buffer_offset;
        uint16_t remaining_row_words;
        // todo we should combine these
        uint16_t frame_row_count;
        uint16_t row_index;
    } video_read, video_read_rollback;
    struct {
        uint32_t sector_base;
        uint16_t sector_count;
    } current_sd_read;
    struct {
        uint16_t valid_from_row;
        uint16_t valid_to_row;
        // we display from frame_start to valid_to_row, and then wrap back prior to display_start (i.e.
        // remaining data from previous frame in a pinch)...
        // todo right now we always try and keep MOVIE_ROWS valid lines ending at valid_to_row
        uint16_t display_start_row;
    } rows;
    struct {
        volatile enum {
            BS_EMPTY, BS_FILLING, BS_QUEUED
        } buffer_state[2];
        uint load_thread_buffer_index;
        uint32_t sector_base;
    } audio;
    bool hold_frame;
    bool paused;
    uint8_t unpause;
    bool awaiting_first_frame;
    uint32_t display_time_code;
    bool loaded_audio_this_frame;
} ds;

static uint32_t waste[128]; // todo we can move this to no write thru XIP cache alias

void popcorn_producer_pool_give_buffer(struct audio_connection *connection, struct audio_buffer *buffer) {
    // hand directly to consumer
    queue_full_audio_buffer(connection->consumer_pool, buffer);
}

void popcorn_consumer_pool_give_buffer(struct audio_connection *connection, struct audio_buffer *buffer) {
    for (int i = 0; i < NUM_AUDIO_BUFFERS; i++) {
        if (buffer == audio_buffers[i]) {
            ds.audio.buffer_state[i] = BS_EMPTY;
            return;
        }
    }

    __breakpoint();
    __builtin_unreachable();
}

static struct audio_connection popcorn_passthru_connection = {
        .consumer_pool_take = consumer_pool_take_buffer_default,
        .consumer_pool_give = popcorn_consumer_pool_give_buffer,
        .producer_pool_take = producer_pool_take_buffer_default,
        .producer_pool_give = popcorn_producer_pool_give_buffer,
};

static void main_init( void );
static bool __isr __time_critical_func(timer_callback)( repeating_timer_t *rt );

static void __time_critical_func(handle_new_frame)() {
    printf("newframe_sector:%d\n",ds.current_sd_read.sector_base);
    gpio_xor_mask(1u<<P_DEBUG);
    ds.state = NEED_FRAME_HEADER_SECTOR;
    ds.loaded_audio_this_frame = false;
}
static void __time_critical_func(handle_need_frame_header_sector)(struct frame_header *head) {
    head->mark0 = 0; // mark as invalid
    const int sector_count = 1;
    assert(sector_count <= PICO_SD_MAX_BLOCK_COUNT);
    uint32_t *p = scatter;
    for (int i = 0; i < sector_count; i++) {
        *p++ = native_safe_hw_ptr((frame_header_sector + i * 128));
        *p++ = 128;
        // for now we read the CRCs also
        *p++ = native_safe_hw_ptr(waste);
        *p++ = 2;
    }
    *p++ = 0;
    *p++ = 0;
    ds.current_sd_read.sector_count = sector_count;
    sd_readblocks_scatter_async(scatter, ds.current_sd_read.sector_base, ds.current_sd_read.sector_count);
    ds.state = READING_FRAME_HEADER_SECTOR;
}
static void __time_critical_func(handle_reading_frame_header_sector)() {
    if (sd_scatter_read_complete(NULL)) {
        struct frame_header *head = (struct frame_header *) frame_header_sector;
        if (head->mark0 != 0xffffffff || head->mark1 != 0xffffffff || head->magic != PLATYPUS_MAGIC) {
            printf("no header found @ %d\n", (int) ds.current_sd_read.sector_base);
            ds.current_sd_read.sector_base++;
            ds.state = NEED_FRAME_HEADER_SECTOR;
        } else {
            assert(ds.current_sd_read.sector_count == 1);
            if (head->header_words > 128) {
                printf("expect 1 sector header\n");
                ds.current_sd_read.sector_base++;
                ds.state = NEED_FRAME_HEADER_SECTOR;
            } else {
                //movies[registered_current_movie].current_sector = ds.current_sd_read.sector_base;
                ds.display_time_code = (head->hh << 24u) | (head->mm << 16u) | (head->ss << 8u) | (head->ff);
                ds.current_sd_read.sector_base++;
                // skip audio sectors
                ds.audio.sector_base = ds.current_sd_read.sector_base;
                ds.current_sd_read.sector_base += (head->audio_words + 127) / 128;
                ds.video_read.sector_base = ds.current_sd_read.sector_base;
                ds.video_read.frame_base_row = ds.rows.valid_to_row;
                ds.video_read.frame_row_count = 0;
                ds.state = NEED_VIDEO_SECTORS;
            }
        }
    }
}
static void __time_critical_func(handle_reading_video_sectors)() {
    assert(ds.current_sd_read.sector_count);
    if (sd_scatter_read_complete(NULL)) {
        ds.video_read.sector_base += ds.current_sd_read.sector_count;
        ds.current_sd_read.sector_base = ds.video_read.sector_base;
        ds.current_sd_read.sector_count = 0;
        ds.state = NEED_VIDEO_SECTORS;
    }
}
static void handle_init(const struct frame_header *head) {
//    movie_count = 1;
//    movies = static_movies;
    if (sd_init_4pins() < 0) {
        ds.state = ERROR;
    } else {
        printf("No GPT found, so assuming single movie\n");
    }
    //sd_set_clock_divider(2);
    ds.audio.buffer_state[0] = ds.audio.buffer_state[1] = BS_EMPTY;
    ds.audio.load_thread_buffer_index = 0;
    ds.rows.valid_from_row = ds.rows.valid_to_row = 0;
    ds.rows.display_start_row = 0;
    ds.awaiting_first_frame = 1;
    ds.hold_frame = true;
//    registered_current_movie = current_movie;
//    ds.current_sd_read.sector_base = movies[current_movie].start_sector;
    ds.current_sd_read.sector_base = 0;
    ds.state = NEW_FRAME;
}
static void __time_critical_func(handle_await_audio_buffer)() {
    if( (ds.audio.buffer_state[ds.audio.load_thread_buffer_index] == BS_EMPTY ) ){
        ds.audio.buffer_state[ds.audio.load_thread_buffer_index] = BS_FILLING;
        ds.state = NEED_AUDIO_SECTORS;
    }
}
static void __time_critical_func(handle_hit_end)() {
    if (ds.hold_frame && ds.audio.buffer_state[ds.audio.load_thread_buffer_index] == BS_EMPTY &&
!ds.loaded_audio_this_frame) {
        ds.audio.buffer_state[ds.audio.load_thread_buffer_index] = BS_FILLING;
        ds.state = NEED_AUDIO_SECTORS;
    } else {
        ds.state = NEED_VIDEO_SECTORS;
    }
}
static void __time_critical_func(handle_reading_audio_sectors)(const struct frame_header *head) {
    if (sd_scatter_read_complete(NULL)) {
        // todo we have DMA completely capable of reading backwards - seems like a strange thing to expose in any lower level API though
        //  still we could use DMA to reverse the buffers for us (although it is a bit complicated to not step on our toes)
        ds.state = AUDIO_BUFFER_READY;
    }
}
static void __time_critical_func(handle_post_processing_audio_sectors)() {

}
static void __time_critical_func(handle_frame_ready)(const struct frame_header *head) {
    // not much to do really now other than start reading data for next sector
    ds.awaiting_first_frame = false;
    uint index = 0;
    uint32_t next_sector;

    lcd_trans_image(&image_data[0], head->image_words*2);

    if (ds.paused) {
        next_sector = head->sector_number;
    } else {
        next_sector = head->forward_frame_sector[index];
    }
    if (next_sector == 0xffffffff) {
        next_sector = 0;
    }
    ds.current_sd_read.sector_base = next_sector;

    ds.state = NEW_FRAME;
}
static void __time_critical_func(handle_need_video_sectors)() {
    ds.video_read_rollback = ds.video_read;
    ds.state = PREPPING_VIDEO_SECTORS;
}
static void __time_critical_func(handle_need_audio_sectors)(const struct frame_header *head) {
    assert(ds.audio.buffer_state[ds.audio.load_thread_buffer_index] == BS_FILLING);
    audio_buffers[ds.audio.load_thread_buffer_index]->sample_count = head->audio_words;
    // todo update sd.current_read_sector for consistency...
    //  can't do it until we pick the next frame sector explicitly rather than just happening into it.
    sd_readblocks_async(audio_buffer_start[ds.audio.load_thread_buffer_index], ds.audio.sector_base,
                        (head->audio_words + 127) / 128);
    ds.state = READING_AUDIO_SECTORS;
}
static void __time_critical_func(handle_prep_video_sectors)(struct frame_header *head) {
    bool done = false;
    // we are making a decision about how many sectors we can read (we read up to MAX_BLOCK_COUNT - 1) in case one is split
    uint32_t *p = scatter;
    int sector_count = 0;
    uint32_t part1_buffer_offset;
    uint32_t part1_size;
    // note part2 is always loaded at ram offset 0
    uint32_t part2_size;
    // constraint 1) we must have enough space for the scatter information (part1 + (part2) + CRC) * 2 = 6
    while (sector_count < PICO_SD_MAX_BLOCK_COUNT && p < scatter + count_of(scatter) - 6) {
        // we must read a whole sector
        if ( ds.video_read.sector_base > head->sector_number +(head->audio_words + 127) / 128 + (head->image_words + 127) / 128)
        {
            done = true;
            break;
        }
        if ( ds.video_read.sector_base + sector_count <= head->sector_number +(head->audio_words + 127) / 128 + (head->image_words + 127) / 128) {
            part1_size=128;
            part2_size=0;
            part1_buffer_offset = 128 * ((ds.video_read.sector_base + sector_count)-(head->sector_number +(head->audio_words + 127) / 128 + 1));
        } else {
            break;
        }

        assert(part1_size + part2_size == 128);

        *p++ = native_safe_hw_ptr(image_data + part1_buffer_offset);
        *p++ = part1_size;
        if (part2_size) {
            *p++ = native_safe_hw_ptr(image_data); // part2 always at start of buffer
            *p++ = part2_size;
        }
        // CRC
        *p++ = native_safe_hw_ptr(waste);
        *p++ = 2;
        sector_count++;
    }
    if (sector_count) {
        *p++ = 0;
        *p++ = 0;
        assert(p <= scatter + count_of(scatter));
        ds.current_sd_read.sector_base = ds.video_read.sector_base;
        ds.current_sd_read.sector_count = sector_count;
        sd_readblocks_scatter_async(scatter, ds.video_read.sector_base, sector_count);
        ds.state = READING_VIDEO_SECTORS;
    } else if (done) {
        if (!ds.loaded_audio_this_frame) {
            ds.state = AWAIT_AUDIO_BUFFER;
        } else {
            ds.state = FRAME_READY;
        }
    } else {
        ds.state = HIT_END;
    }
}
static void __time_critical_func(handle_audio_buffer_ready)(const struct frame_header *head) {
    ds.loaded_audio_this_frame = true;
    ds.audio.buffer_state[ds.audio.load_thread_buffer_index] = BS_QUEUED;

    audio_buffers[ds.audio.load_thread_buffer_index]->buffer->bytes = (uint8_t *) audio_buffer_start[ds.audio.load_thread_buffer_index];

    give_audio_buffer(audio_buffer_pool, audio_buffers[ds.audio.load_thread_buffer_index]);

    ds.audio.load_thread_buffer_index++;
    if (ds.audio.load_thread_buffer_index == NUM_AUDIO_BUFFERS) ds.audio.load_thread_buffer_index = 0;
    ds.state = NEED_VIDEO_SECTORS;
}

static void __attribute__((noinline)) __time_critical_func(sd_state_update)() {
    struct frame_header *head = (struct frame_header *) frame_header_sector;
    switch (ds.state) {
        case NEW_FRAME:
            handle_new_frame();
            break;
        case NEED_FRAME_HEADER_SECTOR:
            handle_need_frame_header_sector(head);
            break;
        case READING_FRAME_HEADER_SECTOR:
            handle_reading_frame_header_sector();
            break;
        case READING_VIDEO_SECTORS:
            handle_reading_video_sectors();
            break;
        case INIT:
            handle_init(head);
            break;
        case ERROR:
            panic("doh!");
        case AWAIT_AUDIO_BUFFER:
            handle_await_audio_buffer();
            break;
        case HIT_END:
            handle_hit_end();
            break;
        case READING_AUDIO_SECTORS:
            handle_reading_audio_sectors(head);
            break;
        case POST_PROCESSING_AUDIO_SECTORS:
            handle_post_processing_audio_sectors();
            break;
        case FRAME_READY:
            handle_frame_ready(head);
            break;
        case NEED_VIDEO_SECTORS:
            handle_need_video_sectors();
            break;
        case NEED_AUDIO_SECTORS:
            handle_need_audio_sectors(head);
            break;
        case PREPPING_VIDEO_SECTORS:
        case AUDIO_BUFFER_READY:
            // handled below because we want to be able to do them in conjunction immediately after the above
            break;
    }
    if (ds.state == PREPPING_VIDEO_SECTORS) {
        handle_prep_video_sectors(head);
    } else if (ds.state == AUDIO_BUFFER_READY) {
        handle_audio_buffer_ready(head);
    }
}


void setup_audio() {
    static const audio_format_t platypus_audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .sample_freq = 22050,//TODO
            .channel_count = 1,//TODO
    };

    static struct audio_buffer_format producer_format = {
            .format = &platypus_audio_format,
            .sample_stride = 2//TODO
    };

    static const audio_pwm_channel_config_t config = {
                .core = {
                        .base_pin = P_PWM,
                        .pio_sm = PWM_PIO_SM,
                        .dma_channel = PWM_DMA_CH
                },
                .pattern = 3,
    };

    audio_buffer_pool = audio_new_producer_pool(&producer_format, 0, 0);
    for (int i = 0; i < NUM_AUDIO_BUFFERS; i++) {
        audio_buffers[i] = audio_new_wrapping_buffer(&producer_format,
                                                     pico_buffer_wrap((uint8_t *) audio_buffer_start[i],
                                                                      AUDIO_BUFFER_B));
    }

    const struct audio_format *output_format;
    output_format = audio_pwm_setup(&platypus_audio_format, &config);
    if (!output_format) {
        panic("Unable to open audio device.");
    }

    bool ok = audio_pwm_connect_thru(audio_buffer_pool, &popcorn_passthru_connection);
    if (!ok) {
        panic("Failed to connect audio");
    }
}


void main( void )
{
    set_sys_clock_48mhz();

    stdio_init_all();
    sleep_ms(5000);
    printf("FIRM_VER:%s\n",FIRM_VER);
    printf("CLK %d\n", PICO_SD_CLK_PIN);
    printf("CMD %d\n", PICO_SD_CMD_PIN);
    printf("DAT %d\n", PICO_SD_DAT0_PIN);
    main_init();
    led_init();

    ds.state = INIT;
    ds.awaiting_first_frame = true;
    setup_audio();
    audio_pwm_set_enabled(true);


    while ( true )
    {
        lcd_main();
        Lcd2Main();
        led_main();
        if ( lcd_is_ready() && lcd2_is_ready() )
        {
            sd_state_update();
        }
    }
}

static void main_init( void )
{
    /* GPIO初期設定 */
    {
        gpio_init( P_LCD_CS     );
        gpio_init( P_LCD_RS     );
        gpio_init( P_LCD_WR     );
        gpio_init( P_LCD_RD     );
        gpio_init( P_LCD_D0     );
        gpio_init( P_LCD_D1     );
        gpio_init( P_LCD_D2     );
        gpio_init( P_LCD_D3     );
        gpio_init( P_LCD_D4     );
        gpio_init( P_LCD_D5     );
        gpio_init( P_LCD_D6     );
        gpio_init( P_LCD_D7     );
        gpio_init( P_LCD_RESET  );
        gpio_init( P_LCD2_SCK   );
        gpio_init( P_LCD2_TX    );
        gpio_init( P_LCD2_RX    );
        gpio_init( P_LCD2_CS    );
        gpio_init( P_LCD2_CD    );
        gpio_init( P_LCD2_RESET );
        //gpio_init( P_PWM        );
        gpio_init( P_DEBUG        );

        gpio_set_dir( P_LCD_CS    , GPIO_OUT );
        gpio_set_dir( P_LCD_RS    , GPIO_OUT );
        gpio_set_dir( P_LCD_WR    , GPIO_OUT );
        gpio_set_dir( P_LCD_RD    , GPIO_OUT );
        gpio_set_dir( P_LCD_D0    , GPIO_OUT );
        gpio_set_dir( P_LCD_D1    , GPIO_OUT );
        gpio_set_dir( P_LCD_D2    , GPIO_OUT );
        gpio_set_dir( P_LCD_D3    , GPIO_OUT );
        gpio_set_dir( P_LCD_D4    , GPIO_OUT );
        gpio_set_dir( P_LCD_D5    , GPIO_OUT );
        gpio_set_dir( P_LCD_D6    , GPIO_OUT );
        gpio_set_dir( P_LCD_D7    , GPIO_OUT );
        gpio_set_dir( P_LCD_RESET , GPIO_OUT );
        gpio_set_dir( P_LCD2_SCK  , GPIO_OUT );
        gpio_set_dir( P_LCD2_TX   , GPIO_OUT );
        gpio_set_dir( P_LCD2_RX   , GPIO_OUT );
        gpio_set_dir( P_LCD2_CS   , GPIO_OUT );
        gpio_set_dir( P_LCD2_CD   , GPIO_OUT );
        gpio_set_dir( P_LCD2_RESET, GPIO_OUT );
        //gpio_set_dir( P_PWM       , GPIO_OUT );
        gpio_set_dir( P_DEBUG       , GPIO_OUT );
    }

    /* SPI初期設定 */
    {
        spi_init( LCD2_SPI_CH, 5 * 1000 * 1000 );
        spi_set_format( LCD2_SPI_CH, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST );
        gpio_set_function( P_LCD2_RX, GPIO_FUNC_SPI );
        gpio_set_function( P_LCD2_SCK, GPIO_FUNC_SPI );
        gpio_set_function( P_LCD2_TX, GPIO_FUNC_SPI );
    }

    {
        i2c_init( I2C_PORT, 400*1000 );
        gpio_set_function( I2C_SDA, GPIO_FUNC_I2C );
        gpio_set_function( I2C_SCL, GPIO_FUNC_I2C );
        gpio_pull_up( I2C_SDA );
        gpio_pull_up( I2C_SCL );
    }

    /* タイマ初期設定 */
    {
        static repeating_timer_t timer;
        /* 1msインターバルタイマ設定 */
        add_repeating_timer_us( -1000, &timer_callback, NULL, &timer );
    }

}

/* 1ms割り込みでコールすること */
static bool __isr __time_critical_func(timer_callback)( repeating_timer_t *rt )
{
    lcd_timer_inc();
    Lcd2TimerInc();
    led_timer_inc();
    return ( true );
}
