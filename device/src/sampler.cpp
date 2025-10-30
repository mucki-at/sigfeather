//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.

#include "sampler.h"
#include "samplePin.pio.h"
#include <stdexcept>
#include <hardware/gpio.h>

Sampler::Sampler(uint pinNumber) :
    pio(nullptr),
    sm(0),
    offset(0),
    pinNumber(pinNumber),
    dma(false)
{
    if (!dma.isValid())
    {
        return;
    }

    // claim PIO instance
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(&samplePin_program, &pio, &sm, &offset, pinNumber, 1, true))
    {
        pio=nullptr;
        return;
    }

    pio_sm_config c = samplePin_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pinNumber); // we will set the pin later
    sm_config_set_clkdiv_int_frac8(&c, 150*100, 0); // 10khz
    pio_sm_init(pio, sm, offset + samplePin_wrap_target, &c);

    gpio_set_dir(pinNumber, GPIO_IN);
    gpio_set_function(pinNumber, static_cast<gpio_function_t>(pio_get_funcsel(pio)));
    gpio_set_pulls(pinNumber, false, false);

    // configure DMA
    auto config = dma.getDefaultConfig();
    channel_config_set_dreq(&config, pio_get_dreq(pio, sm, false));
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    dma.configure(&config, nullptr, &pio->rxf[sm], 0, false);
}

Sampler::~Sampler()
{
    if (pio!=nullptr)
    {
        dma.stop();
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
        gpio_set_input_enabled(pinNumber, false);
        pio_remove_program_and_unclaim_sm(&samplePin_program, pio, sm, offset);
        pio = nullptr;
    }
}

size_t Sampler::prepareSampling(void* buffer, size_t bufferSizeInBytes, size_t& sampleCount)
{
    if (bufferSizeInBytes == 0 || buffer == nullptr)
    {
        return 0;
    }

    size_t requiredWords = (sampleCount+31)/32;
    if (bufferSizeInBytes < requiredWords*4)
    {
        auto maxWords = bufferSizeInBytes / 4;
        sampleCount = maxWords * 32;
        requiredWords = maxWords;
    }
    return requiredWords * 4;
}

void Sampler::startSampling(void* buffer, size_t bufferSizeInBytes, size_t& sampleCount)
{
    auto requiredWords = (prepareSampling(buffer, bufferSizeInBytes, sampleCount)+3)/4;

    // start sampling
    expectedTransferCount=requiredWords;
    gpio_set_input_enabled(pinNumber, true);
    pio_sm_set_enabled(pio, sm, true);
    dma.transferToBufferNow(buffer, requiredWords);
}

volatile size_t Sampler::getBytesAvailable() const
{
    if (pio==nullptr)
    {
        return 0;
    }

    if (!dma.isRunning())
    {
        pio_sm_set_enabled(pio, sm, false); // stop PIO when DMA is done
        gpio_set_input_enabled(pinNumber, false);
        return expectedTransferCount*4;
    }
    return (expectedTransferCount - dma.getTransferCount())*4;
}