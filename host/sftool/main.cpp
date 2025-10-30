#include <iostream>
#include "sigfeather.h"
#include <boost/program_options.hpp>
#include <chrono>

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    SigFeather sf;

    po::options_description desc("sftool - SigFeather command line tool");
    desc.add_options()
        ("help,h", "show help message")
        ("list,l", "list connected devices")
        ("serial", po::value<std::string>()->default_value(""), "select device by serial number")
        ("bench,b", po::value<size_t>(), "run benchmark")
        ("sample,s", po::value<size_t>(), "acquire samples")
    ;

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }
    else if (vm.count("list"))
    {
        std::cout << "Connected SigFeather devices:" << std::endl;
        sf.enumerateDevices([](SigFeather::DeviceHandle device, void* user_data)
            {
                std::cout << " - " << device->getSerialNumber() << std::endl;
                return true; // continue enumeration
            }, nullptr);
        return 0;
    }

    std::string serial = vm["serial"].as<std::string>();
    auto device = sf.findDevice(serial);
    if (!device)
    {
        std::cerr << "Error: no device found" << std::endl;
        return 1;
    }
    std::cout << "Using device with serial number: " << device->getSerialNumber() << std::endl;

    try
    {
        device->open();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: failed to open device: " << ex.what() << std::endl;
        return 1;
    }

    if (vm.count("bench"))
    {
        size_t requested=vm["bench"].as<size_t>();
        auto start=std::chrono::high_resolution_clock::now();
        size_t actual=device->benchmark(requested);
        auto end=std::chrono::high_resolution_clock::now();
        double kilobytes=double(actual)/double(1000.0);
        double seconds=std::chrono::duration_cast<std::chrono::duration<double>>(end-start).count();
        std::cout << "transferred " << kilobytes << " kbytes in " << seconds << " seconds" << std::endl;
        std::cout << "effective rate: " << kilobytes/seconds << " kBps" << std::endl;
    }
    else if (vm.count("sample"))
    {
        size_t requested=vm["sample"].as<size_t>();
        auto result=device->sample(requested);
        std::cout << "acquired " << result.size() << " bytes of sample data:" << std::endl;

        for (size_t i=0;i<result.size();++i)
        {
            std::cout << std::hex << static_cast<int>(result[i]) << " ";
            if ((i%16)==15) std::cout << std::endl;
        }
        std::cout << std::dec << std::endl;
    }

    device->close();
    device=nullptr;
    
    return 0;
}
