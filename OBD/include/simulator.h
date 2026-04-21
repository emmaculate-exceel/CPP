#pragma once
#include "obd2.h"
#include <string>
#include <vector>

// Simulates an ELM327 adapter + vehicle ECU
class VehicleSimulator {
public:
    VehicleSimulator();

    // Send an OBD-II command, get raw hex response back
    std::string sendCommand(const std::string& command);

    // Inject a DTC into the simulated ECU
    void addDTC(const std::string& code);
    void clearDTCs();

private:
    std::vector<std::string> activeDTCs;

    std::string handleLiveData(int pid);
    std::string handleFreezeFrame(int pid);
    std::string handleReadDTCs();
    std::string handleVehicleInfo(int pid);
};
