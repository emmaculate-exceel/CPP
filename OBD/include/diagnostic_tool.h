#pragma once
#include "obd_interface.h"
#include "obd2.h"
#include "decoder.h"
#include <memory>
#include <string>
#include <termios.h>

// RAII: restores terminal state on scope exit and on SIGINT/SIGTERM/SIGHUP
class TerminalGuard {
public:
    TerminalGuard();
    ~TerminalGuard();
private:
    using SigHandler = void(*)(int);
    struct termios saved;
    SigHandler prevINT, prevTERM, prevHUP;
    static TerminalGuard* instance;
    static void handle(int sig);
};

class DiagnosticTool {
public:
    DiagnosticTool();
    void run();

private:
    std::unique_ptr<OBDInterface> iface;
    OBDDecoder                    decoder;
    bool                          simMode;

    void        selectMode();
    void        showMenu();
    void        readLiveData();
    void        readDTCs();
    void        clearDTCs();
    void        showVehicleInfo();
    void        showFreezeFrame();
    void        injectFaultCode();
    void        saveReport();

    FreezeFrame queryFreezeFrame();
    int         getMenuChoice(int min, int max);
    void        saveToFile(const std::string& content, const std::string& label);
};
