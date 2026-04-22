#pragma once
#include "obd2.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class OBDDecoder {
public:
    OBDDecoder();

    void loadDTCDatabase(const std::string& csvPath);

    SensorData       decodeLiveData(int pid, const std::string& rawResponse);
    std::vector<DTC> decodeDTCs(const std::string& rawResponse);
    std::string      decodeVIN(const std::string& rawResponse);

private:
    std::map<std::string, std::pair<std::string, Severity>> dtcDatabase;

    std::string dtcBytesToCode(uint8_t a, uint8_t b);
    void        loadBuiltinDTCs();
};
