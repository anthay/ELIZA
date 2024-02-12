#ifndef SERIAL_IO_H_INCLUDED
#define SERIAL_IO_H_INCLUDED

#include <memory>
#include <string>

class serial_io {
public:
    serial_io();
    ~serial_io();

    bool open(
        const std::string & device_name,
        const std::string & device_configuration);

    std::string getline();

    void write(const std::string & data);

    std::string last_error_text() const;

private:
    class implementation;
    std::unique_ptr<implementation> impl_;
};

#endif
