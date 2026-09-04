#include <doctest/doctest.h>
#include "plc/plc_client.hpp"
#include "plc/plc_value.hpp"
#include "support/test_plc_server.hpp"
#include <memory>

namespace {
// Returns false (and prints how to fix it) if the fixture can't bind port 102.
bool startServerOrSkip(std::unique_ptr<TestPlcServer>& out, int dbNumber, int dbSize) {
    try {
        out = std::make_unique<TestPlcServer>(dbNumber, dbSize);
        return true;
    } catch (const TestPlcServerUnavailable& e) {
        MESSAGE("SKIPPING: ", std::string(e.what()));
        return false;
    }
}
} // namespace

TEST_CASE("PlcClient connects to and disconnects from a local Snap7 server") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;

    PlcClient client;
    CHECK_FALSE(client.isConnected());
    client.connect("127.0.0.1", 0, 1);
    CHECK(client.isConnected());
    client.disconnect();
    CHECK_FALSE(client.isConnected());
}

TEST_CASE("PlcClient pduLength returns 0 when not connected, and a positive value once connected") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;

    PlcClient client;
    CHECK(client.pduLength() == 0);
    client.connect("127.0.0.1", 0, 1);
    CHECK(client.pduLength() > 0);
    client.disconnect();
}

TEST_CASE("PlcClient readMultiVars/writeMultiVars round-trip against server memory") {
    std::unique_ptr<TestPlcServer> server;
    if (!startServerOrSkip(server, 10, 64)) return;

    PlcClient client;
    client.connect("127.0.0.1", 0, 1);

    encodePlcValue(PlcType::Real, 3.5f, server->db(), 0, 0);

    uint8_t readBuf[4] = {0};
    TS7DataItem readItem{};
    readItem.Area = S7AreaDB;
    readItem.WordLen = S7WLByte;
    readItem.DBNumber = 10;
    readItem.Start = 0;
    readItem.Amount = 4;
    readItem.pdata = readBuf;
    std::vector<TS7DataItem> readItems{readItem};

    client.readMultiVars(readItems);
    CHECK(std::get<float>(decodePlcValue(PlcType::Real, readBuf, 0, 0)) == doctest::Approx(3.5f));

    uint8_t writeBuf[2];
    encodePlcValue(PlcType::Int, int32_t{77}, writeBuf, 0, 0);
    TS7DataItem writeItem{};
    writeItem.Area = S7AreaDB;
    writeItem.WordLen = S7WLByte;
    writeItem.DBNumber = 10;
    writeItem.Start = 10;
    writeItem.Amount = 2;
    writeItem.pdata = writeBuf;
    std::vector<TS7DataItem> writeItems{writeItem};

    client.writeMultiVars(writeItems);
    CHECK(std::get<int32_t>(decodePlcValue(PlcType::Int, server->db(), 10, 0)) == 77);

    client.disconnect();
}

TEST_CASE("PlcClient throws PlcError when used while not connected") {
    PlcClient client;
    std::vector<TS7DataItem> items;
    CHECK_THROWS_AS(client.readMultiVars(items), PlcError);
}

TEST_CASE("PlcClient throws PlcError when connecting to an unreachable address") {
    PlcClient client;
    CHECK_THROWS_AS(client.connect("127.0.0.1", 0, 1), PlcError); // nothing listening
}
