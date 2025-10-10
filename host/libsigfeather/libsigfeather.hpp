//!@author mucki (coding@mucki.dev)
//!@copyright Copyright (c) 2025
//! please see LICENSE file in root folder for licensing terms.
#pragma once

#include "libsigfeather.h"
#include <stdexcept>

//! @brief C++ RAII interface for libsigfeather
class SigFeather
{
public:
    SigFeather()
    {
        int result=lsf_init();
        if (result)
            throw std::runtime_error("Failed to initialize libsigfeather");
    }

    ~SigFeather()
    {
        lsf_deinit();
    }

    template<typename Callback>
    inline void constexpr enumerate(Callback callback) const
    {
        lsf_enumerate_devices([](const char* v,void* c){std::cout << v << std::endl;}, nullptr);
    }
};