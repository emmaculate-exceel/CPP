#include "diagnostic_tool.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

static void printSeparator() {
    std::cout << std::string(50, '-') << "\n";
}

DiagnosticTool::DiagnosticTool() {}

void DiagnosticTool::run() {
    std::cout << "\n";
    printSeparator();
    std::cout << "  Vehicle Diagnostic Tool v1.0  [SIMULATION MODE]\n";
    printSeparator();

    int choice = 0;
    while (choice != 7) {
        showMenu();
        std::cin >> choice;
        std::cin.ignore();
        std::cout << "\n";

        switch (choice) {
            case 1: readLiveData();    break;
            case 2: readDTCs();        break;
            case 3: clearDTCs();       break;
            case 4: showVehicleInfo(); break;
            case 5: showFreezeFrame(); break;
            case 6: injectFaultCode(); break;
            case 7: std::cout << "Disconnecting... Goodbye.\n\n"; break;
            default: std::cout << "Invalid option. Try again.\n";
        }
    }
}

void DiagnosticTool::showMenu() {
    std::cout << "\n";
    printSeparator();
    std::cout << "  MAIN MENU\n";
    printSeparator();
    std::cout << "  1. Live Sensor Data\n";
    std::cout << "  2. Read Fault Codes (DTCs)\n";
    std::cout << "  3. Clear Fault Codes\n";
    std::cout << "  4. Vehicle Info (VIN)\n";
    std::cout << "  5. Freeze Frame Data\n";
    std::cout << "  6. Inject Test Fault Code [SIM ONLY]\n";
    std::cout << "  7. Exit\n";
    printSeparator();
    std::cout << "  Select option: ";
}

void DiagnosticTool::readLiveData() {
    struct PIDEntry { int pid; std::string cmd; };
    std::vector<PIDEntry> pids = {
        {0x0C, "01 0C"},
        {0x0D, "01 0D"},
        {0x05, "01 05"},
        {0x11, "01 11"},
        {0x04, "01 04"},
        {0x0A, "01 0A"},
        {0x0F, "01 0F"},
        {0x10, "01 10"},
        {0x42, "01 42"}
    };

    std::cout << "  LIVE SENSOR DATA\n";
    printSeparator();

    for (auto& entry : pids) {
        std::string raw = sim.sendCommand(entry.cmd);
        if (raw == "NO DATA") continue;
        SensorData s = decoder.decodeLiveData(entry.pid, raw);
        std::cout << "  " << std::left << std::setw(28) << s.name
                  << std::right << std::setw(10) << std::fixed
                  << std::setprecision(2) << s.value
                  << " " << s.unit << "\n";
    }
    printSeparator();
}

void DiagnosticTool::readDTCs() {
    std::string raw = sim.sendCommand("03 00");
    std::vector<DTC> dtcs = decoder.decodeDTCs(raw);

    std::cout << "  DIAGNOSTIC TROUBLE CODES\n";
    printSeparator();

    if (dtcs.empty()) {
        std::cout << "  No fault codes found. System OK.\n";
    } else {
        std::cout << "  Found " << dtcs.size() << " fault code(s):\n\n";
        for (const auto& d : dtcs)
            std::cout << "  [" << d.code << "]  " << d.description << "\n";
    }
    printSeparator();
}

void DiagnosticTool::clearDTCs() {
    std::cout << "  Sending clear command...\n";
    sim.sendCommand("04 00");
    std::cout << "  Fault codes cleared successfully.\n";
}

void DiagnosticTool::showVehicleInfo() {
    std::string raw = sim.sendCommand("09 02");
    std::string vin = decoder.decodeVIN(raw);

    std::cout << "  VEHICLE INFORMATION\n";
    printSeparator();
    std::cout << "  VIN: " << vin << "\n";
    std::cout << "  Protocol: OBD-II (SAE J1979)\n";
    std::cout << "  Mode: Simulation\n";
    printSeparator();
}

void DiagnosticTool::showFreezeFrame() {
    std::string raw = sim.sendCommand("02 0C");
    FreezeFrame ff = decoder.decodeFreezeFrame(raw);

    std::cout << "  FREEZE FRAME DATA\n";
    printSeparator();
    std::cout << "  Triggered by DTC: " << ff.dtc << "\n\n";

    for (const auto& s : ff.sensors)
        std::cout << "  " << std::left << std::setw(28) << s.name
                  << std::right << std::setw(10) << std::fixed
                  << std::setprecision(2) << s.value
                  << " " << s.unit << "\n";
    printSeparator();
}

void DiagnosticTool::injectFaultCode() {
    std::cout << "  Enter DTC code to inject (e.g. P0301): ";
    std::string code;
    std::getline(std::cin, code);
    sim.addDTC(code);
    std::cout << "  Fault code [" << code << "] injected into simulation.\n";
}
