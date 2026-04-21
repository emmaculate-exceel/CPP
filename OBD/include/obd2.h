#pragma once
#include <string>
#include <vector>
#include <map>

// OBD-II Service Modes
enum class OBDService {
    LIVE_DATA       = 0x01,
    FREEZE_FRAME    = 0x02,
    READ_DTCS       = 0x03,
    CLEAR_DTCS      = 0x04,
    VEHICLE_INFO    = 0x09
};

// OBD-II PIDs (Parameter IDs) for Service 01
enum class PID {
    ENGINE_RPM          = 0x0C,
    VEHICLE_SPEED       = 0x0D,
    COOLANT_TEMP        = 0x05,
    THROTTLE_POSITION   = 0x11,
    ENGINE_LOAD         = 0x04,
    FUEL_PRESSURE       = 0x0A,
    INTAKE_AIR_TEMP     = 0x0F,
    MAF_SENSOR          = 0x10,
    OXYGEN_SENSOR       = 0x14,
    BATTERY_VOLTAGE     = 0x42,
    VIN                 = 0x02   // used with service 09
};

struct SensorData {
    std::string name;
    double value;
    std::string unit;
};

struct DTC {
    std::string code;       // e.g. "P0300"
    std::string description;
};

struct FreezeFrame {
    std::string dtc;
    std::vector<SensorData> sensors;
};
