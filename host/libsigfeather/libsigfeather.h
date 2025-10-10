//!@author mucki (code@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//! @brief Initialize the libsigfeather library.
//! @return library version on success, 0 on failure.
int lsf_init();

//! @brief Deinitialize the libsigfeather library.
void lsf_deinit();


using lsf_device_found_callback = void(*)(const char* device_path, void* user_data);
int lsf_enumerate_devices(lsf_device_found_callback callback, void* user_data);

#ifdef __cplusplus
}
#endif
