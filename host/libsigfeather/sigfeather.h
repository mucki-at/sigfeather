//!@author mucki (code@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include <memory>
#include <string_view>
#include <vector>

class SigFeather
{
public:
    class DeviceManager;
    class IDevice
    {
    public:
        virtual ~IDevice() = default;  

        virtual std::string getManufacturer() const = 0;
        virtual std::string getProduct() const = 0;
        virtual std::string getSerialNumber() const = 0;
        virtual std::string getAddress() const = 0;

        virtual void open() =0;
        virtual void close() =0;
        virtual bool isOpen() const =0;

        virtual size_t benchmark(size_t bytes) const =0;
        virtual std::vector<uint8_t> sample(size_t samples) const =0;
    };

    using DeviceHandle=std::shared_ptr<IDevice>;
    using DeviceFoundCallback=bool(*)(DeviceHandle device, void* user_data);

public:
    SigFeather();
    ~SigFeather();

    DeviceHandle findDevice(std::string_view serialNumber={}) const;
    void enumerateDevices(DeviceFoundCallback callback, void* user_data) const;

private:
    std::shared_ptr<DeviceManager> deviceManager;
};
