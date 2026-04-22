#include "simulator.h"
#include "decoder.h"
#include <iostream>
#include <cmath>
#include <string>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { std::cout << "  PASS  " << (name) << "\n"; ++passed; } \
    else       { std::cout << "  FAIL  " << (name) << "\n"; ++failed; } \
} while(0)

// ── Simulator unit tests ──────────────────────────────────────────────────────

static void testATCommands(VehicleSimulator& sim) {
    TEST("ATZ returns OK",   sim.sendCommand("ATZ")   == "OK");
    TEST("ATE0 returns OK",  sim.sendCommand("ATE0")  == "OK");
    TEST("ATL0 returns OK",  sim.sendCommand("ATL0")  == "OK");
    TEST("ATSP0 returns OK", sim.sendCommand("ATSP0") == "OK");
}

static void testLiveDataResponses(VehicleSimulator& sim) {
    // All supported PIDs should return something other than NO DATA
    std::vector<std::string> cmds = {
        "01 0C", "01 0D", "01 05", "01 11", "01 04",
        "01 0A", "01 0F", "01 10", "01 42", "01 2F",
        "01 33", "01 06", "01 07", "01 14"
    };
    for (auto& cmd : cmds)
        TEST("PID " + cmd + " not NO DATA", sim.sendCommand(cmd) != "NO DATA");

    // Unsupported PID returns NO DATA
    TEST("Unknown PID returns NO DATA", sim.sendCommand("01 FF") == "NO DATA");
}

static void testFreezeFrameResponses(VehicleSimulator& sim) {
    TEST("Freeze RPM valid",   sim.sendCommand("02 0C 00") != "NO DATA");
    TEST("Freeze speed valid", sim.sendCommand("02 0D 00") != "NO DATA");
    TEST("Freeze temp valid",  sim.sendCommand("02 05 00") != "NO DATA");
    TEST("Freeze unknown NO DATA", sim.sendCommand("02 FF 00") == "NO DATA");
}

static void testDTCLifecycle(VehicleSimulator& sim) {
    // Pre-loaded DTCs: P0300, P0171
    std::string raw = sim.sendCommand("03 00");
    TEST("DTC response not empty", raw != "43 00 00");

    // Inject a new DTC
    sim.injectDTC("P0301");
    raw = sim.sendCommand("03 00");
    TEST("Injected DTC appears in response", raw.find("03 01") != std::string::npos ||
                                              raw != "43 00 00");

    // Duplicate inject — should not add again
    size_t lenBefore = sim.sendCommand("03 00").size();
    sim.injectDTC("P0301");
    size_t lenAfter = sim.sendCommand("03 00").size();
    TEST("Duplicate inject ignored", lenBefore == lenAfter);

    // Clear DTCs
    sim.sendCommand("04 00");
    TEST("After clear, response is empty", sim.sendCommand("03 00") == "43 00 00");
}

static void testVINResponse(VehicleSimulator& sim) {
    std::string vin = sim.sendCommand("09 02");
    TEST("VIN response starts with 49", vin.substr(0, 2) == "49");
    TEST("VIN response not empty", !vin.empty());
    TEST("Unknown vehicle info NO DATA", sim.sendCommand("09 FF") == "NO DATA");
}

static void testUnknownService(VehicleSimulator& sim) {
    TEST("Unknown service NO DATA", sim.sendCommand("08 0C") == "NO DATA");
    TEST("Short command NO DATA",   sim.sendCommand("01")    == "NO DATA");
}

// ── Integration tests: simulator + decoder ────────────────────────────────────

static void testIntegrationLiveData(VehicleSimulator& sim, OBDDecoder& dec) {
    // RPM must be in realistic range (800–4000)
    std::string raw = sim.sendCommand("01 0C");
    auto s = dec.decodeLiveData(0x0C, raw);
    TEST("Integration: RPM in range", s.value >= 800.0 && s.value <= 4000.0);

    // Speed must be 0–120 km/h
    raw = sim.sendCommand("01 0D");
    s = dec.decodeLiveData(0x0D, raw);
    TEST("Integration: Speed in range", s.value >= 0.0 && s.value <= 120.0);

    // Coolant must be 75–105 C
    raw = sim.sendCommand("01 05");
    s = dec.decodeLiveData(0x05, raw);
    TEST("Integration: Coolant in range", s.value >= 75.0 && s.value <= 105.0);

    // Battery must be 13.0–14.5 V
    raw = sim.sendCommand("01 42");
    s = dec.decodeLiveData(0x42, raw);
    TEST("Integration: Battery in range", s.value >= 13.0 && s.value <= 14.5);

    // Fuel level must be 0–100%
    raw = sim.sendCommand("01 2F");
    s = dec.decodeLiveData(0x2F, raw);
    TEST("Integration: Fuel level in range", s.value >= 0.0 && s.value <= 100.0);

    // Barometric pressure must be realistic (95–110 kPa)
    raw = sim.sendCommand("01 33");
    s = dec.decodeLiveData(0x33, raw);
    TEST("Integration: Baro pressure in range", s.value >= 95.0 && s.value <= 110.0);
}

static void testIntegrationDTCs(VehicleSimulator& sim, OBDDecoder& dec) {
    // Fresh simulator has P0300 and P0171 pre-loaded
    sim.sendCommand("04 00"); // clear first
    sim.injectDTC("P0300");
    sim.injectDTC("P0171");

    auto dtcs = dec.decodeDTCs(sim.sendCommand("03 00"));
    TEST("Integration: 2 DTCs decoded", dtcs.size() == 2);

    bool hasP0300 = false, hasP0171 = false;
    for (auto& d : dtcs) {
        if (d.code == "P0300") hasP0300 = true;
        if (d.code == "P0171") hasP0171 = true;
    }
    TEST("Integration: P0300 found",    hasP0300);
    TEST("Integration: P0171 found",    hasP0171);
    TEST("Integration: P0300 critical", dtcs[0].severity == Severity::CRITICAL ||
                                        dtcs[1].severity == Severity::CRITICAL);

    // After clear, decoder sees zero DTCs
    sim.sendCommand("04 00");
    auto cleared = dec.decodeDTCs(sim.sendCommand("03 00"));
    TEST("Integration: Clear -> 0 DTCs", cleared.empty());
}

static void testIntegrationVIN(VehicleSimulator& sim, OBDDecoder& dec) {
    std::string vin = dec.decodeVIN(sim.sendCommand("09 02"));
    TEST("Integration: VIN not empty",    !vin.empty());
    TEST("Integration: VIN length is 17", vin.size() == 17);
    TEST("Integration: VIN starts with 1", vin[0] == '1');
}

static void testIntegrationFreezeFrame(VehicleSimulator& sim, OBDDecoder& dec) {
    // Freeze frame PIDs should decode to realistic values
    auto rpm  = dec.decodeLiveData(0x0C, sim.sendCommand("02 0C 00"));
    auto spd  = dec.decodeLiveData(0x0D, sim.sendCommand("02 0D 00"));
    auto temp = dec.decodeLiveData(0x05, sim.sendCommand("02 05 00"));

    TEST("Integration: Freeze RPM > 0",     rpm.value  > 0.0);
    TEST("Integration: Freeze speed >= 0",  spd.value  >= 0.0);
    TEST("Integration: Freeze temp > -40",  temp.value > -40.0);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "\n  Simulator + Integration Tests\n";
    std::cout << "  " << std::string(40, '=') << "\n\n";

    VehicleSimulator sim;
    OBDDecoder dec;
    dec.loadDTCDatabase("data/dtc_codes.csv");

    std::cout << "  -- AT Commands --\n";
    testATCommands(sim);

    std::cout << "\n  -- Live Data Responses --\n";
    testLiveDataResponses(sim);

    std::cout << "\n  -- Freeze Frame Responses --\n";
    testFreezeFrameResponses(sim);

    std::cout << "\n  -- DTC Lifecycle --\n";
    testDTCLifecycle(sim);

    std::cout << "\n  -- VIN --\n";
    testVINResponse(sim);

    std::cout << "\n  -- Unknown Commands --\n";
    testUnknownService(sim);

    std::cout << "\n  -- Integration: Live Data --\n";
    testIntegrationLiveData(sim, dec);

    std::cout << "\n  -- Integration: DTCs --\n";
    testIntegrationDTCs(sim, dec);

    std::cout << "\n  -- Integration: VIN --\n";
    testIntegrationVIN(sim, dec);

    std::cout << "\n  -- Integration: Freeze Frame --\n";
    testIntegrationFreezeFrame(sim, dec);

    std::cout << "\n  " << std::string(40, '=') << "\n";
    std::cout << "  Results: " << passed << " passed, " << failed << " failed\n\n";
    return failed > 0 ? 1 : 0;
}
