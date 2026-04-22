#pragma once
#include <string>
#include <vector>
#include <map>

enum class OBDService {
    LIVE_DATA       = 0x01,
    FREEZE_FRAME    = 0x02,
    READ_DTCS       = 0x03,
    CLEAR_DTCS      = 0x04,
    VEHICLE_INFO    = 0x09
};

enum class PID {
    ENGINE_RPM          = 0x0C,
    VEHICLE_SPEED       = 0x0D,
    COOLANT_TEMP        = 0x05,
    THROTTLE_POSITION   = 0x11,
    ENGINE_LOAD         = 0x04,
    FUEL_PRESSURE       = 0x0A,
    INTAKE_AIR_TEMP     = 0x0F,
    MAF_SENSOR          = 0x10,
    BATTERY_VOLTAGE     = 0x42,
    FUEL_LEVEL          = 0x2F,
    BARO_PRESSURE       = 0x33,
    SHORT_FUEL_TRIM_B1  = 0x06,
    LONG_FUEL_TRIM_B1   = 0x07,
    O2_VOLTAGE_B1S1     = 0x14,
    VIN                 = 0x02
};

enum class Severity {
    INFO,
    WARNING,
    CRITICAL
};

struct SensorData {
    std::string name;
    double      value;
    std::string unit;
};

struct DTC {
    std::string code;
    std::string description;
    Severity    severity;
};

struct FreezeFrame {
    std::string          dtc;
    std::vector<SensorData> sensors;
};
