
#pragma once

#include <iostream>
#include "driver/usb_serial_jtag.h"

#ifdef __cplusplus
extern "C"
{
#endif

    class UsbSerialStreamBuf : public std::streambuf
    {
    public:
        UsbSerialStreamBuf(); // 构造函数声明

    protected:
        static const size_t BUFFER_SIZE = 256; // 缓冲区大小
        char buffer[BUFFER_SIZE];              // 缓冲区

        usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
            .tx_buffer_size = 256, /* bytes */
            .rx_buffer_size = 256, /* bytes */
        };

        // 重写虚函数声明
        virtual int_type overflow(int_type ch) override;
        virtual int sync() override;
    };

    void redirectStdoutToUsbSerial(void);

#ifdef __cplusplus
}
#endif
