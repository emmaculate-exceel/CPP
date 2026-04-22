#pragma once
#include <string>

class OBDInterface {
public:
    virtual ~OBDInterface() = default;
    virtual std::string sendCommand(const std::string& cmd) = 0;
    virtual bool isConnected() const = 0;
    virtual void injectDTC(const std::string&) {}  // no-op on real hardware
};
