#include "plc/plc_client.hpp"

namespace {
std::string errorText(int code) {
    char buf[1024];
    Cli_ErrorText(code, buf, sizeof(buf));
    return std::string(buf);
}
} // namespace

PlcError::PlcError(int code, const std::string& message)
    : std::runtime_error(message), code_(code) {}

int PlcError::code() const { return code_; }

PlcClient::PlcClient() {
    client_ = Cli_Create();
}

PlcClient::~PlcClient() {
    Cli_Destroy(&client_);
}

void PlcClient::connect(const std::string& ip, int rack, int slot) {
    int rc = Cli_ConnectTo(client_, ip.c_str(), rack, slot);
    if (rc != 0) throw PlcError(rc, errorText(rc));
    connected_ = true;
}

void PlcClient::disconnect() {
    Cli_Disconnect(client_);
    connected_ = false;
}

bool PlcClient::isConnected() const { return connected_; }

int PlcClient::pduLength() const {
    if (!connected_) return 0;
    int requested = 0, negotiated = 0;
    int rc = Cli_GetPduLength(client_, &requested, &negotiated);
    if (rc != 0 || negotiated <= 0) return 0;
    return negotiated;
}

void PlcClient::readMultiVars(std::vector<TS7DataItem>& items) {
    if (!connected_) throw PlcError(-1, "readMultiVars called while not connected");
    int rc = Cli_ReadMultiVars(client_, items.data(), static_cast<int>(items.size()));
    if (rc != 0) throw PlcError(rc, errorText(rc));
}

void PlcClient::writeMultiVars(std::vector<TS7DataItem>& items) {
    if (!connected_) throw PlcError(-1, "writeMultiVars called while not connected");
    int rc = Cli_WriteMultiVars(client_, items.data(), static_cast<int>(items.size()));
    if (rc != 0) throw PlcError(rc, errorText(rc));
}
