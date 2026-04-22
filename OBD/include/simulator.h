#pragma once
#include "obd_interface.h"
#include <string>
#include <vector>

class VehicleSimulator : public OBDInterface {
public:
    VehicleSimulator();

    std::string sendCommand(const std::string& command) override;
    bool isConnected() const override { return true; }
    void injectDTC(const std::string& code) override { addDTC(code); }

    void addDTC(const std::string& code);
    void clearDTCs();

private:
    std::vector<std::string> activeDTCs;

    std::string handleLiveData(int pid);
    std::string handleFreezeFrame(int pid);
    std::string handleReadDTCs();
    std::string handleVehicleInfo(int pid);
};
