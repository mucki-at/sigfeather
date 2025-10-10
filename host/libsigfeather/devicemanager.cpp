//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.

#include "devicemanager.h"
#include <stdexcept>

DeviceManager::DeviceManager()
{
    if (libusb_init(&usbContext) < 0)
        throw std::runtime_error("Failed to initialize libusb");
}

DeviceManager::~DeviceManager()
{
    if (usbContext)
        libusb_exit(usbContext);
    usbContext = nullptr;   
}

