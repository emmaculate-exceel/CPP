#include "simulator.h"
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <algorithm>

VehicleSimulator::VehicleSimulator() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    // Pre-load a couple of DTCs for demo
    activeDTCs.push_back("P0300");
    activeDTCs.push_back("P0171");
}

std::string VehicleSimulator::sendCommand(const std::string& command) {
    if (command == "ATZ" || command == "ATE0" || command == "ATL0")
        return "OK";

    if (command.size() < 4)
        return "NO DATA";

    int service = std::stoi(command.substr(0, 2), nullptr, 16);
    int pid     = std::stoi(command.substr(3, 2), nullptr, 16);

    switch (service) {
        case 0x01: return handleLiveData(pid);
        case 0x02: return handleFreezeFrame(pid);
        case 0x03: return handleReadDTCs();
        case 0x04: clearDTCs(); return "44";
        case 0x09: return handleVehicleInfo(pid);
        default:   return "NO DATA";
    }
}

std::string VehicleSimulator::handleLiveData(int pid) {
    std::ostringstream oss;
    oss << "41 " << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << pid << " ";

    int r = std::rand();

    switch (pid) {
        case 0x0C: { // RPM: formula = (A*256+B)/4, range 800-4000
            int rpm = 800 + (r % 3200);
            int raw = rpm * 4;
            oss << std::setw(2) << std::setfill('0') << ((raw >> 8) & 0xFF) << " "
                << std::setw(2) << std::setfill('0') << (raw & 0xFF);
            break;
        }
        case 0x0D: // Speed: A km/h, range 0-120
            oss << std::setw(2) << std::setfill('0') << (r % 121);
            break;
        case 0x05: // Coolant temp: A-40, range 75-105°C
            oss << std::setw(2) << std::setfill('0') << (75 + (r % 31) + 40);
            break;
        case 0x11: // Throttle: A*100/255 %, range 10-80
            oss << std::setw(2) << std::setfill('0') << (26 + (r % 178));
            break;
        case 0x04: // Engine load: A*100/255 %
            oss << std::setw(2) << std::setfill('0') << (r % 256);
            break;
        case 0x0A: // Fuel pressure: A*3 kPa
            oss << std::setw(2) << std::setfill('0') << (10 + (r % 90));
            break;
        case 0x0F: // Intake air temp: A-40
            oss << std::setw(2) << std::setfill('0') << (25 + (r % 20) + 40);
            break;
        case 0x10: { // MAF: (A*256+B)/100 g/s
            int maf = 100 + (r % 1900);
            oss << std::setw(2) << std::setfill('0') << ((maf >> 8) & 0xFF) << " "
                << std::setw(2) << std::setfill('0') << (maf & 0xFF);
            break;
        }
        case 0x42: { // Battery voltage: (A*256+B)/1000 V
            int mv = 13000 + (r % 1500);
            oss << std::setw(2) << std::setfill('0') << ((mv >> 8) & 0xFF) << " "
                << std::setw(2) << std::setfill('0') << (mv & 0xFF);
            break;
        }
        default:
            return "NO DATA";
    }
    return oss.str();
}

std::string VehicleSimulator::handleFreezeFrame(int pid) {
    // Freeze frame uses same encoding as live data, but at a fixed snapshot
    std::ostringstream oss;
    oss << "42 " << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << pid << " ";

    switch (pid) {
        case 0x0C: oss << "1A F8"; break; // ~1726 RPM
        case 0x0D: oss << "45";    break; // 69 km/h
        case 0x05: oss << "69";    break; // 65°C
        case 0x11: oss << "50";    break; // ~31% throttle
        default:   return "NO DATA";
    }
    return oss.str();
}

std::string VehicleSimulator::handleReadDTCs() {
    if (activeDTCs.empty())
        return "43 00";

    std::ostringstream oss;
    oss << "43";

    for (const auto& code : activeDTCs) {
        char type = code[0];
        int num  = std::stoi(code.substr(1), nullptr, 16);
        int high = (num >> 8) & 0x3F;
        int b    = num & 0xFF;
        int a    = 0;

        switch (type) {
            case 'P': a = high;        break;
            case 'C': a = high | 0x40; break;
            case 'B': a = high | 0x80; break;
            case 'U': a = high | 0xC0; break;
        }
        oss << " " << std::uppercase << std::hex
            << std::setw(2) << std::setfill('0') << a << " "
            << std::setw(2) << std::setfill('0') << b;
    }
    return oss.str();
}

std::string VehicleSimulator::handleVehicleInfo(int pid) {
    if (pid == 0x02) // VIN
        return "49 02 01 31 47 31 4A 43 35 34 34 34 52 37 32 32 31 32 33 34 35";
    return "NO DATA";
}

void VehicleSimulator::addDTC(const std::string& code) {
    auto it = std::find(activeDTCs.begin(), activeDTCs.end(), code);
    if (it == activeDTCs.end())
        activeDTCs.push_back(code);
}

void VehicleSimulator::clearDTCs() {
    activeDTCs.clear();
}
