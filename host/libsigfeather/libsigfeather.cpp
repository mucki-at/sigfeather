//!@author mucki (code@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.

#include "libsigfeather.h"
#include "devicemanager.h"
#include <mutex>
#include <memory>
#include <iostream>
#include <system_error>

std::mutex initMutex;
size_t initCount = 0;
std::unique_ptr<DeviceManager> deviceManager=nullptr;

#ifdef __cplusplus
extern "C"
{
#endif

//! @brief Initialize the libsigfeather library.
//! @return library version on success, 0 on failure.
int lsf_init()
try
{
    std::scoped_lock lock(initMutex);

    if (initCount == 0)
    {
        deviceManager = std::make_unique<DeviceManager>();
    }
    ++initCount;
    return 0;
} catch (std::system_error& se)
{
    std::cerr << "System error in lsf_init: " << se.what() << std::endl;
    return se.code().value();
}  catch (std::exception& e)
{
    std::cerr << "Exception in lsf_init: " << e.what() << std::endl;
    return -1;
} catch (...)
{
    std::cerr << "Unexpected exception in lsf_init." << std::endl;
    return -1;
}

//! @brief Deinitialize the libsigfeather library.
void lsf_deinit()
try
{
    std::scoped_lock lock(initMutex);
    if (initCount > 0)
    {
        --initCount;
        if (initCount == 0)
        {
            deviceManager.reset();
        }
    }   
} catch (std::exception& e)
{
    std::cerr << "Exception in lsf_init: " << e.what() << std::endl;
} catch (...)
{
    std::cerr << "Unexpected exception in lsf_init." << std::endl;
}


int lsf_enumerate_devices(lsf_device_found_callback callback, void* user_data)
try
{
    std::scoped_lock lock(initMutex);
    if (!deviceManager)
        return std::make_error_code(std::errc::not_supported).value();

    return deviceManager->findUsbDevices([callback, user_data](const char* device_path) {
        callback(device_path, user_data);
    });
} catch (std::system_error& se)
{
    std::cerr << "System error in lsf_init: " << se.what() << std::endl;
    return se.code().value();
}  catch (std::exception& e)
{
    std::cerr << "Exception in lsf_init: " << e.what() << std::endl;
    return -1;
} catch (...)
{
    std::cerr << "Unexpected exception in lsf_init." << std::endl;
    return -1;
}

#ifdef __cplusplus
}
#endif


