#include "diagnostic_tool.h"
#include "simulator.h"
#include "serial_port.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <csignal>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <unistd.h>
#include <sys/select.h>

namespace fs = std::filesystem;

// ── TerminalGuard ─────────────────────────────────────────────────────────────

TerminalGuard* TerminalGuard::instance = nullptr;

TerminalGuard::TerminalGuard() {
    tcgetattr(STDIN_FILENO, &saved);
    instance = this;
    prevINT  = std::signal(SIGINT,  TerminalGuard::handle);
    prevTERM = std::signal(SIGTERM, TerminalGuard::handle);
    prevHUP  = std::signal(SIGHUP,  TerminalGuard::handle);

    struct termios raw = saved;
    raw.c_lflag &= ~(static_cast<unsigned>(ECHO) | static_cast<unsigned>(ICANON));
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

TerminalGuard::~TerminalGuard() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
    std::signal(SIGINT,  prevINT);
    std::signal(SIGTERM, prevTERM);
    std::signal(SIGHUP,  prevHUP);
    instance = nullptr;
}

void TerminalGuard::handle(int sig) {
    if (instance)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &instance->saved);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool waitKey(char& c, int timeoutMs) {
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0)
        return read(STDIN_FILENO, &c, 1) == 1;
    return false;
}

static void clearScreen() { std::cout << "\033[2J\033[H" << std::flush; }

static void printSep(char ch = '-', int width = 56) {
    std::cout << "  " << std::string(width, ch) << "\n";
}

static std::string severityLabel(Severity s) {
    switch (s) {
        case Severity::CRITICAL: return "[CRITICAL]";
        case Severity::WARNING:  return "[WARNING] ";
        case Severity::INFO:     return "[INFO]    ";
    }
    return "[INFO]    ";
}

static std::string timestamp() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

static std::string filenameTimestamp() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
    return buf;
}

// Resolve path relative to executable — used for both data/ and reports/
static fs::path exeDir() {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0)
        return fs::path(std::string(buf, len)).parent_path();
    return fs::current_path();
}

static fs::path reportsDir() { return exeDir() / "reports"; }

// ── PIDs ──────────────────────────────────────────────────────────────────────

struct PIDEntry { int pid; std::string cmd; };

static const std::vector<PIDEntry> LIVE_PIDS = {
    {0x0C, "01 0C"}, {0x0D, "01 0D"}, {0x05, "01 05"},
    {0x11, "01 11"}, {0x04, "01 04"}, {0x0A, "01 0A"},
    {0x0F, "01 0F"}, {0x10, "01 10"}, {0x42, "01 42"},
    {0x2F, "01 2F"}, {0x33, "01 33"}, {0x06, "01 06"},
    {0x07, "01 07"}, {0x14, "01 14"}
};

static const std::vector<PIDEntry> FREEZE_PIDS = {
    {0x0C, "02 0C 00"}, {0x0D, "02 0D 00"}, {0x05, "02 05 00"},
    {0x11, "02 11 00"}, {0x04, "02 04 00"}, {0x2F, "02 2F 00"}
};

// ── DiagnosticTool ────────────────────────────────────────────────────────────

DiagnosticTool::DiagnosticTool() : simMode(true) {
    decoder.loadDTCDatabase((exeDir() / "data/dtc_codes.csv").string());
}

void DiagnosticTool::run() {
    clearScreen();
    printSep('=');
    std::cout << "    Vehicle Diagnostic Tool  v2.1\n";
    printSep('=');

    selectMode();

    int choice = 0;
    while (choice != 8) {
        showMenu();
        choice = getMenuChoice(1, 8);
        std::cout << "\n";

        switch (choice) {
            case 1: readLiveData();    break;
            case 2: readDTCs();        break;
            case 3: clearDTCs();       break;
            case 4: showVehicleInfo(); break;
            case 5: showFreezeFrame(); break;
            case 6: injectFaultCode(); break;
            case 7: saveReport();      break;
            case 8: std::cout << "  Disconnecting... Goodbye.\n\n"; break;
        }
    }
}

void DiagnosticTool::selectMode() {
    printSep();
    std::cout << "  SELECT CONNECTION MODE\n";
    printSep();
    std::cout << "  1. Simulation Mode  (no hardware needed)\n";
    std::cout << "  2. Real Hardware    (ELM327 via serial port)\n";
    printSep();
    std::cout << "  Select: ";

    int choice = getMenuChoice(1, 2);
    std::cout << "\n";

    if (choice == 1) {
        simMode = true;
        iface   = std::make_unique<VehicleSimulator>();
        std::cout << "  Connected: Simulation Mode\n";
    } else {
        simMode = false;
        std::cout << "  Enter serial port path [/dev/ttyUSB0]: ";
        std::string port;
        std::getline(std::cin, port);
        if (port.empty()) port = "/dev/ttyUSB0";

        auto sp = std::make_unique<SerialPort>(port);
        std::cout << "  Connecting to " << port << "...\n";
        if (!sp->connect()) {
            std::cout << "  Connection failed. Falling back to simulation.\n";
            simMode = true;
            iface   = std::make_unique<VehicleSimulator>();
        } else {
            std::cout << "  Connected to " << port << "\n";
            iface = std::move(sp);
        }
    }
}

void DiagnosticTool::showMenu() {
    std::cout << "\n";
    printSep();
    std::cout << "  MAIN MENU  [" << (simMode ? "SIMULATION" : "REAL HARDWARE") << "]\n";
    printSep();
    std::cout << "  1. Live Sensor Data         (continuous refresh)\n";
    std::cout << "  2. Read Fault Codes         (DTCs with severity)\n";
    std::cout << "  3. Clear Fault Codes\n";
    std::cout << "  4. Vehicle Info             (VIN)\n";
    std::cout << "  5. Freeze Frame Data\n";
    std::cout << "  6. Inject Test Fault Code   [SIM ONLY]\n";
    std::cout << "  7. Save Diagnostic Report\n";
    std::cout << "  8. Exit\n";
    printSep();
    std::cout << "  Select option: ";
}

// ── Live data ─────────────────────────────────────────────────────────────────

static void printSensorRow(const SensorData& s) {
    std::cout << "  " << std::left  << std::setw(30) << s.name
              << std::right << std::setw(10) << std::fixed
              << std::setprecision(2) << s.value
              << "  " << s.unit << "\n";
}

void DiagnosticTool::readLiveData() {
    std::cout << "  Press Enter to start live data. Press 'q' to stop.\n";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Fix 4: TerminalGuard RAII — restores terminal even if we crash or Ctrl+C
    TerminalGuard guard;

    while (true) {
        clearScreen();
        printSep('=');
        std::cout << "  LIVE SENSOR DATA   [" << timestamp() << "]\n";
        std::cout << "  Press 'q' to return to menu\n";
        printSep('=');

        for (auto& entry : LIVE_PIDS) {
            std::string raw = iface->sendCommand(entry.cmd);
            if (raw == "NO DATA") continue;
            printSensorRow(decoder.decodeLiveData(entry.pid, raw));
        }
        printSep();

        char c;
        if (waitKey(c, 1000) && (c == 'q' || c == 'Q'))
            break;
    }
    // guard destructor restores terminal here
    clearScreen();
}

// ── Freeze frame — Fix 1: actually queries the interface ─────────────────────

FreezeFrame DiagnosticTool::queryFreezeFrame() {
    FreezeFrame ff;

    // Read the DTC that triggered freeze frame (service 02, PID 02 is the DTC)
    std::string dtcRaw = iface->sendCommand("03 00");
    auto dtcs = decoder.decodeDTCs(dtcRaw);
    ff.dtc = dtcs.empty() ? "None" : dtcs[0].code;

    for (auto& entry : FREEZE_PIDS) {
        std::string raw = iface->sendCommand(entry.cmd);
        if (raw == "NO DATA") continue;
        ff.sensors.push_back(decoder.decodeLiveData(entry.pid, raw));
    }
    return ff;
}

void DiagnosticTool::showFreezeFrame() {
    FreezeFrame ff = queryFreezeFrame();

    printSep('=');
    std::cout << "  FREEZE FRAME DATA\n";
    printSep('=');
    std::cout << "  Triggered by DTC: " << ff.dtc << "\n\n";

    for (const auto& s : ff.sensors)
        printSensorRow(s);
    printSep();
}

// ── DTCs ──────────────────────────────────────────────────────────────────────

void DiagnosticTool::readDTCs() {
    std::string raw       = iface->sendCommand("03 00");
    std::vector<DTC> dtcs = decoder.decodeDTCs(raw);

    printSep('=');
    std::cout << "  DIAGNOSTIC TROUBLE CODES\n";
    printSep('=');

    if (dtcs.empty()) {
        std::cout << "  No fault codes found. System OK.\n";
    } else {
        std::cout << "  Found " << dtcs.size() << " fault code(s):\n\n";
        for (const auto& d : dtcs)
            std::cout << "  " << severityLabel(d.severity)
                      << "  [" << d.code << "]  " << d.description << "\n";
    }
    printSep();
}

void DiagnosticTool::clearDTCs() {
    std::cout << "  Sending clear command...\n";
    iface->sendCommand("04 00");
    std::cout << "  Fault codes cleared successfully.\n";
}

// ── Vehicle info ──────────────────────────────────────────────────────────────

void DiagnosticTool::showVehicleInfo() {
    std::string raw = iface->sendCommand("09 02");
    std::string vin = decoder.decodeVIN(raw);

    printSep('=');
    std::cout << "  VEHICLE INFORMATION\n";
    printSep('=');
    std::cout << "  VIN      : " << vin << "\n";
    std::cout << "  Protocol : OBD-II (SAE J1979)\n";
    std::cout << "  Mode     : " << (simMode ? "Simulation" : "Real Hardware") << "\n";
    printSep();
}

// ── Inject fault — Fix 2: no dynamic_cast ────────────────────────────────────

void DiagnosticTool::injectFaultCode() {
    if (!simMode) {
        std::cout << "  Fault injection is only available in simulation mode.\n";
        return;
    }
    std::cout << "  Enter DTC code to inject (e.g. P0301): ";
    std::string code;
    std::getline(std::cin, code);

    if (code.size() < 5 || (code[0] != 'P' && code[0] != 'C' &&
                             code[0] != 'B' && code[0] != 'U')) {
        std::cout << "  Invalid code format. Use format like P0301.\n";
        return;
    }

    iface->injectDTC(code);  // virtual dispatch — no cast needed
    std::cout << "  Fault code [" << code << "] injected.\n";
}

// ── Save report — Fix 5: absolute path ───────────────────────────────────────

void DiagnosticTool::saveReport() {
    fs::path dir = reportsDir();
    fs::create_directories(dir);

    std::ostringstream report;
    report << "Vehicle Diagnostic Report\n";
    report << "Generated : " << timestamp() << "\n";
    report << "Mode      : " << (simMode ? "Simulation" : "Real Hardware") << "\n";
    report << std::string(56, '=') << "\n\n";

    // VIN
    report << "VIN: " << decoder.decodeVIN(iface->sendCommand("09 02")) << "\n\n";

    // Live snapshot
    report << "LIVE SENSOR SNAPSHOT\n" << std::string(40, '-') << "\n";
    for (auto& entry : LIVE_PIDS) {
        std::string raw = iface->sendCommand(entry.cmd);
        if (raw == "NO DATA") continue;
        SensorData s = decoder.decodeLiveData(entry.pid, raw);
        std::ostringstream row;
        row << std::left << std::setw(30) << s.name
            << std::right << std::setw(10) << std::fixed
            << std::setprecision(2) << s.value << "  " << s.unit;
        report << row.str() << "\n";
    }
    report << "\n";

    // DTCs
    auto dtcs = decoder.decodeDTCs(iface->sendCommand("03 00"));
    report << "DIAGNOSTIC TROUBLE CODES\n" << std::string(40, '-') << "\n";
    if (dtcs.empty()) {
        report << "No fault codes found. System OK.\n";
    } else {
        for (const auto& d : dtcs)
            report << severityLabel(d.severity) << "  [" << d.code << "]  " << d.description << "\n";
    }
    report << "\n";

    // Freeze frame
    FreezeFrame ff = queryFreezeFrame();
    report << "FREEZE FRAME (Triggered by: " << ff.dtc << ")\n" << std::string(40, '-') << "\n";
    for (const auto& s : ff.sensors) {
        std::ostringstream row;
        row << std::left << std::setw(30) << s.name
            << std::right << std::setw(10) << std::fixed
            << std::setprecision(2) << s.value << "  " << s.unit;
        report << row.str() << "\n";
    }

    saveToFile(report.str(), "diagnostic");
}

void DiagnosticTool::saveToFile(const std::string& content, const std::string& label) {
    fs::path path = reportsDir() / (label + "_" + filenameTimestamp() + ".txt");
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cout << "  Error: could not write to " << path << "\n";
        return;
    }
    f << content;
    std::cout << "  Report saved to: " << path.string() << "\n";
}

// ── Input validation ──────────────────────────────────────────────────────────

int DiagnosticTool::getMenuChoice(int min, int max) {
    int choice;
    while (true) {
        if (std::cin >> choice) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (choice >= min && choice <= max)
                return choice;
        } else {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        std::cout << "  Invalid input. Enter a number (" << min << "-" << max << "): ";
    }
}
