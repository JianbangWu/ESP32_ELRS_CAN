#include "uart_redirect.hpp"
#include <cstdint>

void redirectStdoutToUsbSerial(void)
{
    static UsbSerialStreamBuf usbSerialStreamBuf; // 创建 UsbSerialStreamBuf 实例
    std::cout.rdbuf(&usbSerialStreamBuf);         // 将 std::cout 的缓冲区设置为 UsbSerialStreamBuf
}

UsbSerialStreamBuf::UsbSerialStreamBuf()
{
    usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    setp(buffer, buffer + BUFFER_SIZE - 1); // 设置缓冲区
}

UsbSerialStreamBuf::int_type UsbSerialStreamBuf::overflow(int_type ch)
{
    if (ch != traits_type::eof())
    {
        *pptr() = static_cast<char>(ch); // 将字符放入缓冲区
        pbump(1);                        // 移动缓冲区指针
        sync();                          // 刷新缓冲区
    }
    return ch;
}

int UsbSerialStreamBuf::sync()
{
    if (pbase() != pptr())
    {                                                                                     // 如果缓冲区不为空
        *pptr() = '\0';                                                                   // 添加字符串结束符
        usb_serial_jtag_write_bytes((uint8_t *)pbase(), pptr() - pbase(), portMAX_DELAY); // 写入 USB Serial/JTAG
        setp(buffer, buffer + BUFFER_SIZE - 1);                                           // 重置缓冲区
    }
    return 0;
}