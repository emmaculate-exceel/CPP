#pragma once
#include "simulator.h"
#include "decoder.h"
#include <string>

class DiagnosticTool {
public:
    DiagnosticTool();

    void run();               // main CLI loop

private:
    VehicleSimulator sim;
    OBDDecoder decoder;

    void showMenu();
    void readLiveData();
    void readDTCs();
    void clearDTCs();
    void showVehicleInfo();
    void showFreezeFrame();
    void injectFaultCode();   // for simulation testing
};
