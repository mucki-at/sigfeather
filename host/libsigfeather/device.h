//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include <libusb.h>
#include <string>
#include "sigfeather.h"
#include "protocol.h"

class Device : public SigFeather::IDevice
{
public:
    Device(libusb_device* device, const libusb_device_descriptor& desc);
    ~Device();

    virtual std::string getManufacturer() const override;
    virtual std::string getProduct() const override;
    virtual std::string getSerialNumber() const override;
    virtual std::string getAddress() const override;

    virtual void open() override;
    virtual void close() override;
    virtual bool isOpen() const override { return opened; }

    virtual size_t benchmark(size_t bytes) const override;
    virtual std::vector<uint8_t> sample(size_t samples) const override;

private:
    libusb_device* device = nullptr;
    libusb_device_handle* handle = nullptr;
    libusb_device_descriptor descriptor{};

    bool opened = false;
    uint8_t interfaceId=0;
    uint8_t endpoint=0;

    inline uint16_t readControlResult(Command command, uint16_t param, void* buffer, uint16_t maxBytes, unsigned int timeout=1000) const
    {
        int result=libusb_control_transfer(
            handle,
            LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
            static_cast<uint8_t>(command), param, interfaceId,
            static_cast<unsigned char*>(buffer), maxBytes,
            timeout
        );
        if (result<0) throw std::runtime_error(std::string("failed to send control command: ") + libusb_error_name(result));
        if (result>maxBytes) throw std::runtime_error("control command returned too many bytes!");
        return result;
    }

    inline uint16_t writeControlBuffer(Command command, uint16_t param, const void* buffer, uint16_t maxBytes, unsigned int timeout=1000) const
    {
        int result=libusb_control_transfer(
            handle,
            LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
            static_cast<uint8_t>(command), param, interfaceId,
            const_cast<unsigned char*>(static_cast<const unsigned char*>(buffer)), maxBytes,
            timeout
        );
        if (result<0) throw std::runtime_error(std::string("failed to send control command: ") + libusb_error_name(result));
        if (result>maxBytes) throw std::runtime_error("control command sent too many bytes!");
        return result;
    }


    template<typename ResultType>
    ResultType readCommand(Command command, uint16_t param, unsigned int timeout=1000) const
    {
        ResultType retval;
        auto bytes=readControlResult(command, param, &retval, sizeof(retval), timeout);
        if (bytes != sizeof(retval)) throw std::runtime_error("Unexpected command result size");
        return retval;
    }

    template<typename BufferType>
    void writeCommand(Command command, uint16_t param, const BufferType& buffer, unsigned int timeout=1000) const
    {
        auto bytes=writeControlBuffer(command, param, &buffer, sizeof(buffer), timeout);
        if (bytes != sizeof(buffer)) throw std::runtime_error("Command completed only partially");
    }
};