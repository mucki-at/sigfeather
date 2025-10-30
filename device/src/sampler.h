//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include <hardware/pio.h>
#include "dmatransfer.h"

class Sampler
{
public:
    Sampler(uint pinNumber);
    ~Sampler();

    // not copyable
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    // movable
    Sampler& operator=(Sampler&& rhs)
    {
        std::swap(pio, rhs.pio);
        std::swap(sm, rhs.sm);
        std::swap(offset, rhs.offset);
        std::swap(dma, rhs.dma);
        return *this;
    }
    Sampler(Sampler&& rhs) : pio(std::exchange(rhs.pio, nullptr)),
                             sm(std::exchange(rhs.sm, 0)),
                             offset(std::exchange(rhs.offset, 0)),
                             dma(std::move(rhs.dma))
    {
    }

    inline bool isValid() const { return dma.isValid() && pio!=nullptr; }
    inline bool isRunning() const { return dma.isRunning(); }

    size_t prepareSampling(void* buffer, size_t bufferSizeInBytes, size_t& sampleCount);
    void startSampling(void* buffer, size_t bufferSizeInBytes, size_t& sampleCount);
    volatile size_t getBytesAvailable() const;

private:
    PIO pio;
    uint sm;
    uint offset;
    uint pinNumber;
    DMATransfer dma;
    uint32_t expectedTransferCount=0;
};