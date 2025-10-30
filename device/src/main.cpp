#include <pico/status_led.h>
#include <tusb.h>
#include "usbinterface.h"
#include "logging.h"
#include "sampler.h"
#include <memory>

class SigFeather : public IProtocolHandler
{
private:
    static constexpr uint32_t LedColorUsbConnected=PICO_COLORED_STATUS_LED_COLOR_FROM_WRGB(0,128,128,0);
    static constexpr uint32_t LedColorDriverConnected=PICO_COLORED_STATUS_LED_COLOR_FROM_WRGB(0,0,255,0);
    static constexpr uint32_t LedColorSampling=PICO_COLORED_STATUS_LED_COLOR_FROM_WRGB(0,0,0,255);
    static constexpr uint32_t LedColorError=PICO_COLORED_STATUS_LED_COLOR_FROM_WRGB(0,255,0,0);

public:
    enum class State
    {
        NotConnected,
        UsbConnected,
        DriverConnected,
        Sampling,
        Error
    };

public:
    SigFeather() :
        state(State::NotConnected),
        sampleBuffer(nullptr),
        sampleBufferSize(0),
        transferOffset(0),
        currentConfig(),
        sampler(nullptr)
    {
        sampleBufferSize=256*1024; // 256KB buffer
        sampleBuffer=new uint8_t[sampleBufferSize];
        if (!sampleBuffer)
        {
            fatal("Failed to allocate sample buffer");
        }
    }
    virtual ~SigFeather() = default;

    // IProtocolHandler
    Status getStatus() override
    {
        switch (state)
        {
        case State::NotConnected:
        case State::UsbConnected:
            return Status::Closed;
        case State::DriverConnected:
            return Status::Opened;
        case State::Sampling:
            return Status::Running;
        case State::Error:
        default:
            return Status::Error;
        }
    }
    
    Status open() override
    {
        switch (state)
        {
        case State::Error: return Status::Error; // we do not try to recover from error state
        case State::DriverConnected: return Status::Opened; // should not happen, but no change
        case State::Sampling:
        case State::NotConnected:
            fatal("Unexpected open event while in state %d:", state);
            return Status::Error;
        case State::UsbConnected:
            break;
        default:
            fatal("Unknown state in open: %d", state);
            return Status::Error;
        }
        if (openDriver())
        {
            Info("Driver connected");
            colored_status_led_set_on_with_color(LedColorDriverConnected);
            state=State::DriverConnected;
            return Status::Opened;
        }
        else
        {
            fatal("Driver connection failed");
            return Status::Error;
        }
    }

    Status close() override
    {
        switch (state)
        {
        case State::Error: return Status::Error; // we do not try to recover from error state
        case State::UsbConnected: return Status::Closed; // should not happen, but no change
        case State::NotConnected:
            fatal("Unexpected open event while in state %d:", state);
            return Status::Error;
        case State::Sampling:
            if (!stopSampling())
            {
                fatal("Failed to stop sampling while processing close event");
                return Status::Error;
            }
            // fallthrough
         case State::DriverConnected:
            break;
        default:
            fatal("Unknown state in close: %d", state);
            return Status::Error;
        }
        if (closeDriver())
        {
            Info("Driver disconnected");
            colored_status_led_set_on_with_color(LedColorUsbConnected);
            state=State::UsbConnected;
            return Status::Closed;
        }
        else
        {
            fatal("Driver disconnect failed");
            return Status::Error;
        }
    }

    virtual Status start()
    {
        switch (state)
        {
        case State::Error: return Status::Error; // we do not try to recover from error state
        case State::NotConnected:
        case State::UsbConnected:
            fatal("Unexpected open event while in state %d:", state);
            return Status::Error;
        case State::Sampling:
            if (!stopSampling())
            {
                fatal("Failed to stop sampling while processing start event");
                return Status::Error;
            }
            // fallthrough
        case State::DriverConnected:
            break;
        default:
            fatal("Unknown state in start: %d", state);
            return Status::Error;
        }
        if (startSampling())
        {
            Info("Sampling (re)started");
            colored_status_led_set_on_with_color(LedColorSampling);
            state=State::Sampling;
            return Status::Running;
        }
        else
        {
            fatal("Driver disconnect failed");
            return Status::Error;
        }
    }

    virtual Status stop()
    {
        switch (state)
        {
        case State::Error: return Status::Error; // we do not try to recover from error state
        case State::DriverConnected: return Status::Opened; // should not happen, but benign
        case State::NotConnected:
        case State::UsbConnected:
            fatal("Unexpected open event while in state %d:", state);
            return Status::Error;
        case State::Sampling:
            break;
        default:
            fatal("Unknown state in stop: %d", state);
            return Status::Error;
        }
        if (stopSampling())
        {
            Info("Sampling stopped");
            colored_status_led_set_on_with_color(LedColorDriverConnected);
            state=State::DriverConnected;
            return Status::Opened;
        }
        else
        {
            fatal("Driver disconnect failed");
            return Status::Error;
        }
    }

    virtual void configureSession(SessionConfiguration& config)
    {
        switch (config.type)
        {
        case SessionType::Benchmark:
            currentConfig=config;
            if (currentConfig.sampleCount>sampleBufferSize)
            {
                currentConfig.sampleCount=sampleBufferSize;
            }
            for (size_t i=0;i<currentConfig.sampleCount;i++)
            {
                sampleBuffer[i]=static_cast<uint8_t>(i % 251);
            }
            currentConfig.bytesLeft=currentConfig.sampleCount;
            transferOffset=0;
            Info("Configured session: type=Benchmark, sampleCount=%u", config.sampleCount);
            break;
        case SessionType::SingleBit:
        {
            currentConfig=config;
            sampler=std::make_unique<Sampler>(2); // hardcoded pin 2 for now
            if (!sampler->isValid())
            {
                sampler.reset();
                fatal("Failed to initialize sampler for SingleBit session");
                return;
            }
            size_t sampleCount=currentConfig.sampleCount;
            currentConfig.bytesLeft=sampler->prepareSampling(sampleBuffer, sampleBufferSize, sampleCount);
            currentConfig.sampleCount=static_cast<uint32_t>(sampleCount);
            Info("Configured session: type=SingleBit, sampleCount=%u, bytes=%u", currentConfig.sampleCount, currentConfig.bytesLeft);
            return;
        }
        default:
            fatal("Unknown session type requested: %d", static_cast<uint8_t>(config.type));
            return;
        }
    }   

    virtual SessionConfiguration getSessionConfiguration()
    {
       return currentConfig;
    }

    // interface for main()
    inline State getState() const { return state; }

    void usbConnected()
    {
        switch (state)
        {
        case State::Error: return; // we do not try to recover from error state
        case State::UsbConnected: return; // should not happen, but no change
        case State::Sampling:
        case State::DriverConnected:
            fatal("Unexpected USB connected event while in state: %d", state);
            break;
        case State::NotConnected:
            break;
        default:
            fatal("Unknown state in usbConnected: %d", state);
            return;
        }
        Info("USB BUS connected");
        colored_status_led_set_on_with_color(LedColorUsbConnected);
        state=State::UsbConnected;
    }

    void usbDisconnected()
    {
        switch (state)
        {
        case State::Error: return; // we do not try to recover from error state
        case State::NotConnected: return; // should not happen, but no change
        case State::Sampling:
            if (!stopSampling())
            {
                fatal("Failed to stop sampling while entering NotConnected state");
                return;
            }
            // fallthrough
        case State::DriverConnected:
            if (!closeDriver())
            {
                fatal("Failed to close driver while entering NotConnected state");
                state=State::Error;
                return;
            }
            // fallthrough
        case State::UsbConnected:
            break;
        default:
            fatal("Unknown state in usbDisconnected: %d", state);
            return;
        }
        state=State::NotConnected;
        colored_status_led_set_state(false);
        Info("USB BUS disconnected");
    }

    void fatal(const char* message,...)
    {
        va_list args;
        va_start(args, message);
        ErrorV(message,args);
        va_end(args);

        colored_status_led_set_on_with_color(LedColorError);
        state=State::Error;        
    }

    void update()
    {
        if (state==State::Sampling)
        {
            if (currentConfig.bytesLeft==0)
            {
                tud_vendor_n_write_flush(0);
                stop();
                return;
            }

            size_t available=0;
            if (sampler && sampler->isValid())
            {
                // debug
                Sampler* samplerPtr=sampler.get();
                available=samplerPtr->getBytesAvailable();
                if (available<transferOffset)
                {
                    fatal("Sampler reported less available bytes (%u) than already transferred (%u)", available, transferOffset);
                    return;
                }
                available-=transferOffset;
            }
            else if (currentConfig.type==SessionType::Benchmark)
            {
                available=currentConfig.bytesLeft;
            }
            if (available>currentConfig.bytesLeft) available=currentConfig.bytesLeft;
            if (available==0) return;
            uint32_t max=tud_vendor_n_write_available(0);
            if (available>max) available=max;
            if (available==0) return;
 
            tud_vendor_n_write(0, sampleBuffer+transferOffset, available);
            transferOffset+=available;
            currentConfig.bytesLeft-=available;
        }
    }

private:
    State state;
    uint8_t* sampleBuffer;
    size_t sampleBufferSize=0;
    size_t transferOffset=0;
    SessionConfiguration currentConfig{};
    std::unique_ptr<Sampler> sampler;

    bool openDriver()
    {
        // perform all initialization for session
        return true;
    }

    bool closeDriver()
    {
        // cleanup stuff initialized in openDriver
        return true;
    }

    bool startSampling()
    {
        switch (currentConfig.type)
        {
        case SessionType::Benchmark:
            currentConfig.bytesLeft=currentConfig.sampleCount;
            break;
        case SessionType::SingleBit:
            if (!sampler || !sampler->isValid())
            {
                fatal("Sampler not initialized for SingleBit session");
                sampler=nullptr;
                return false;
            }
            if (sampler->isRunning())
            {
                fatal("Sampler already running when starting SingleBit session");
                return false;
            }
            size_t sampleCount=currentConfig.sampleCount;
            sampler->startSampling(sampleBuffer, sampleBufferSize, sampleCount);
            if (sampleCount!=currentConfig.sampleCount)
            {
                fatal("Sampler could not start full sampling session, expected %u samples, got %u samples", currentConfig.sampleCount, sampleCount);
                return false;
            }
            break;
        }
        transferOffset=0;
        return true;
    }

    bool stopSampling()
    {
        // stop data acquisition
        sampler=nullptr;
        return true;
    }
};

SigFeather* globalInstance=nullptr;

int main(void)
{
    status_led_init();
    status_led_set_state(true);
    colored_status_led_set_state(false);
 
    setupLogging();
    Info("--- New Session ---");

    SigFeather sigFeather;
    globalInstance=&sigFeather;
    USBInterface interface(sigFeather);

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(0, &dev_init); // initialize device stack on roothub port 0

    while (true)
    {
        sigFeather.update();
        tud_task();
    }
}


extern "C" {

    
void USB0_IRQHandler(void) {
    tusb_int_handler(0, true);
}


// Invoked when device is mounted
void tud_mount_cb(void)
{
    globalInstance->usbConnected();
}

void tud_umount_cb(void)
{
    globalInstance->usbDisconnected();
}

}


