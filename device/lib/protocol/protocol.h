//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include <cstdint>

enum class Command : uint8_t
{
    Open = 0x01,
    Close = 0x02,
    GetStatus = 0x03,
    Start = 0x10,
    Stop = 0x11,
    ConfigureSession = 0x21,
    GetSessionConfiguration = 0x22
};

enum class Status : uint8_t
{
    Closed = 0x00,
    Opened = 0x01,
    Running = 0x02,
    Error = 0xFF
};

enum class SessionType : uint8_t
{
    Benchmark = 0x00,
    SingleBit = 0x01
};

struct [[gnu::packed]] SessionConfiguration
{
    SessionType type=SessionType::Benchmark;
    uint32_t sampleCount=0;
    uint32_t bytesLeft=0;
};
static_assert(sizeof(SessionConfiguration) == 9, "SessionConfiguration size mismatch");

class IProtocolHandler
{
public:
    virtual ~IProtocolHandler() = default;

    virtual Status getStatus() = 0;
    virtual Status open() = 0;
    virtual Status close() = 0;
    virtual Status start() = 0;
    virtual Status stop() = 0;

    virtual void configureSession(SessionConfiguration& config) = 0;
    virtual SessionConfiguration getSessionConfiguration() = 0;

};