// Implement serial_io for POSIX.
// Specifically to support a 110 baud ASR 33 teletype.
// (Tested on macOS 14.2.1 Sonoma on iMac M1 only.)


#include "serial_io.h"

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sstream>
#include <cctype>


class serial_io::implementation {
public:
    implementation()
    {}

    ~implementation()
    {
        if (fd_ != -1) {
            ::tcflush(fd_, TCIOFLUSH);
            ::close(fd_);
        }
    }

    bool open(
        const std::string & device_name,
        const std::string & /*device_configuration*/)
    {
        // The following is mostly based on https://en.wikibooks.org/wiki/Serial_Programming/termios

        if (fd_ != -1) {
            ::tcflush(fd_, TCIOFLUSH);
            ::close(fd_);
        }

        fd_ = ::open(device_name.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd_ == -1) {
            last_error_text_ = format_error_message("Device open failed", device_name);
            return false;
        }

        if (!::isatty(fd_)) {
            last_error_text_ = format_error_message("Device is not a TTY", device_name);
            return false;
        }

        if (::tcflush(fd_, TCIOFLUSH) < 0) {
            last_error_text_ = format_error_message("Device flush failed", device_name);
            return false;
        }

        struct termios config;
        if (::tcgetattr(fd_, &config) < 0) {
            last_error_text_ = format_error_message("Get terminal attributes failed", device_name);
            return false;
        }

        //
        // Input flags - Turn off input processing
        //
        // convert break to null byte, no CR to NL translation,
        // no NL to CR translation, don't mark parity errors or breaks
        // no input parity check, don't strip high bit off,
        // no XON/XOFF software flow control
        //
        config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                            INLCR | PARMRK | INPCK | ISTRIP | IXON);

        //
        // Output flags - Turn off output processing
        //
        // no CR to NL translation, no NL to CR-NL translation,
        // no NL to CR translation, no column 0 CR suppression,
        // no Ctrl-D suppression, no fill characters, no case mapping,
        // no local output processing
        //
        // config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
        //                     ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
        config.c_oflag = 0;

        //
        // Line processing
        //
        // echo on, echo newline on, canonical mode off,
        // extended input processing off, signal chars off
        //
        config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
        config.c_lflag |=  (ECHO | ECHONL);

        //
        // Turn off character processing
        //
        // clear current char size mask, no parity checking,
        // no output processing, force 8 bit input
        //
        config.c_cflag &= ~(CSIZE | PARENB);
        config.c_cflag |= CS8;

        //
        // One input byte is enough to return from read()
        // Inter-character timer off
        //
        config.c_cc[VMIN]  = 1;
        config.c_cc[VTIME] = 0;

        if (::cfsetispeed(&config, B110) < 0) {
            last_error_text_ = format_error_message("Set input speed failed", device_name);
            return false;
        }

        if (::cfsetospeed(&config, B110) < 0) {
            last_error_text_ = format_error_message("Set output speed failed", device_name);
            return false;
        }

        if (::tcsetattr(fd_, TCSANOW, &config) < 0) {
            last_error_text_ = format_error_message("Set terminal attributes failed", device_name);
            return false;
        }

        return true;
    }

    std::string getline()
    {
        std::string line;

        for (;;) {
            unsigned char ch;
            if (::read(fd_, &ch, 1) == 1) {
                ch &= 0x7F;
                if (ch == '\r')
                    break;
                line += ch;
                if (std::isprint(ch))
                    ++column_;
                if (column_ > column_limit_) {
                    // break lines at column_limit_
                   ::write(fd_, newline_, sizeof(newline_));
                   column_ = 1;
                }
            }
            ::usleep(100000);
        }
        ::write(fd_, newline_, sizeof(newline_));
        column_ = 1;

        return line;
    }

    void write(const std::string & data)
    {
        unsigned remaining = data.length();
        if (remaining) {
            const char * p = data.c_str();

            while (remaining) {
                const unsigned char ch = std::toupper(*p++ & 0x7F);
                if (std::isprint(ch) && column_ > column_limit_) {
                    // break lines at column_limit_
                    ::write(fd_, newline_, sizeof(newline_));
                    column_ = 1;
                }
                ::write(fd_, &ch, 1);
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
    int fd_ = -1;
    std::string last_error_text_;
    unsigned column_ = 1;
    const unsigned column_limit_ = 72; // ASR 33 last column
    const char newline_[4] = {'\r', '\n', '\0', '\0'};

    std::string format_error_message(const std::string & msg, const std::string & value)
    {
        std::ostringstream oss;
        oss << msg << " '" << value << "' Error " << errno << " (" << ::strerror(errno) << ")";
        return oss.str();
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

