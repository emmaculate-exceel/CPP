#include "serial_port.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <chrono>
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
