#include "decoder.h"
#include <iostream>
#include <cmath>
#include <cassert>

static int passed = 0, failed = 0;

static bool near(double a, double b, double eps = 0.05) {
    return std::abs(a - b) < eps;
}

#define TEST(name, expr) do { \
    if (expr) { std::cout << "  PASS  " << (name) << "\n"; ++passed; } \
    else       { std::cout << "  FAIL  " << (name) << "\n"; ++failed; } \
} while(0)

static void testRPM(OBDDecoder& dec) {
    // "41 0C 1A F8": A=0x1A=26, B=0xF8=248 -> (26*256+248)/4 = 1726
    auto s = dec.decodeLiveData(0x0C, "41 0C 1A F8");
    TEST("RPM name",   s.name  == "Engine RPM");
    TEST("RPM value",  near(s.value, 1726.0));
    TEST("RPM unit",   s.unit  == "RPM");

    // Zero RPM edge case: A=0, B=0
    auto z = dec.decodeLiveData(0x0C, "41 0C 00 00");
    TEST("RPM zero",   near(z.value, 0.0));

    // Max RPM: A=FF, B=FF -> (255*256+255)/4 = 16383.75
    auto m = dec.decodeLiveData(0x0C, "41 0C FF FF");
    TEST("RPM max",    near(m.value, 16383.75, 1.0));
}

static void testSpeed(OBDDecoder& dec) {
    // "41 0D 45": A=0x45=69 -> 69 km/h
    auto s = dec.decodeLiveData(0x0D, "41 0D 45");
    TEST("Speed value", near(s.value, 69.0));
    TEST("Speed unit",  s.unit == "km/h");

    // Zero speed
    auto z = dec.decodeLiveData(0x0D, "41 0D 00");
    TEST("Speed zero",  near(z.value, 0.0));
}

static void testCoolant(OBDDecoder& dec) {
    // "41 05 69": A=0x69=105 -> 105-40=65 C
    auto s = dec.decodeLiveData(0x05, "41 05 69");
    TEST("Coolant value", near(s.value, 65.0));
    TEST("Coolant unit",  s.unit == "C");

    // Minimum: A=0 -> -40 C
    auto z = dec.decodeLiveData(0x05, "41 05 00");
    TEST("Coolant min",   near(z.value, -40.0));
}

static void testThrottle(OBDDecoder& dec) {
    // "41 11 50": A=0x50=80 -> 80*100/255 = 31.37%
    auto s = dec.decodeLiveData(0x11, "41 11 50");
    TEST("Throttle value", near(s.value, 31.37, 0.1));
    TEST("Throttle unit",  s.unit == "%");

    // Full throttle: A=FF -> 100%
    auto f = dec.decodeLiveData(0x11, "41 11 FF");
    TEST("Throttle full",  near(f.value, 100.0, 0.5));
}

static void testBattery(OBDDecoder& dec) {
    // "41 42 33 B8": A=0x33=51, B=0xB8=184 -> (51*256+184)/1000 = 13.24 V
    auto s = dec.decodeLiveData(0x42, "41 42 33 B8");
    TEST("Battery value", near(s.value, 13.24));
    TEST("Battery unit",  s.unit == "V");
}

static void testFuelLevel(OBDDecoder& dec) {
    // "41 2F 7F": A=127 -> 127*100/255 = 49.80%
    auto s = dec.decodeLiveData(0x2F, "41 2F 7F");
    TEST("Fuel level value", near(s.value, 49.80, 0.1));
    TEST("Fuel level unit",  s.unit == "%");
}

static void testFuelTrim(OBDDecoder& dec) {
    // STFT zero: A=128 -> (128-128)*100/128 = 0%
    auto z = dec.decodeLiveData(0x06, "41 06 80");
    TEST("STFT zero",     near(z.value, 0.0));

    // STFT positive: A=144 -> (144-128)*100/128 = 12.5%
    auto p = dec.decodeLiveData(0x06, "41 06 90");
    TEST("STFT positive", near(p.value, 12.5, 0.1));

    // STFT negative: A=112 -> (112-128)*100/128 = -12.5%
    auto n = dec.decodeLiveData(0x06, "41 06 70");
    TEST("STFT negative", near(n.value, -12.5, 0.1));
}

static void testO2Voltage(OBDDecoder& dec) {
    // "41 14 64 FF": A=0x64=100 -> 100/200 = 0.5 V
    auto s = dec.decodeLiveData(0x14, "41 14 64 FF");
    TEST("O2 voltage value", near(s.value, 0.5));
    TEST("O2 voltage unit",  s.unit == "V");
}

static void testMAF(OBDDecoder& dec) {
    // "41 10 01 2C": A=1, B=44 -> (256+44)/100 = 3.0 g/s
    auto s = dec.decodeLiveData(0x10, "41 10 01 2C");
    TEST("MAF value", near(s.value, 3.0));
    TEST("MAF unit",  s.unit == "g/s");
}

static void testDTCDecode(OBDDecoder& dec) {
    // "43 03 00": A=0x03, B=0x00 -> P0300
    auto dtcs = dec.decodeDTCs("43 03 00");
    TEST("Single DTC count",    dtcs.size() == 1);
    TEST("DTC code P0300",      !dtcs.empty() && dtcs[0].code == "P0300");
    TEST("DTC severity CRIT",   !dtcs.empty() && dtcs[0].severity == Severity::CRITICAL);

    // "43 01 71": A=0x01, B=0x71 -> P0171
    auto dtcs2 = dec.decodeDTCs("43 01 71");
    TEST("P0171 code",          !dtcs2.empty() && dtcs2[0].code == "P0171");
    TEST("P0171 severity WARN", !dtcs2.empty() && dtcs2[0].severity == Severity::WARNING);

    // Multiple DTCs: "43 03 00 01 71"
    auto dtcs3 = dec.decodeDTCs("43 03 00 01 71");
    TEST("Multi DTC count",     dtcs3.size() == 2);

    // Empty DTCs: "43 00 00"
    auto empty = dec.decodeDTCs("43 00 00");
    TEST("Empty DTCs",          empty.empty());

    // C code: A=0x40, B=0x00 -> C0000
    auto cDtc = dec.decodeDTCs("43 40 00");
    TEST("C-code type",         !cDtc.empty() && cDtc[0].code[0] == 'C');

    // B code: A=0x80, B=0x01 -> B0001
    auto bDtc = dec.decodeDTCs("43 80 01");
    TEST("B-code type",         !bDtc.empty() && bDtc[0].code[0] == 'B');

    // U code: A=0xC0, B=0x01 -> U0001
    auto uDtc = dec.decodeDTCs("43 C0 01");
    TEST("U-code type",         !uDtc.empty() && uDtc[0].code[0] == 'U');
}

static void testVIN(OBDDecoder& dec) {
    auto vin = dec.decodeVIN("49 02 01 31 47 31 4A 43 35 34 34 34 52 37 32 32 31 32 33 34");
    TEST("VIN decode", vin == "1G1JC5444R7221234");
    TEST("VIN length", vin.size() == 17);
}

static void testUnknownPID(OBDDecoder& dec) {
    auto s = dec.decodeLiveData(0xFF, "41 FF 00");
    TEST("Unknown PID name", s.name == "Unknown PID");
    TEST("Unknown PID value", near(s.value, 0.0));
}

int main() {
    std::cout << "\n  OBD Decoder Unit Tests\n";
    std::cout << "  " << std::string(40, '=') << "\n\n";

    OBDDecoder dec;
    dec.loadDTCDatabase("data/dtc_codes.csv");

    testRPM(dec);
    testSpeed(dec);
    testCoolant(dec);
    testThrottle(dec);
    testBattery(dec);
    testFuelLevel(dec);
    testFuelTrim(dec);
    testO2Voltage(dec);
    testMAF(dec);
    testDTCDecode(dec);
    testVIN(dec);
    testUnknownPID(dec);

    std::cout << "\n  " << std::string(40, '=') << "\n";
    std::cout << "  Results: " << passed << " passed, " << failed << " failed\n\n";
    return failed > 0 ? 1 : 0;
}
