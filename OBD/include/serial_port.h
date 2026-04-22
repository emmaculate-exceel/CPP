#pragma once
#include "obd_interface.h"
#include <string>
#include <utility>
#include <vector>

class SerialPort : public OBDInterface {
public:
    SerialPort(const std::string& port, int baudRate = 38400);
    ~SerialPort();

    bool connect();
    void disconnect();

    std::string sendCommand(const std::string& cmd) override;
    bool isConnected() const override { return connected; }

    // Scans common ports and returns the first one that responds.
    // Prints progress to stdout. Returns empty string if none found.
    // Returns {port, baudRate} of detected adapter, or {"", 0} if none found
    static std::pair<std::string, int> autoDetect();

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
    bool        probe();   // quick open+send ATI+check, no full init
};
