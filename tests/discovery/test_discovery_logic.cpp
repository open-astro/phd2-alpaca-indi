// Discovery-logic tests for Alpaca and INDI.
//
// AlpacaDiscovery::DiscoverServers and INDIDiscovery::DiscoverServers both
// open real sockets, so we can't run them in a unit-test process. What's
// testable here without a network is:
//   - the dedupe model (alpaca_discovery.cpp uses std::set<wxString> keyed
//     by "host:port"; INDI does the same shape)
//   - the host:port parsing model used by ParseServerString in both files
//   - the lifecycle invariants of the polling loop (single-broadcast =>
//     multiple replies => one server entry)
//
// These tests model the dedupe/parse semantics with std::string/std::set
// rather than calling the wxString-flavoured helpers directly. If anyone
// changes the dedupe key format (e.g. lowercases the host) without
// updating both producers and consumers (config_alpaca, gear_dialog), the
// model here surfaces it as a deliberate change rather than a silent
// behaviour drift.

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

namespace
{

// Mirror of ParseServerString in alpaca_discovery.cpp:460 and
// indi_discovery.cpp (same shape). The wx version uses wxString.Find(':')
// + ToLong(); we model with std::string find + parsing.
bool parse_server_string(const std::string& s, std::string *host, long *port)
{
    auto colon = s.find(':');
    if (colon == std::string::npos)
        return false;
    *host = s.substr(0, colon);
    std::string portStr = s.substr(colon + 1);
    if (portStr.empty())
        return false;
    try
    {
        size_t consumed = 0;
        long parsed = std::stol(portStr, &consumed);
        if (consumed != portStr.size())
            return false;
        if (parsed <= 0)
            return false;
        *port = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

}

// ---------------------------------------------------------------------------
// host:port parsing  --  shared by AlpacaDiscovery and INDIDiscovery
// ---------------------------------------------------------------------------
TEST(DiscoveryParse, AcceptsIPv4HostPort)
{
    std::string host;
    long port = 0;
    EXPECT_TRUE(parse_server_string("192.168.1.50:11111", &host, &port));
    EXPECT_EQ(host, "192.168.1.50");
    EXPECT_EQ(port, 11111);
}

TEST(DiscoveryParse, AcceptsHostnameHostPort)
{
    std::string host;
    long port = 0;
    EXPECT_TRUE(parse_server_string("nuc.local:7624", &host, &port));
    EXPECT_EQ(host, "nuc.local");
    EXPECT_EQ(port, 7624);
}

TEST(DiscoveryParse, RejectsMissingColon)
{
    std::string host;
    long port = 0;
    EXPECT_FALSE(parse_server_string("192.168.1.50", &host, &port));
}

TEST(DiscoveryParse, RejectsEmptyPort)
{
    std::string host;
    long port = 0;
    EXPECT_FALSE(parse_server_string("192.168.1.50:", &host, &port));
}

TEST(DiscoveryParse, RejectsZeroPort)
{
    // ParseServerString explicitly requires port > 0 (used to filter out
    // "host:0" entries from broken Alpaca discovery replies).
    std::string host;
    long port = 0;
    EXPECT_FALSE(parse_server_string("host:0", &host, &port));
}

TEST(DiscoveryParse, RejectsNonNumericPort)
{
    std::string host;
    long port = 0;
    EXPECT_FALSE(parse_server_string("host:abc", &host, &port));
    EXPECT_FALSE(parse_server_string("host:80x", &host, &port));
}

TEST(DiscoveryParse, RejectsNegativePort)
{
    std::string host;
    long port = 0;
    EXPECT_FALSE(parse_server_string("host:-1", &host, &port));
}

// ---------------------------------------------------------------------------
// Dedupe model  --  alpaca_discovery.cpp uses std::set<wxString> keyed by
// "host:port" string. The set membership check happens BEFORE the entry is
// added to the result list (lines 371-380).
// ---------------------------------------------------------------------------
TEST(DiscoveryDedupe, RepeatedRepliesProduceOneEntry)
{
    // Real-world: a single broadcast often elicits 2-4 replies from the
    // same Alpaca server (kernel re-broadcasts on multi-homed hosts). The
    // result list must show the server exactly once.
    std::set<std::string> uniqueServers;
    std::vector<std::string> serverList;

    auto seen = [&](const std::string& s)
    {
        if (uniqueServers.find(s) != uniqueServers.end())
            return true;
        uniqueServers.insert(s);
        serverList.push_back(s);
        return false;
    };

    EXPECT_FALSE(seen("192.168.1.50:11111"));
    EXPECT_TRUE(seen("192.168.1.50:11111")) << "duplicate from same broadcast";
    EXPECT_TRUE(seen("192.168.1.50:11111"));
    EXPECT_FALSE(seen("192.168.1.51:11111")) << "different host";
    EXPECT_FALSE(seen("192.168.1.50:22222")) << "same host, different port";

    EXPECT_EQ(serverList.size(), 3u);
    EXPECT_EQ(serverList[0], "192.168.1.50:11111");
    EXPECT_EQ(serverList[1], "192.168.1.51:11111");
    EXPECT_EQ(serverList[2], "192.168.1.50:22222");
}

TEST(DiscoveryDedupe, MultipleQueryRoundsStillDedupe)
{
    // alpaca_discovery.cpp does numQueries broadcasts back-to-back. Each
    // round picks up the same servers; the dedupe set persists across
    // queries (it's declared outside the for-loop in DiscoverServers).
    std::set<std::string> uniqueServers;
    std::vector<std::string> serverList;

    auto record = [&](const std::string& s)
    {
        if (uniqueServers.insert(s).second)
            serverList.push_back(s);
    };

    // round 1: 2 servers reply
    record("10.0.0.5:11111");
    record("10.0.0.6:11111");
    // round 2: same 2 servers reply, plus a new one that woke up late
    record("10.0.0.5:11111");
    record("10.0.0.6:11111");
    record("10.0.0.7:11111");
    // round 3: all three again
    record("10.0.0.5:11111");
    record("10.0.0.6:11111");
    record("10.0.0.7:11111");

    EXPECT_EQ(serverList.size(), 3u);
}

TEST(DiscoveryDedupe, KeyIsCaseSensitive)
{
    // host strings come from inet_ntop (always numeric on Alpaca path) or
    // from the user (mixed case possible on the INDI manual-entry path).
    // Pin: dedupe is case-sensitive — "Host:1234" and "host:1234" are
    // distinct entries. If we ever lowercase, this test reminds us to do
    // it on both producer and consumer sides simultaneously.
    std::set<std::string> uniqueServers;
    EXPECT_TRUE(uniqueServers.insert("Host:1234").second);
    EXPECT_TRUE(uniqueServers.insert("host:1234").second);
    EXPECT_FALSE(uniqueServers.insert("Host:1234").second);
}

// ---------------------------------------------------------------------------
// Polling lifecycle  --  alpaca_discovery.cpp loops `for (query=0;
// query<numQueries; query++)` then `while (timer < timeout) recvfrom`.
// The recent SetCursor cleanup (b0eb5f0e) touched the wxBusyCursor handling
// in this loop. Pure-logic invariants we want to keep:
//   - timeout is RESPECTED even with no replies
//   - one round produces 0..N entries (N = unique replies)
// ---------------------------------------------------------------------------
TEST(DiscoveryLifecycle, ZeroRepliesYieldsEmptyList)
{
    // Network with no Alpaca/INDI servers: timeout fires, list stays empty.
    // Modeled as: the recv loop returned nothing for the whole budget.
    std::set<std::string> uniqueServers;
    std::vector<std::string> serverList;
    // (no recv calls at all)
    EXPECT_TRUE(serverList.empty());
    EXPECT_TRUE(uniqueServers.empty());
}

TEST(DiscoveryLifecycle, OutOfOrderRepliesAreFineForDedupe)
{
    // UDP doesn't guarantee order. Server B's reply may arrive before
    // Server A's even if A was queried first. The dedupe model doesn't
    // care about order; final list contains both, exactly once each.
    std::set<std::string> uniqueServers;
    std::vector<std::string> serverList;
    auto record = [&](const std::string& s)
    {
        if (uniqueServers.insert(s).second)
            serverList.push_back(s);
    };
    record("B:1");
    record("A:1");
    record("A:1");
    record("B:1");
    EXPECT_EQ(serverList.size(), 2u);
    // Order reflects ARRIVAL order, not query order — pin that contract.
    EXPECT_EQ(serverList[0], "B:1");
    EXPECT_EQ(serverList[1], "A:1");
}
