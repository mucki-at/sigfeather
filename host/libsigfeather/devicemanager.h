//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include <libusb.h>
#include "sigfeather.h"
#include "device.h"
#include <memory>

class SigFeather::DeviceManager
{
public:
    static constexpr int VID_SIGFEATHER = 0x1209;
    static constexpr int PID_SIGFEATHER = 0x7366;

public:
    DeviceManager();
    ~DeviceManager();

    template<typename Callback>
    void findUsbDevices(Callback callback)
    {
        libusb_device** list = nullptr;
        ssize_t count = libusb_get_device_list(usbContext, &list);
        
        for (ssize_t i = 0; i < count; ++i)
        {
            libusb_device* device = list[i];
            libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(device, &desc) == 0)
            {
                if (desc.idVendor == VID_SIGFEATHER && desc.idProduct == PID_SIGFEATHER)
                {
                    if (callback(std::make_shared<Device>(device, desc))) break;
                }
            }
        }

        libusb_free_device_list(list, 1);
        return;
    }

private:
    libusb_context* usbContext = nullptr;
};
