#include <snap7.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop{false};
void handleSignal(int) { g_stop = true; }
} // namespace

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::vector<unsigned char> db10(64, 0);
    unsigned char speedBytes[4] = {0x42, 0x28, 0x00, 0x00}; // big-endian float 42.0
    std::memcpy(db10.data(), speedBytes, 4);
    db10[4] = 0x01; // motor1_running = true (bit 0)

    std::vector<unsigned char> db20(64, 0); // tank_level (INT @ offset 0)

    S7Object server = Srv_Create();
    Srv_RegisterArea(server, srvAreaDB, 10, db10.data(), static_cast<int>(db10.size()));
    Srv_RegisterArea(server, srvAreaDB, 20, db20.data(), static_cast<int>(db20.size()));

    int rc = Srv_StartTo(server, "127.0.0.1");
    if (rc != 0) {
        std::fprintf(stderr,
            "fake_plc_server: Srv_StartTo failed (code %d). Port 102 needs elevated "
            "privileges on Linux: 'sudo setcap cap_net_bind_service=+ep <path-to-fake_plc_server>' "
            "or 'sudo sysctl net.ipv4.ip_unprivileged_port_start=0'.\n", rc);
        Srv_Destroy(&server);
        return 1;
    }

    std::printf("fake_plc_server: listening on 127.0.0.1:102 (DB10, DB20). Ctrl+C to stop.\n");
    int counter = 0;
    while (!g_stop) {
        int16_t level = static_cast<int16_t>((++counter / 10) % 1000);
        db20[0] = static_cast<unsigned char>((level >> 8) & 0xFF);
        db20[1] = static_cast<unsigned char>(level & 0xFF);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    Srv_Stop(server);
    Srv_Destroy(&server);
    std::printf("fake_plc_server: stopped.\n");
    return 0;
}
