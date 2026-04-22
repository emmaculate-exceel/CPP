#include "decoder.h"
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <fstream>
#include <map>
#include <vector>

OBDDecoder::OBDDecoder() {
    loadBuiltinDTCs();
}

void OBDDecoder::loadBuiltinDTCs() {
    dtcDatabase = {
        {"P0100", {"Mass Air Flow Circuit Malfunction",                    Severity::WARNING}},
        {"P0115", {"Engine Coolant Temperature Circuit Malfunction",        Severity::WARNING}},
        {"P0117", {"Engine Coolant Temperature Circuit Low Input",          Severity::CRITICAL}},
        {"P0118", {"Engine Coolant Temperature Circuit High Input",         Severity::CRITICAL}},
        {"P0120", {"Throttle Position Sensor Circuit Malfunction",          Severity::WARNING}},
        {"P0130", {"O2 Sensor Circuit Malfunction (Bank 1, Sensor 1)",      Severity::WARNING}},
        {"P0171", {"System Too Lean (Bank 1)",                              Severity::WARNING}},
        {"P0172", {"System Too Rich (Bank 1)",                              Severity::WARNING}},
        {"P0217", {"Engine Coolant Over Temperature Condition",             Severity::CRITICAL}},
        {"P0300", {"Random/Multiple Cylinder Misfire Detected",             Severity::CRITICAL}},
        {"P0301", {"Cylinder 1 Misfire Detected",                           Severity::CRITICAL}},
        {"P0302", {"Cylinder 2 Misfire Detected",                           Severity::CRITICAL}},
        {"P0303", {"Cylinder 3 Misfire Detected",                           Severity::CRITICAL}},
        {"P0304", {"Cylinder 4 Misfire Detected",                           Severity::CRITICAL}},
        {"P0335", {"Crankshaft Position Sensor Circuit Malfunction",        Severity::CRITICAL}},
        {"P0340", {"Camshaft Position Sensor Circuit Malfunction",          Severity::CRITICAL}},
        {"P0400", {"Exhaust Gas Recirculation Flow Malfunction",            Severity::WARNING}},
        {"P0420", {"Catalyst System Efficiency Below Threshold",            Severity::WARNING}},
        {"P0440", {"Evaporative Emission Control System Malfunction",       Severity::WARNING}},
        {"P0442", {"Evaporative Emission System Leak (Small)",              Severity::INFO}},
        {"P0500", {"Vehicle Speed Sensor Malfunction",                      Severity::WARNING}},
        {"P0505", {"Idle Control System Malfunction",                       Severity::WARNING}},
        {"P0520", {"Engine Oil Pressure Sensor Malfunction",                Severity::CRITICAL}},
        {"P0524", {"Engine Oil Pressure Too Low",                           Severity::CRITICAL}},
        {"P0600", {"Serial Communication Link Malfunction",                 Severity::CRITICAL}},
        {"P0700", {"Transmission Control System Malfunction",               Severity::WARNING}},
    };
}

void OBDDecoder::loadDTCDatabase(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open())
        return;

    std::string line;
    int loaded = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string code, sevStr, desc;
        if (!std::getline(ss, code, ','))   continue;
        if (!std::getline(ss, sevStr, ',')) continue;
        if (!std::getline(ss, desc))        continue;

        Severity sev = Severity::INFO;
        if (sevStr == "WARNING")  sev = Severity::WARNING;
        if (sevStr == "CRITICAL") sev = Severity::CRITICAL;

        dtcDatabase[code] = {desc, sev};
        ++loaded;
    }
    if (loaded == 0)
        loadBuiltinDTCs();
}

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
            data.unit  = "C";
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
            data.unit  = "C";
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
        case 0x2F:
            data.name  = "Fuel Level";
            data.value = (A * 100.0) / 255.0;
            data.unit  = "%";
            break;
        case 0x33:
            data.name  = "Barometric Pressure";
            data.value = A;
            data.unit  = "kPa";
            break;
        case 0x06:
            data.name  = "Short Term Fuel Trim B1";
            data.value = ((A - 128) * 100.0) / 128.0;
            data.unit  = "%";
            break;
        case 0x07:
            data.name  = "Long Term Fuel Trim B1";
            data.value = ((A - 128) * 100.0) / 128.0;
            data.unit  = "%";
            break;
        case 0x14:
            data.name  = "O2 Voltage (B1 S1)";
            data.value = A / 200.0;
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

std::vector<DTC> OBDDecoder::decodeDTCs(const std::string& raw) {
    std::vector<DTC> dtcs;
    std::istringstream iss(raw);
    std::string token;

    iss >> token; // skip mode byte "43"

    std::vector<int> bytes;
    while (iss >> token)
        bytes.push_back(std::stoi(token, nullptr, 16));

    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        if (bytes[i] == 0 && bytes[i+1] == 0) continue;
        DTC dtc;
        dtc.code = dtcBytesToCode(static_cast<uint8_t>(bytes[i]),
                                   static_cast<uint8_t>(bytes[i+1]));
        auto it = dtcDatabase.find(dtc.code);
        if (it != dtcDatabase.end()) {
            dtc.description = it->second.first;
            dtc.severity    = it->second.second;
        } else {
            dtc.description = "Unknown fault code";
            dtc.severity    = Severity::INFO;
        }
        dtcs.push_back(dtc);
    }
    return dtcs;
}

std::string OBDDecoder::decodeVIN(const std::string& raw) {
    std::istringstream iss(raw);
    std::string token, vin;
    int i = 0;
    while (iss >> token) {
        if (i++ < 3) continue; // skip "49 02 01"
        int c = std::stoi(token, nullptr, 16);
        if (c > 0) vin += static_cast<char>(c);
    }
    return vin;
}
