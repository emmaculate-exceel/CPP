#pragma once
#include "obd2.h"
#include <cstdint>
#include <string>
#include <vector>

// Decodes raw OBD-II hex responses into human-readable values
class OBDDecoder {
public:
    SensorData decodeLiveData(int pid, const std::string& rawResponse);
    std::vector<DTC> decodeDTCs(const std::string& rawResponse);
    FreezeFrame decodeFreezeFrame(const std::string& rawResponse);
    std::string decodeVIN(const std::string& rawResponse);

private:
    std::string dtcBytesToCode(uint8_t a, uint8_t b);
};
