//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.

#include <tusb.h>
#include <pico/unique_id.h>
#include "usbinterface.h"
#include "logging.h"
#include "usbstrings.h"

#define LOG_USB_TRANSFERS 0

//--------------------------------------------------------------------+
// USB interface implementation
//--------------------------------------------------------------------+

USBInterface* USBInterface::instance = nullptr;

USBInterface::USBInterface(IProtocolHandler& handler) :
    handler(handler)
{
    instance = this;
}

bool USBInterface::reportStatus(uint8_t rhport, tusb_control_request_t const* request, Status status)
{
    uint8_t response = static_cast<uint8_t>(status);
    return tud_control_xfer(rhport, request, &response, sizeof(response));
}


bool USBInterface::handleControlTransfer(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    static uint8_t dataBuffer[64]; // large enough for any request USB allows
    auto cmd = Command(request->bRequest);

    if (stage == CONTROL_STAGE_SETUP)
    {
        // if request has a response, setup response transfer here:
        // tud_control_xfer(rhport, request, response_buffer, response_length);
        // or use tud_control_status(rhport, request) to send zero-length packet (OK response)
        // if request has incoming data, provide a buffer for it with
        // tud_control_xfer(rhport, request, data_buffer, data_length);
        switch ( cmd )
        {
        case Command::Open:     return reportStatus(rhport, request, handler.open());
        case Command::Close:    return reportStatus(rhport, request, handler.close());
        case Command::Start:    return reportStatus(rhport, request, handler.start());
        case Command::Stop:     return reportStatus(rhport, request, handler.stop());
        case Command::GetStatus: return reportStatus(rhport, request, handler.getStatus());

        case Command::ConfigureSession:
            return tud_control_xfer(rhport, request, dataBuffer, sizeof(SessionConfiguration));

        case Command::GetSessionConfiguration:
            {
                SessionConfiguration config=handler.getSessionConfiguration();
                return tud_control_xfer(rhport, request, reinterpret_cast<uint8_t*>(&config), sizeof(config));
            }
        default:
            return false;
        }
    }
    else if ( stage == CONTROL_STAGE_DATA )
    {
        // if we sent or received data, this is where we would process it
        switch ( cmd )
        {
        case Command::ConfigureSession:
            handler.configureSession(*reinterpret_cast<SessionConfiguration*>(dataBuffer));
            break;
        default:
            break;
        }
        return true;
    }
    else
    {
        // this is the status stage (after data)
        return true;
    }
}

bool USBInterface::transferComplete(uint8_t rhport, xfer_result_t result, uint32_t xferred_bytes)
{
    return true;
}

//--------------------------------------------------------------------+
// TinyUSB Descriptors
//--------------------------------------------------------------------+
namespace
{


tusb_desc_device_t const sigfeather_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_UNSPECIFIED,   // use class from interface
    .bDeviceSubClass    = 0x00,                     // use subclass from interface
    .bDeviceProtocol    = 0x00,                     // use protocol from interface

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USBInterface::VendorId,
    .idProduct          = USBInterface::ProductId,
    .bcdDevice          = 0x0100,

    .iManufacturer      = USBStrings::addNoCopy("mucki.dev"),
    .iProduct           = USBStrings::addNoCopy("SigFeather"),
    .iSerialNumber      = USBStrings::getSerialNumberStringIndex(),

    .bNumConfigurations = 0x01
};

struct TU_ATTR_PACKED configuration_t
{
    tusb_desc_configuration_t configuration;
    tusb_desc_interface_t interface;
    tusb_desc_endpoint_t dataOut;
};

configuration_t sigfeather_configuration =
{
    .configuration =
    {
        .bLength = sizeof(tusb_desc_configuration_t),
        .bDescriptorType = TUSB_DESC_CONFIGURATION,
        .wTotalLength = sizeof(configuration_t),

        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = 0,    // configuration doesn't have a name by itself
        .bmAttributes = 0x80,   // bit 7 is mandatory, we have no other features
        .bMaxPower = 250,       // request 500mA (max allowed)
    },
    .interface =
    {
        .bLength = sizeof(tusb_desc_interface_t),
        .bDescriptorType = TUSB_DESC_INTERFACE,

        .bInterfaceNumber = USBInterface::InterfaceId,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,     // our data endpoint. configuration is handled via the control ep0
        .bInterfaceClass = TUSB_CLASS_VENDOR_SPECIFIC,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = TUSB_CLASS_VENDOR_SPECIFIC,
        .iInterface = USBStrings::addNoCopy("SigFeather"),
    },
    .dataOut =
    {
        .bLength = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType = TUSB_DESC_ENDPOINT,

        .bEndpointAddress = TUSB_DIR_IN_MASK | USBInterface::EndpointAddress,
        .bmAttributes = { .xfer=TUSB_XFER_BULK },
        .wMaxPacketSize = TUSB_EPSIZE_BULK_FS,  // 64
        .bInterval = 1  //1 frame (1ms atfull speed)
    }
};

}

// The following callbacks are declared in TinyUSB but not (or weakly)
// defined. We provide implementations to configure TinyUSB behavior.
extern "C"
{

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
const uint8_t* tud_descriptor_device_cb(void)
{
    return reinterpret_cast<const uint8_t*>(&sigfeather_device);
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
const uint8_t* tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations
    return reinterpret_cast<const uint8_t*>(&sigfeather_configuration);
}


bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
#if LOG_USB_TRANSFERS
    Info("tud_vendor_control_xfer_cb(rhport=%u, stage=%u, request=%u bytes", rhport, stage, sizeof(tusb_control_request_t));
    Hexdump(request, sizeof(tusb_control_request_t));
#endif
 
    return USBInterface::instance->handleControlTransfer(rhport, stage, request);
}

} // end extern "C"


