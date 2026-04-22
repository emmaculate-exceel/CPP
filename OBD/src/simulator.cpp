#include "simulator.h"
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <random>
#include <algorithm>

static std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

static int randInt(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(rng());
}

VehicleSimulator::VehicleSimulator() {
    activeDTCs.push_back("P0300");
    activeDTCs.push_back("P0171");
}

std::string VehicleSimulator::sendCommand(const std::string& command) {
    if (command == "ATZ" || command == "ATE0" || command == "ATL0" || command == "ATSP0")
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

    switch (pid) {
        case 0x0C: { // RPM: (A*256+B)/4
            int raw = randInt(800, 4000) * 4;
            oss << std::setw(2) << std::setfill('0') << ((raw >> 8) & 0xFF) << " "
                << std::setw(2) << std::setfill('0') << (raw & 0xFF);
            break;
        }
        case 0x0D: // Speed km/h
            oss << std::setw(2) << std::setfill('0') << randInt(0, 120);
            break;
        case 0x05: // Coolant temp: A-40
            oss << std::setw(2) << std::setfill('0') << (randInt(75, 105) + 40);
            break;
        case 0x11: // Throttle: A*100/255
            oss << std::setw(2) << std::setfill('0') << randInt(26, 204);
            break;
        case 0x04: // Engine load: A*100/255
            oss << std::setw(2) << std::setfill('0') << randInt(0, 255);
            break;
        case 0x0A: // Fuel pressure: A*3 kPa
            oss << std::setw(2) << std::setfill('0') << randInt(10, 100);
            break;
        case 0x0F: // Intake air temp: A-40
            oss << std::setw(2) << std::setfill('0') << (randInt(10, 45) + 40);
            break;
        case 0x10: { // MAF: (A*256+B)/100 g/s
            int maf = randInt(100, 2000);
            oss << std::setw(2) << std::setfill('0') << ((maf >> 8) & 0xFF) << " "
                << std::setw(2) << std::setfill('0') << (maf & 0xFF);
            break;
        }
        case 0x42: { // Battery voltage: (A*256+B)/1000 V
            int mv = randInt(13000, 14500);
            oss << std::setw(2) << std::setfill('0') << ((mv >> 8) & 0xFF) << " "
                << std::setw(2) << std::setfill('0') << (mv & 0xFF);
            break;
        }
        case 0x2F: // Fuel level: A*100/255
            oss << std::setw(2) << std::setfill('0') << randInt(50, 200);
            break;
        case 0x33: // Barometric pressure: A kPa
            oss << std::setw(2) << std::setfill('0') << randInt(99, 103);
            break;
        case 0x06: // Short term fuel trim: (A-128)*100/128
            oss << std::setw(2) << std::setfill('0') << randInt(118, 138);
            break;
        case 0x07: // Long term fuel trim
            oss << std::setw(2) << std::setfill('0') << randInt(115, 141);
            break;
        case 0x14: { // O2 sensor: A/200 V
            oss << std::setw(2) << std::setfill('0') << randInt(40, 200) << " FF";
            break;
        }
        default:
            return "NO DATA";
    }
    return oss.str();
}

std::string VehicleSimulator::handleFreezeFrame(int pid) {
    std::ostringstream oss;
    oss << "42 " << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << pid << " ";
    switch (pid) {
        case 0x0C: oss << "1A F8"; break;  // 1726 RPM
        case 0x0D: oss << "45";    break;  // 69 km/h
        case 0x05: oss << "69";    break;  // 65 °C
        case 0x11: oss << "50";    break;  // ~31% throttle
        case 0x04: oss << "80";    break;  // ~50% load
        case 0x2F: oss << "7F";    break;  // ~50% fuel
        default:   return "NO DATA";
    }
    return oss.str();
}

std::string VehicleSimulator::handleReadDTCs() {
    if (activeDTCs.empty())
        return "43 00 00";

    std::ostringstream oss;
    oss << "43";

    for (const auto& code : activeDTCs) {
        char type = code[0];
        int num   = std::stoi(code.substr(1), nullptr, 16);
        int high  = (num >> 8) & 0x3F;
        int b     = num & 0xFF;
        int a     = 0;

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
    if (pid == 0x02)
        // 17-char VIN: 1G1JC5444R7221234
        return "49 02 01 31 47 31 4A 43 35 34 34 34 52 37 32 32 31 32 33 34";
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
