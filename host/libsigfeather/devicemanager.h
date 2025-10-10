//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include <libusb.h>
#include <format>

class DeviceManager
{
public:
    DeviceManager();
    ~DeviceManager();

    template<typename Callback>
    int findUsbDevices(Callback callback)
    {
        libusb_device** list = nullptr;
        ssize_t count = libusb_get_device_list(usbContext, &list);
        
        for (ssize_t i = 0; i < count; ++i)
        {
            libusb_device* device = list[i];
            libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(device, &desc) == 0)
            {
                callback(std::format("Device {:04x}:{:04x}", desc.idVendor, desc.idProduct).c_str());
            }
        }

        libusb_free_device_list(list, 1);
        return 0;
    }

private:
    libusb_context* usbContext = nullptr;
};