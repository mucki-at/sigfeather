//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include "protocol.h"

class USBInterface
{
public:
    static constexpr uint16_t VendorId = 0x1209;
    static constexpr uint16_t ProductId = 0x7366;
    static constexpr uint8_t InterfaceId = 0;
    static constexpr uint8_t ResetInterfaceId = 1;
    static constexpr uint8_t EndpointAddress = 1;

public:
    USBInterface(IProtocolHandler& handler);

private:
    bool reportStatus(uint8_t rhport, tusb_control_request_t const* request, Status status);
    bool handleControlTransfer(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request);
    bool transferComplete(uint8_t rhport, xfer_result_t result, uint32_t xferred_bytes);

    IProtocolHandler& handler;
    
    friend bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
    static USBInterface* instance;
};

