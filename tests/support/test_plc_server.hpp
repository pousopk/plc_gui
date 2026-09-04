#pragma once
#include <snap7.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class TestPlcServerUnavailable : public std::runtime_error {
public:
    explicit TestPlcServerUnavailable(const std::string& msg) : std::runtime_error(msg) {}
};

// In-process Snap7 S7 server bound to 127.0.0.1, standing in for real PLC
// hardware in tests. Registers one DB whose memory is directly accessible
// via db() so a test can seed/inspect values without going through the
// network round-trip it's also exercising.
class TestPlcServer {
public:
    TestPlcServer(int dbNumber, int dbSize);
    ~TestPlcServer();
    TestPlcServer(const TestPlcServer&) = delete;
    TestPlcServer& operator=(const TestPlcServer&) = delete;

    uint8_t* db();
    int dbNumber() const;

private:
    S7Object server_;
    std::vector<uint8_t> dbBuffer_;
    int dbNumber_;
};
