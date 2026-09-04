#include "support/test_plc_server.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

// Probes whether TCP port 102 can be bound, via a plain POSIX socket — never
// touching Snap7's Srv_* API. Discovered the hard way: when the port can't
// be bound, a failed Srv_StartTo() followed by Srv_Destroy() leaves this
// snap7 build's internal server-area bookkeeping corrupted, and a *second*
// Srv_Create()/Srv_Destroy() cycle in the same process then segfaults inside
// PS7AreaContainer::Dispose(). Checking availability up front, once, and
// short-circuiting before any Srv_* call when unavailable sidesteps that
// entirely — every TestPlcServer construction in an unprivileged process
// throws immediately instead of touching the fragile cleanup path at all.
bool canBindPort102() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(102);
    bool ok = ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    ::close(fd);
    return ok;
}

} // namespace

TestPlcServer::TestPlcServer(int dbNumber, int dbSize)
    : dbBuffer_(static_cast<size_t>(dbSize), 0), dbNumber_(dbNumber) {
    static const bool kPortAvailable = canBindPort102();
    if (!kPortAvailable) {
        throw TestPlcServerUnavailable(
            "Cannot bind TCP port 102. The S7 protocol always uses this privileged port on "
            "Linux. Ask the user to run ONE of: "
            "'sudo setcap cap_net_bind_service=+ep <path-to-plc_gui_tests>' "
            "(grant the capability to the test binary, survives rebuilds only if re-run), or "
            "'sudo sysctl net.ipv4.ip_unprivileged_port_start=0' (lowers the floor for all processes).");
    }

    server_ = Srv_Create();

    int rc = Srv_RegisterArea(server_, srvAreaDB, static_cast<word>(dbNumber), dbBuffer_.data(), dbSize);
    if (rc != 0) {
        Srv_Destroy(&server_);
        throw TestPlcServerUnavailable("Srv_RegisterArea failed: code " + std::to_string(rc));
    }

    rc = Srv_StartTo(server_, "127.0.0.1");
    if (rc != 0) {
        Srv_Destroy(&server_);
        throw TestPlcServerUnavailable("Srv_StartTo failed: code " + std::to_string(rc));
    }
}

TestPlcServer::~TestPlcServer() {
    Srv_Stop(server_);
    Srv_Destroy(&server_);
}

uint8_t* TestPlcServer::db() { return dbBuffer_.data(); }
int TestPlcServer::dbNumber() const { return dbNumber_; }
