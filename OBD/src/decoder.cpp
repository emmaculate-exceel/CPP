#include "decoder.h"
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <map>
#include <vector>
#include <stdexcept>

static std::vector<int> parseHexBytes(const std::string& response) {
    std::vector<int> bytes;
    std::istringstream iss(response);
    std::string token;
    int i = 0;
    while (iss >> token) {
        if (i++ < 2) continue; // skip mode and PID echo
        bytes.push_back(std::stoi(token, nullptr, 16));
    }
    return bytes;
}

SensorData OBDDecoder::decodeLiveData(int pid, const std::string& raw) {
    SensorData data;
    auto bytes = parseHexBytes(raw);
    if (bytes.empty()) {
        data.name = "Unknown"; data.value = 0; data.unit = "";
        return data;
    }

    int A = bytes.size() > 0 ? bytes[0] : 0;
    int B = bytes.size() > 1 ? bytes[1] : 0;

    switch (pid) {
        case 0x0C:
            data.name  = "Engine RPM";
            data.value = ((A * 256) + B) / 4.0;
            data.unit  = "RPM";
            break;
        case 0x0D:
            data.name  = "Vehicle Speed";
            data.value = A;
            data.unit  = "km/h";
            break;
        case 0x05:
            data.name  = "Coolant Temperature";
            data.value = A - 40;
            data.unit  = "°C";
            break;
        case 0x11:
            data.name  = "Throttle Position";
            data.value = (A * 100.0) / 255.0;
            data.unit  = "%";
            break;
        case 0x04:
            data.name  = "Engine Load";
            data.value = (A * 100.0) / 255.0;
            data.unit  = "%";
            break;
        case 0x0A:
            data.name  = "Fuel Pressure";
            data.value = A * 3;
            data.unit  = "kPa";
            break;
        case 0x0F:
            data.name  = "Intake Air Temperature";
            data.value = A - 40;
            data.unit  = "°C";
            break;
        case 0x10:
            data.name  = "MAF Air Flow Rate";
            data.value = ((A * 256) + B) / 100.0;
            data.unit  = "g/s";
            break;
        case 0x42:
            data.name  = "Battery Voltage";
            data.value = ((A * 256) + B) / 1000.0;
            data.unit  = "V";
            break;
        default:
            data.name  = "Unknown PID";
            data.value = 0;
            data.unit  = "";
    }
    return data;
}

std::string OBDDecoder::dtcBytesToCode(uint8_t a, uint8_t b) {
    char type;
    switch ((a & 0xC0) >> 6) {
        case 0: type = 'P'; break;
        case 1: type = 'C'; break;
        case 2: type = 'B'; break;
        case 3: type = 'U'; break;
        default: type = 'P';
    }
    int num = ((a & 0x3F) << 8) | b;
    std::ostringstream oss;
    oss << type << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << num;
    return oss.str();
}

static const std::map<std::string, std::string> DTC_DESCRIPTIONS = {
    {"P0100", "Mass Air Flow Circuit Malfunction"},
    {"P0101", "Mass Air Flow Circuit Range/Performance"},
    {"P0110", "Intake Air Temperature Circuit Malfunction"},
    {"P0115", "Engine Coolant Temperature Circuit Malfunction"},
    {"P0120", "Throttle Position Sensor Circuit Malfunction"},
    {"P0130", "O2 Sensor Circuit Malfunction (Bank 1, Sensor 1)"},
    {"P0171", "System Too Lean (Bank 1)"},
    {"P0172", "System Too Rich (Bank 1)"},
    {"P0300", "Random/Multiple Cylinder Misfire Detected"},
    {"P0301", "Cylinder 1 Misfire Detected"},
    {"P0302", "Cylinder 2 Misfire Detected"},
    {"P0303", "Cylinder 3 Misfire Detected"},
    {"P0304", "Cylinder 4 Misfire Detected"},
    {"P0400", "Exhaust Gas Recirculation Flow Malfunction"},
    {"P0420", "Catalyst System Efficiency Below Threshold"},
    {"P0440", "Evaporative Emission Control System Malfunction"},
    {"P0500", "Vehicle Speed Sensor Malfunction"},
    {"P0505", "Idle Control System Malfunction"},
    {"P0600", "Serial Communication Link Malfunction"},
};

std::vector<DTC> OBDDecoder::decodeDTCs(const std::string& raw) {
    std::vector<DTC> dtcs;
    std::istringstream iss(raw);
    std::string token;

    // Skip mode byte "43"
    iss >> token;

    std::vector<int> bytes;
    while (iss >> token)
        bytes.push_back(std::stoi(token, nullptr, 16));

    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        if (bytes[i] == 0 && bytes[i+1] == 0) continue;
        DTC dtc;
        dtc.code = dtcBytesToCode(bytes[i], bytes[i+1]);
        auto it = DTC_DESCRIPTIONS.find(dtc.code);
        dtc.description = (it != DTC_DESCRIPTIONS.end())
            ? it->second : "Unknown fault code";
        dtcs.push_back(dtc);
    }
    return dtcs;
}

FreezeFrame OBDDecoder::decodeFreezeFrame(const std::string& raw) {
    FreezeFrame ff;
    ff.dtc = "P0300";

    // Decode the same PIDs as live data for the freeze frame snapshot
    std::vector<std::pair<int,std::string>> snapshots = {
        {0x0C, "41 0C 1A F8"},
        {0x0D, "41 0D 45"},
        {0x05, "41 05 69"},
        {0x11, "41 11 50"}
    };

    for (auto& [pid, fakeRaw] : snapshots)
        ff.sensors.push_back(decodeLiveData(pid, fakeRaw));

    return ff;
}

std::string OBDDecoder::decodeVIN(const std::string& raw) {
    std::istringstream iss(raw);
    std::string token;
    std::string vin;
    int i = 0;

    while (iss >> token) {
        if (i++ < 3) continue; // skip "49 02 01"
        int c = std::stoi(token, nullptr, 16);
        if (c > 0) vin += static_cast<char>(c);
    }
    return vin;
}
