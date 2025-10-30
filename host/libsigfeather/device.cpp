//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.

#include "device.h"
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace
{
    std::string getStringDescriptor(libusb_device_handle* handle, uint8_t index)
    {
        if (index == 0) return "";
        std::string result;
        result.resize(256);
        int length = libusb_get_string_descriptor_ascii(handle, index, (unsigned char*)result.data(), result.size());
        if (length < 0)
        {
            return libusb_error_name(length);   
        }
        else
        {
            result.resize(length);
            return result;
        }
    }
}

Device::Device(libusb_device* device, const libusb_device_descriptor& desc) :
    SigFeather::IDevice(),
    device(device),
    handle(nullptr),
    descriptor(desc)
{
    if (!device) throw std::invalid_argument("device is null");
    if (libusb_open(device, &handle) != 0)
    {
        this->device = nullptr;
        handle = nullptr;
        throw std::runtime_error("failed to open device");
    }
}

Device::~Device()
{
    close();

    if (handle) libusb_close(handle);
    handle = nullptr;
    device = nullptr;
}

std::string Device::getSerialNumber() const
{
    return getStringDescriptor(handle, descriptor.iSerialNumber);
}

std::string Device::getManufacturer() const
{
    return getStringDescriptor(handle, descriptor.iManufacturer);
}

std::string Device::getProduct() const
{           
    return getStringDescriptor(handle, descriptor.iProduct);
}

std::string Device::getAddress() const
{
    uint8_t bus = libusb_get_bus_number(device);
    uint8_t address = libusb_get_device_address(device);
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%03u:%03u", bus, address);
    return std::string(buffer);
}

void Device::open()
{
    if (opened) return;

    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(device, &config) != 0)
    {
        throw std::runtime_error("failed to get active config descriptor");
    }

    interfaceId = 255;
    for (auto id=0; id < config->bNumInterfaces; ++id)
    {
        const libusb_interface& iface = config->interface[id];
        for (auto alt=0; alt<iface.num_altsetting; ++alt)
        {
            const libusb_interface_descriptor& altsetting = iface.altsetting[alt];
            if (altsetting.bInterfaceClass == 0xFF &&
                altsetting.bInterfaceSubClass == 0x00 &&
                altsetting.bInterfaceProtocol == 0xFF &&
                altsetting.bNumEndpoints == 1)
            {
                // assume first two endpoints are bulk in and out
                endpoint = 0;
                const libusb_endpoint_descriptor& ep = altsetting.endpoint[0];
                if ((ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK)
                {
                    if ((ep.bEndpointAddress & LIBUSB_ENDPOINT_IN)==LIBUSB_ENDPOINT_IN)
                    {
                        endpoint = ep.bEndpointAddress;
                    }
                }
                if (endpoint != 0)
                {
                    libusb_set_interface_alt_setting(handle, altsetting.bInterfaceNumber, altsetting.bAlternateSetting);
                    interfaceId = altsetting.bInterfaceNumber;
                    break; // found suitable interface
                }
            }
        }
    }
    libusb_free_config_descriptor(config);

    if (interfaceId == 255)
    {
        throw std::runtime_error("failed to find sigfeather bulk interface");
    }
    if (libusb_claim_interface(handle, interfaceId) != 0)
    {
        throw std::runtime_error("failed to claim sigfeather bulk interface");
    }

    auto deviceStatus=readCommand<Status>(Command::Open, 0);
    if (deviceStatus!=Status::Opened)
    {
        throw std::runtime_error("Device failed to open properly.");
    }

    opened = true;
}

void Device::close()
{
    if (!opened) return;

    auto deviceStatus=readCommand<Status>(Command::Close, 0);
    if (deviceStatus!=Status::Closed)
    {
        std::cerr << "WARNING: Device failed to close properly. You may have to reset the device." << std::endl;
    }

    if (libusb_release_interface(handle, interfaceId) != 0)
    {
        throw std::runtime_error("failed to release sigfeather bulk interface");
    }
    opened = false;
}


size_t Device::benchmark(size_t bytes) const
{
    if (!opened) return 0;

    SessionConfiguration config;
    config.type=SessionType::Benchmark;
    config.sampleCount=static_cast<uint32_t>(bytes);
    writeCommand<SessionConfiguration>(Command::ConfigureSession, 0, config);

    auto deviceStatus=readCommand<Status>(Command::GetStatus, 0);
    if (deviceStatus!=Status::Opened)
    {
        std::cerr << "Device not in opened state before benchmark, status " << (int)deviceStatus << std::endl;
        return 0;
    }
    config=readCommand<SessionConfiguration>(Command::GetSessionConfiguration, 0);
    if (config.sampleCount<bytes)
    {
        std::cerr << "Device limited benchmark to " << config.sampleCount << " bytes." << std::endl;
        bytes=config.sampleCount;
    }

    deviceStatus=readCommand<Status>(Command::Start, 0);
    if (deviceStatus!=Status::Running)
    {
        std::cerr << "Device returned status " << (int)deviceStatus << std::endl;
        return 0;
    }

    uint8_t* buffer=new uint8_t[bytes];
    uint8_t* cursor=buffer;
    int result=0;
    while (bytes>0)
    {
        //size_t chunk=std::min(bytes,size_t(1024));

        int transferred=0;
        result=libusb_bulk_transfer(handle, endpoint, cursor, bytes, &transferred, 1000);
        if (result==LIBUSB_ERROR_TIMEOUT)
        {
            if (transferred>0) result=0; // if we made progress, ignore the timeout error and try again
        }
        if (transferred>bytes) throw std::runtime_error("too many bytes returned!");
        if (result!=0) break;
        if (transferred==0) break;  // not sure what happened, but we didn't get an data so we stop instead of risking infinite loop
        cursor+=transferred;
        bytes-=transferred;
    }

    if (result!=0)
    {
        std::cerr << "Transfer ended abnormally with status " << libusb_error_name(result) << std::endl;
    }

    bytes=cursor-buffer;
    for (size_t i=0; i<bytes; ++i)
    {
        if (buffer[i]!=uint8_t(i%251))
        {
            std::cerr << "Data integrity error at location " << i << std::endl;
            break;
        }
    }

    deviceStatus=readCommand<Status>(Command::Stop, 0);
    if (deviceStatus!=Status::Opened)
    {
        std::cerr << "Device returned status " << (int)deviceStatus << std::endl;
    }


    delete [] buffer;
    return bytes;
}

std::vector<uint8_t> Device::sample(size_t samples) const
{
    std::vector<uint8_t> buffer;

    if (!opened) return buffer;

    SessionConfiguration config;
    config.type=SessionType::SingleBit;
    config.sampleCount=static_cast<uint32_t>(samples);
    writeCommand<SessionConfiguration>(Command::ConfigureSession, 0, config);

    auto deviceStatus=readCommand<Status>(Command::GetStatus, 0);
    if (deviceStatus!=Status::Opened)
    {
        std::cerr << "Device not in opened state before benchmark, status " << (int)deviceStatus << std::endl;
        return buffer;
    }
    config=readCommand<SessionConfiguration>(Command::GetSessionConfiguration, 0);
    if (config.sampleCount<samples)
    {
        std::cerr << "Device limited sampling to " << config.sampleCount << " samples." << std::endl;
    }

    deviceStatus=readCommand<Status>(Command::Start, 0);
    if (deviceStatus!=Status::Running)
    {
        std::cerr << "Device returned status " << (int)deviceStatus << std::endl;
        return buffer;
    }

    size_t bytesLeft=config.bytesLeft;
    buffer.resize(bytesLeft);
    uint8_t* cursor=buffer.data();
    int result=0;
    while (bytesLeft>0)
    {
        int transferred=0;
        result=libusb_bulk_transfer(handle, endpoint, cursor, bytesLeft, &transferred, 1000);
        if (result==LIBUSB_ERROR_TIMEOUT)
        {
            if (transferred>0) result=0; // if we made progress, ignore the timeout error and try again
        }
        if (transferred>bytesLeft) throw std::runtime_error("too many bytes returned!");
        if (result!=0) break;
        if (transferred==0) break;  // not sure what happened, but we didn't get an data so we stop instead of risking infinite loop
        cursor+=transferred;
        bytesLeft-=transferred;
    }

    if (result!=0)
    {
        std::cerr << "Transfer ended abnormally with status " << libusb_error_name(result) << std::endl;
    }

    if (cursor - buffer.data() != buffer.size())
        buffer.resize(cursor - buffer.data());
 
    deviceStatus=readCommand<Status>(Command::Stop, 0);
    if (deviceStatus!=Status::Opened)
    {
        std::cerr << "Device returned status " << (int)deviceStatus << std::endl;
    }

    return buffer;  
}
