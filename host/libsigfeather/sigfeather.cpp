//!@author mucki (code@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.

#include "sigfeather.h"
#include "devicemanager.h"
#include <mutex>
#include <iostream>
#include <system_error>

namespace
{
    std::shared_ptr<SigFeather::DeviceManager> globalDeviceManager()
    {
        static std::weak_ptr<SigFeather::DeviceManager> global;
        static std::mutex mutex;
        std::scoped_lock lock(mutex);
        auto instance = global.lock();
        if (!instance)
        {
            instance = std::make_shared<SigFeather::DeviceManager>();
            global = instance;
        }
        return instance;
    }
}


SigFeather::SigFeather() :
    deviceManager(globalDeviceManager())
{
}

SigFeather::~SigFeather()
{
}

SigFeather::DeviceHandle SigFeather::findDevice(std::string_view serialNumber) const
{
    SigFeather::DeviceHandle foundDevice = nullptr;
    deviceManager->findUsbDevices([&foundDevice,serialNumber](DeviceHandle device)
        {
            if (serialNumber.empty() || serialNumber==device->getSerialNumber())
            {
                foundDevice = device;
                return false; // stop enumeration
            }
            else
            {
                return true; // continue enumeration
            }
        }
    );
    return foundDevice;
}

void SigFeather::enumerateDevices(DeviceFoundCallback callback, void* user_data) const
{
    deviceManager->findUsbDevices([callback,user_data](DeviceHandle device)
        {
            return callback(device, user_data);
        }
    );
}


