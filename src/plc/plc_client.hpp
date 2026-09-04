#pragma once
#include <snap7.h>
#include <stdexcept>
#include <string>
#include <vector>

class PlcError : public std::runtime_error {
public:
    PlcError(int code, const std::string& message);
    int code() const;
private:
    int code_;
};

// Thin RAII wrapper around a single Snap7 client connection. Not
// thread-safe by itself — the caller (PlcIoThread) is responsible for
// ensuring only one thread ever touches a given instance, since Snap7
// client objects aren't safe for concurrent calls from multiple threads.
class PlcClient {
public:
    PlcClient();
    ~PlcClient();
    PlcClient(const PlcClient&) = delete;
    PlcClient& operator=(const PlcClient&) = delete;

    void connect(const std::string& ip, int rack, int slot);
    void disconnect();
    bool isConnected() const;

    // Returns the negotiated PDU length for the current connection, or 0
    // if it can't be queried (e.g. not connected, or the call itself
    // fails) — callers should fall back to a conservative default in
    // that case rather than treating 0 as a usable value.
    int pduLength() const;

    // Every item's Area/WordLen/DBNumber/Start/Amount/pdata must be set by
    // the caller; WordLen is always S7WLByte in this app (typed decode/encode
    // happens via plc_value.hpp, not at the Snap7 call level).
    void readMultiVars(std::vector<TS7DataItem>& items);
    void writeMultiVars(std::vector<TS7DataItem>& items);

private:
    S7Object client_;
    bool connected_ = false;
};
