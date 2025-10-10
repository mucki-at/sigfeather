#include <iostream>
#include "libsigfeather.hpp"

int main(int, char**)
{
    SigFeather sf;

    sf.enumerate([](const char* device_path) {
        std::cout << "Found device: " << device_path << std::endl;
    });
    
    return 0;
}
