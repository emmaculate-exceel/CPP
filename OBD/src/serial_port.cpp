#include "serial_port.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <thread>
#include <iostream>

SerialPort::SerialPort(const std::string& port, int baud)
    : portPath(port), baudRate(baud), fd(-1), connected(false), retryCount(3) {}

SerialPort::~SerialPort() {
    disconnect();
}

bool SerialPort::connect() {
    if (connected) return true;

    fd = open(portPath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        std::cerr << "  Error: cannot open port " << portPath << "\n";
        return false;
    }
    fcntl(fd, F_SETFL, 0);

    if (!configure()) {
        close(fd);
        fd = -1;
        return false;
    }

    connected = true;
    initialize();
    return true;
}

bool SerialPort::configure() {
    struct termios options;
    if (tcgetattr(fd, &options) != 0)
        return false;

    speed_t speed = B38400;
    if (baudRate == 9600)   speed = B9600;
    if (baudRate == 115200) speed = B115200;

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10; // 1 second timeout

    tcflush(fd, TCIFLUSH);
    return tcsetattr(fd, TCSANOW, &options) == 0;
}

void SerialPort::initialize() {
    sendCommand("ATZ");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sendCommand("ATE0");
    sendCommand("ATL0");
    sendCommand("ATSP0");
}

bool SerialPort::ensureConnected() {
    if (connected) return true;
    std::cerr << "  Connection lost. Reconnecting...\n";
    return connect();
}

std::string SerialPort::sendCommand(const std::string& cmd) {
    if (!ensureConnected())
        return "NO DATA";

    for (int attempt = 0; attempt < retryCount; attempt++) {
        std::string toSend = cmd + "\r";
        ssize_t written = write(fd, toSend.c_str(), toSend.size());

        if (written < 0) {
            // Write failed — port likely disconnected
            connected = false;
            if (!ensureConnected()) return "NO DATA";
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string response = readResponse();

        if (!response.empty() && response != "NO DATA")
            return response;
    }
    return "NO DATA";
}

std::string SerialPort::readResponse() {
    std::string response;
    char buf[256];

    auto start = std::chrono::steady_clock::now();
    while (true) {
        int bytesRead = read(fd, buf, sizeof(buf) - 1);
        if (bytesRead > 0) {
            buf[bytesRead] = '\0';
            response += buf;
            if (response.find('>') != std::string::npos)
                break;
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(3))
            break;
    }

    // Strip prompt and whitespace
    auto pos = response.find('>');
    if (pos != std::string::npos)
        response = response.substr(0, pos);

    std::string clean;
    for (char c : response)
        if (c != '\r' && c != '\n' && c != '>')
            clean += c;

    size_t s = clean.find_first_not_of(' ');
    size_t e = clean.find_last_not_of(' ');
    if (s == std::string::npos) return "NO DATA";
    return clean.substr(s, e - s + 1);
}

void SerialPort::disconnect() {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    connected = false;
}

bool SerialPort::probe() {
    if (access(portPath.c_str(), F_OK | R_OK | W_OK) != 0)
        return false;

    int testFd = open(portPath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (testFd < 0) return false;
    fcntl(testFd, F_SETFL, 0);

    // Use instance baud rate — supports 9600, 38400, 115200
    speed_t speed = B38400;
    if (baudRate == 9600)   speed = B9600;
    if (baudRate == 115200) speed = B115200;

    struct termios opts = {};
    tcgetattr(testFd, &opts);
    cfsetispeed(&opts, speed);
    cfsetospeed(&opts, speed);
    opts.c_cflag  = CS8 | CLOCAL | CREAD;
    opts.c_lflag  = 0;
    opts.c_iflag  = 0;
    opts.c_oflag  = 0;
    opts.c_cc[VMIN]  = 0;
    opts.c_cc[VTIME] = 3; // Fix 1: 300ms timeout (was 2 seconds)
    tcsetattr(testFd, TCSANOW, &opts);
    tcflush(testFd, TCIFLUSH);

    const char* cmd = "ATI\r";
    write(testFd, cmd, 4);

    char buf[128] = {};
    int  total    = 0;
    auto start    = std::chrono::steady_clock::now();

    while (total < 127) {
        int n = read(testFd, buf + total, 127 - total);
        if (n > 0) {
            total += n;
            if (buf[total - 1] == '>') break;
        }
        // Fix 1: 300ms cutoff (was 2 seconds) — unresponsive port moves on fast
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(300))
            break;
    }

    close(testFd);
    std::string resp(buf, total);
    return resp.find("ELM") != std::string::npos ||
           resp.find("elm") != std::string::npos;
}

// Fix 1 + Fix 3: scans 7 ports × 3 baud rates, worst case ~6s (was ~14s)
std::pair<std::string, int> SerialPort::autoDetect() {
    static const std::vector<std::string> PORTS = {
        "/dev/rfcomm0",  // Bluetooth ELM327
        "/dev/rfcomm1",
        "/dev/ttyUSB0",  // USB ELM327
        "/dev/ttyUSB1",
        "/dev/ttyUSB2",
        "/dev/ttyACM0",  // Some USB adapters
        "/dev/ttyACM1",
    };
    // Fix 3: try common baud rates — cheap clones often differ from default
    static const std::vector<int> BAUDS = {38400, 9600, 115200};

    for (const auto& port : PORTS) {
        std::cout << "    Trying " << std::left << std::setw(18) << port << " ... " << std::flush;
        for (int baud : BAUDS) {
            SerialPort sp(port, baud);
            if (sp.probe()) {
                std::cout << "detected! (" << baud << " baud)\n";
                return {port, baud};
            }
        }
        std::cout << "no response\n";
    }
    return {"", 0};
}
