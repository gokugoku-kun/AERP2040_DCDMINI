/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2023 gokugoku
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "audio_pwm.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "sample_encoding.h"
#include "audio_pwm.pio.h"

// TODO: add noise shaped fixed dither


#define audio_pio __CONCAT(pio, PICO_AUDIO_PWM_PIO)
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_PWM_PIO)
#define DREQ_PIOx_TX0 __CONCAT(__CONCAT(DREQ_PIO, PICO_AUDIO_PWM_PIO), _TX0)

struct {
    audio_buffer_t *playing_buffer;
    uint32_t freq;
    uint8_t pio_sm;
    uint8_t dma_channel;
} shared_state;

audio_format_t pwm_consumer_format;
audio_buffer_format_t pwm_consumer_buffer_format = {
        .format = &pwm_consumer_format,
        .sample_stride = sizeof(pwm_cmd_t)
};
static audio_buffer_pool_t *audio_pwm_consumer;

// ======================
// == DEBUGGING =========

#define ENABLE_PIO_AUDIO_PWM_ASSERTIONS

CU_REGISTER_DEBUG_PINS(audio_timing, audio_underflow)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(audio_timing)

// ======================

#ifdef ENABLE_PIO_AUDIO_PWM_ASSERTIONS
#define audio_assert(x) assert(x)
#else
#define audio_assert(x) (void)0
#endif

#define _UNDERSCORE(x, y) x ## _ ## y
#define _CONCAT(x, y) _UNDERSCORE(x,y)
#define audio_program _CONCAT(program_name,program)
#define audio_program_get_default_config _CONCAT(program_name,program_get_default_config)
#define audio_entry_point _CONCAT(program_name,offset_entry_point)


static void __isr __time_critical_func(audio_pwm_dma_irq_handler)();


static inline void audio_start_dma_transfer() {
    assert(!shared_state.playing_buffer);
    audio_buffer_t *ab = take_audio_buffer(audio_pwm_consumer, false);

    shared_state.playing_buffer = ab;
    if (!ab) {
        DEBUG_PINS_XOR(audio_timing, 1);
        DEBUG_PINS_XOR(audio_timing, 2);
        DEBUG_PINS_XOR(audio_timing, 1);
        //DEBUG_PINS_XOR(audio_timing, 2);
        // just play some silence
        static uint32_t zero=(127<<7);
        dma_channel_config c = dma_get_channel_config(shared_state.dma_channel);
        channel_config_set_read_increment(&c, false);
        dma_channel_set_config(shared_state.dma_channel, &c, false);
        dma_channel_transfer_from_buffer_now(shared_state.dma_channel, &zero, PICO_AUDIO_PWM_SILENCE_BUFFER_SAMPLE_LENGTH);
        return;
    }
    assert(ab->sample_count);
    // todo better naming of format->format->format!!
    assert(ab->format->format->format == AUDIO_BUFFER_FORMAT_PCM_S16);

    assert(ab->format->format->channel_count == 1);
    assert(ab->format->sample_stride == 2);

    dma_channel_config c = dma_get_channel_config(shared_state.dma_channel);
    channel_config_set_read_increment(&c, true);
    dma_channel_set_config(shared_state.dma_channel, &c, false);
    dma_channel_transfer_from_buffer_now(shared_state.dma_channel, ab->buffer->bytes, ab->sample_count*2);
}

// irq handler for DMA
void __isr __time_critical_func(audio_pwm_dma_irq_handler)() {
#if PICO_AUDIO_PWM_NOOP
    assert(false);
#else
    uint dma_channel = shared_state.dma_channel;
    if (dma_irqn_get_channel_status(PICO_AUDIO_PWM_DMA_IRQ, dma_channel)) {
        dma_irqn_acknowledge_channel(PICO_AUDIO_PWM_DMA_IRQ, dma_channel);
        DEBUG_PINS_SET(audio_timing, 4);
        // free the buffer we just finished
        if (shared_state.playing_buffer) {
            give_audio_buffer(audio_pwm_consumer, shared_state.playing_buffer);
#ifndef NDEBUG
            shared_state.playing_buffer = NULL;
#endif
        }
        audio_start_dma_transfer();
        DEBUG_PINS_CLR(audio_timing, 4);
    }
#endif
}


const audio_format_t *audio_pwm_setup(const audio_format_t *intended_audio_format,
                                               const audio_pwm_channel_config_t *channel_config0) {

    const audio_format_t *format = intended_audio_format;
    const audio_pwm_channel_config_t *config = channel_config0;
    assert(intended_audio_format);
    assert(channel_config0);

    uint offset = pio_add_program(audio_pio, &audio_program);

    irq_add_shared_handler(DMA_IRQ_0 + PICO_AUDIO_PWM_DMA_IRQ, audio_pwm_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);

// 1ch only
//    for(int ch = 0; ch < format->channel_count; ch++)
    {

        gpio_set_function(config->core.base_pin, GPIO_FUNC_PIOx);

        uint8_t sm = shared_state.pio_sm = config->core.pio_sm;
        pio_sm_claim(audio_pio, sm);

        pio_sm_config sm_config = audio_program_get_default_config(offset);
        sm_config_set_out_pins(&sm_config, config->core.base_pin, 1);
        sm_config_set_sideset_pins(&sm_config, config->core.base_pin);
        // disable auto-pull for !OSRE (which doesn't work with auto-pull)
        static_assert(CYCLES_PER_SAMPLE <= 18, "");
        sm_config_set_out_shift(&sm_config, true, false, CMD_BITS + CYCLES_PER_SAMPLE);
        pio_sm_init(audio_pio, sm, offset, &sm_config);

        pio_sm_set_consecutive_pindirs(audio_pio, sm, config->core.base_pin, 1, true);
        pio_sm_set_pins(audio_pio, sm, 0);

        // todo this should be part of sm_init
        pio_sm_exec(audio_pio, sm, pio_encode_jmp(offset + audio_entry_point)); // jmp to ep

        uint8_t dma_channel = config->core.dma_channel;
        dma_channel_claim(dma_channel);

        shared_state.dma_channel = dma_channel;

        dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);

        channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);

        channel_config_set_dreq(&dma_config, DREQ_PIOx_TX0 + sm);
        dma_channel_configure(dma_channel,
                              &dma_config,
                              &audio_pio->txf[sm],  // dest
                              NULL, // src
                              0, // count
                              false // trigger
        );
        dma_irqn_set_channel_enabled(PICO_AUDIO_PWM_DMA_IRQ, dma_channel, 1);
        config = 0;
    }

    return intended_audio_format;
}


bool audio_pwm_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, uint buffer_count,
                                 uint samples_per_buffer, audio_connection_t *connection) {
    assert(connection);
    printf("Connecting PIO PWM audio\n");

    //connection->producer_pool_give=producer_pool_blocking_give_to_pwm_s16; // overwite

    // todo we need to pick a connection based on the frequency - e.g. 22050 can be more simply upsampled to 44100
    assert(producer->format->format == AUDIO_BUFFER_FORMAT_PCM_S16);
    pwm_consumer_format.format = AUDIO_BUFFER_FORMAT_PCM_S16;
    // todo we could do mono
    // todo we can't match exact, so we should return what we can do
    pwm_consumer_format.sample_freq = producer->format->sample_freq;

    pwm_consumer_format.channel_count = 1;
    pwm_consumer_buffer_format.sample_stride = 2;

    audio_pwm_consumer = audio_new_consumer_pool(&pwm_consumer_buffer_format, buffer_count, samples_per_buffer);

//    update_pio_frequency(producer->format->sample_freq);

    // todo cleanup threading
    __mem_fence_release();

    audio_complete_connection(connection, producer, audio_pwm_consumer);
    return true;
}


bool audio_pwm_connect_thru(audio_buffer_pool_t *producer, audio_connection_t *connection) {
    return audio_pwm_connect_extra(producer, false, 2, 256, connection);
}

static bool audio_enabled;

void audio_pwm_set_enabled(bool enabled) {
    if (enabled != audio_enabled) {
#ifndef NDEBUG
        if (enabled)
        {
            puts("Enabling PIO PWM audio\n");
        }
#endif
        irq_set_enabled(DMA_IRQ_0 + PICO_AUDIO_PWM_DMA_IRQ, enabled);

        if (enabled)
        {
            audio_start_dma_transfer();
        }

        pio_sm_set_enabled(audio_pio, shared_state.pio_sm, enabled);

        audio_enabled = enabled;
    }
}
