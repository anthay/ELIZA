// Implement serial_io for Windows.
// Specifically to support a 110 baud ASR 33 teletype.


#include "serial_io.h"

#include <stdio.h>
#include <conio.h>

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <sstream>


class serial_io::implementation {
public:
    implementation()
        : serial_port_handle_(INVALID_HANDLE_VALUE)
    {}

    ~implementation()
    {
        if (serial_port_handle_ != INVALID_HANDLE_VALUE)
            ::CloseHandle(serial_port_handle_);
    }

    bool open(
        const std::string & device_name,
        const std::string & /*device_configuration*/)
    {
        serial_port_handle_ = ::CreateFileA(
            device_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            NULL);
        if (serial_port_handle_ == INVALID_HANDLE_VALUE) {
            last_error_text_ = format_error_message("Failed to open serial device", device_name);
            return false;
        }

        const std::string device_configuration{ "baud=110 parity=n data=8 stop=2" };
        if (!device_configuration.empty()) {
            DCB port;
            memset(&port, 0, sizeof(port));
            port.DCBlength = sizeof(port);
            if (!::GetCommState(serial_port_handle_, &port)) {
                last_error_text_ = format_error_message("GetCommState() failed for device", device_name);
                return false;
            }
            if (!::BuildCommDCBA(device_configuration.c_str(), &port)) {
                last_error_text_ = format_error_message("BuildCommDCBA() failed for device", device_name);
                return false;
            }
            if (!::SetCommState(serial_port_handle_, &port)) {
                last_error_text_ = format_error_message("SetCommState() failed for device", device_name);
                return false;
            }

            COMMTIMEOUTS timeouts;
            timeouts.ReadIntervalTimeout = 1;
            timeouts.ReadTotalTimeoutMultiplier = 1;
            timeouts.ReadTotalTimeoutConstant = 1;
            timeouts.WriteTotalTimeoutMultiplier = 1;
            timeouts.WriteTotalTimeoutConstant = 10000;
            if (!::SetCommTimeouts(serial_port_handle_, &timeouts)) {
                last_error_text_ = format_error_message("SetCommTimeouts() failed for device", device_name);
                return false;
            }

            if (!::EscapeCommFunction(serial_port_handle_, CLRDTR)) {
                last_error_text_ = format_error_message("EscapeCommFunction(CLRDTR) failed for device", device_name);
                return false;
            }
            ::Sleep(200);
            if (!::EscapeCommFunction(serial_port_handle_, SETDTR)) {
                last_error_text_ = format_error_message("EscapeCommFunction(SETDTR) failed for device", device_name);
                return false;
            }
        }

        return true;
    }

    std::string getline()
    {
        std::string line;

        for (;;) {
            unsigned char ch;
            if (getch(ch)) {
                ch &= 0x7F;
                if (ch == '\r')
                    break;
                line += ch;
                if (std::isprint(ch))
                    ++column_;
                if (column_ > column_limit_) {
                    // break lines at column_limit_
                    for (auto c : newline_)
                        putch(c);
                    column_ = 1;
                }
            }
            ::Sleep(100);
        }
        for (auto c : newline_)
            putch(c);
        column_ = 1;

        return line;
    }

    void write(const std::string & data)
    {
        unsigned remaining = static_cast<unsigned>(data.length());
        if (remaining) {
            const char * p = data.c_str();

            while (remaining) {
                const unsigned char ch = static_cast<unsigned char>(
                    std::toupper(*p++ & 0x7F));
                if (std::isprint(ch) && column_ > column_limit_) {
                    // break lines at column_limit_
                    for (auto c : newline_)
                        putch(c);
                    column_ = 1;
                }
                putch(ch);
                --remaining;
                if (ch == '\r')
                    column_ = 1;
                else if (std::isprint(ch))
                    ++column_;
            }
        }
    }

    std::string last_error_text() const
    {
        return last_error_text_;
    }

private:
    HANDLE serial_port_handle_;
    std::string last_error_text_;
    unsigned column_ = 1;
    const unsigned column_limit_ = 72; // ASR 33 last column
    const char newline_[4] = {'\r', '\n', '\0', '\0'};

    std::string get_last_windows_error_message() {
        std::ostringstream oss;
        const DWORD last_error = ::GetLastError();
        oss << "Windows error " << last_error;

        char * ptr = NULL;
        ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM,
            0,
            last_error,
            0,
            (char*)&ptr,
            1024,
            NULL);

        if (ptr) {
            const size_t len = strlen(ptr);
            if (len > 1 && ptr[len-2] == '\r' && ptr[len-1] == '\n')
                ptr[len-2] = '\0';
            oss << " (" << ptr << ")";
            ::LocalFree(ptr);
        }
        return oss.str();
    }

    std::string format_error_message(const std::string & msg)
    {
        std::ostringstream oss;
        oss << msg << " " << get_last_windows_error_message();
        return oss.str();
    }

    std::string format_error_message(const std::string & msg, const std::string & value)
    {
        std::ostringstream oss;
        oss << msg << " '" << value << "' " << get_last_windows_error_message();
        return oss.str();
    }

    bool putch(char data)
    {
        DWORD written = 0;
        OVERLAPPED o = {0};
        o.hEvent = ::CreateEventA(NULL, FALSE, FALSE, NULL);
        if (o.hEvent == NULL)
            throw std::runtime_error(
                format_error_message("CreateEventA() failed"));

        ::WriteFile(serial_port_handle_, &data, 1, &written, &o);

        bool success = false;
        const DWORD last_error_code = ::GetLastError();
        if (last_error_code == ERROR_SUCCESS)
            success = true;
        else if (last_error_code == ERROR_IO_PENDING)
            if (::WaitForSingleObject(o.hEvent, INFINITE) == WAIT_OBJECT_0)
                if (::GetOverlappedResult(serial_port_handle_, &o, &written, FALSE))
                    success = true;
        ::CloseHandle(o.hEvent);
        return success && written == 1;
    }

    bool getch(unsigned char & ch)
    {
        bool success = false;
        OVERLAPPED o = {0};

        o.hEvent = ::CreateEventA(NULL, FALSE, FALSE, NULL);
        if (o.hEvent == NULL)
            throw std::runtime_error(
                format_error_message("CreateEventA() failed"));

        DWORD bytes_read = 0;
        ReadFile(serial_port_handle_, &ch, 1, &bytes_read, &o);
        const DWORD last_error_code = ::GetLastError();
        if (last_error_code == ERROR_SUCCESS)
            success = true;
        else if (last_error_code == ERROR_IO_PENDING) {
            if (WaitForSingleObject(o.hEvent, INFINITE) == WAIT_OBJECT_0)
                success = true;
            GetOverlappedResult(serial_port_handle_, &o, &bytes_read, FALSE);
        }
        CloseHandle(o.hEvent);
        return success && bytes_read == 1;
    }

};



// just pass all serial_io calls through to implementation above

serial_io::serial_io()
    : impl_(std::make_unique<implementation>())
{
}

serial_io::~serial_io()
{
}

bool serial_io::open(
    const std::string & device_name,
    const std::string & device_configuration)
{
    return impl_->open(device_name, device_configuration);
}

std::string serial_io::getline()
{
    return impl_->getline();
}

void serial_io::write(const std::string & data)
{
    impl_->write(data);
}

std::string serial_io::last_error_text() const
{
    return impl_->last_error_text();
}


