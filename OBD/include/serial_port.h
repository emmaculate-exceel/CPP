#pragma once
#include "obd_interface.h"
#include <string>

class SerialPort : public OBDInterface {
public:
    SerialPort(const std::string& port, int baudRate = 38400);
    ~SerialPort();

    bool connect();
    void disconnect();

    std::string sendCommand(const std::string& cmd) override;
    bool isConnected() const override { return connected; }

private:
    std::string portPath;
    int         baudRate;
    int         fd;
    bool        connected;
    int         retryCount;

    bool        configure();
    std::string readResponse();
    void        initialize();
    bool        ensureConnected();
};
