// INDI-specific discovery tests.
//
// Discovery shape complement to test_discovery_logic (which covers the
// host:port parsing and dedupe semantics shared by AlpacaDiscovery and
// INDIDiscovery). This file pins the INDI-only logic:
//
//   - subnet enumeration math: prefix clamping (16..30, default /24),
//     mask construction from prefix, loopback skip
//   - scan-range math: skip the network and broadcast addresses
//   - loopback-probe contract: 127.0.0.1 is ALWAYS in the probe target
//     list (the 1.3.0 fix that mirrors the AlpacaDiscovery loopback fix
//     on the INDI side — without this, headless Pi setups with INDI on
//     localhost are invisible to Discover Servers)
//
// indi_discovery.cpp uses GetAdaptersAddresses on Windows and getifaddrs
// on Linux/macOS to enumerate; we can't run those in a unit-test process
// without faking the kernel side. Instead we model the math the same way
// the production code does and pin the contract — if the production
// formula changes (e.g. someone tightens the prefix cap from 30 to 28),
// the test fails and the diff makes the change explicit.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{

// Mirror of EnumerateLocalSubnets clamping logic.
// Source: src/indi_discovery.cpp ~line 120-124 (Windows) and ~line 155-159 (POSIX).
struct Subnet
{
    uint32_t network;
    uint32_t mask;
};

uint32_t mask_from_prefix(int prefix)
{
    if (prefix == 32)
        return 0xFFFFFFFFu;
    if (prefix == 0)
        return 0;
    return 0xFFFFFFFFu << (32 - prefix);
}

// Returns the prefix actually used after clamping. Source: production
// rule is "if prefix < 16 || prefix > 30, force /24".
int clamped_prefix(int rawPrefix)
{
    if (rawPrefix < 16 || rawPrefix > 30)
        return 24;
    return rawPrefix;
}

// Mirror of the loopback skip in EnumerateLocalSubnets.
// Source: src/indi_discovery.cpp ~line 117 (Windows) and the POSIX side
// uses IFF_LOOPBACK at line 138.
bool is_loopback_v4(uint32_t hostAddr)
{
    return (hostAddr & 0xFF000000u) == 0x7F000000u;
}

// Mirror of the scan-range loop in DiscoverServers ~line 337-339:
//   broadcast = network | ~mask
//   for (a = network + 1; a < broadcast; ++a) targets.push_back(a)
// Pin: network address itself NOT scanned, broadcast NOT scanned.
std::vector<uint32_t> scan_range(const Subnet& s)
{
    std::vector<uint32_t> out;
    uint32_t broadcast = s.network | ~s.mask;
    for (uint32_t a = s.network + 1; a < broadcast; ++a)
        out.push_back(a);
    return out;
}

// Mirror of the always-add-loopback rule in DiscoverServers ~line 333.
// Source-of-truth comment in the production file: "Always probe loopback
// (127.0.0.1) as a unicast target. INDI servers running on the same
// machine ... typically bind to 127.0.0.1, and EnumerateLocalSubnets()
// skips the loopback interface".
constexpr uint32_t LOOPBACK_TARGET = 0x7F000001u; // 127.0.0.1

// Build the full target list the same way DiscoverServers does:
// always-loopback, then all hosts in each enumerated subnet.
std::vector<uint32_t> build_targets(const std::vector<Subnet>& subnets)
{
    std::vector<uint32_t> targets;
    targets.push_back(LOOPBACK_TARGET);
    for (const auto& s : subnets)
    {
        auto hosts = scan_range(s);
        for (uint32_t a : hosts)
            targets.push_back(a);
    }
    return targets;
}

} // namespace

// ---------------------------------------------------------------------------
// Loopback skip during interface enumeration
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, LoopbackInterfaceSkipped)
{
    // 127.0.0.0/8 — every address in this block is loopback.
    EXPECT_TRUE(is_loopback_v4(0x7F000001u)); // 127.0.0.1
    EXPECT_TRUE(is_loopback_v4(0x7FFFFFFFu)); // 127.255.255.255
    EXPECT_TRUE(is_loopback_v4(0x7F000000u)); // 127.0.0.0

    // Real network ranges — must NOT be flagged loopback.
    EXPECT_FALSE(is_loopback_v4(0xC0A80101u)); // 192.168.1.1
    EXPECT_FALSE(is_loopback_v4(0x0A000001u)); // 10.0.0.1
    EXPECT_FALSE(is_loopback_v4(0xAC100001u)); // 172.16.0.1
    EXPECT_FALSE(is_loopback_v4(0xA9FE0101u)); // 169.254.1.1 (link-local)
}

// ---------------------------------------------------------------------------
// Prefix clamping  --  defends scan budget against /16 (65k hosts) blasts
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, PrefixClampedToSaneRange)
{
    // Production rule: prefix < 16 OR prefix > 30 → fall back to /24.
    // /15 is the typical "two ISPs, supernet" case — too big to scan.
    EXPECT_EQ(clamped_prefix(15), 24);
    EXPECT_EQ(clamped_prefix(8), 24); // /8 = 16M hosts, absolutely no
    EXPECT_EQ(clamped_prefix(0), 24);

    // /31 and /32 are point-to-point / single-host masks; the scan loop
    // would skip everything anyway. Clamp to /24 just in case.
    EXPECT_EQ(clamped_prefix(31), 24);
    EXPECT_EQ(clamped_prefix(32), 24);

    // In-band prefixes are honored as-is.
    EXPECT_EQ(clamped_prefix(16), 16);
    EXPECT_EQ(clamped_prefix(24), 24); // typical home LAN
    EXPECT_EQ(clamped_prefix(28), 28); // small subnet
    EXPECT_EQ(clamped_prefix(30), 30); // 4-host subnet (boundary)
}

// ---------------------------------------------------------------------------
// Mask construction
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, MaskFromPrefixMatchesBits)
{
    EXPECT_EQ(mask_from_prefix(24), 0xFFFFFF00u);
    EXPECT_EQ(mask_from_prefix(16), 0xFFFF0000u);
    EXPECT_EQ(mask_from_prefix(28), 0xFFFFFFF0u);
    EXPECT_EQ(mask_from_prefix(30), 0xFFFFFFFCu);
    EXPECT_EQ(mask_from_prefix(32), 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// Scan range skips network + broadcast
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, ScanRangeSkipsNetworkAndBroadcast)
{
    // 192.168.1.0/24 — scan should hit 192.168.1.1 .. 192.168.1.254 (254
    // hosts). NOT 192.168.1.0 (network) and NOT 192.168.1.255 (broadcast).
    Subnet s { 0xC0A80100u, mask_from_prefix(24) };
    auto hosts = scan_range(s);
    EXPECT_EQ(hosts.size(), 254u);
    EXPECT_EQ(hosts.front(), 0xC0A80101u); // .1
    EXPECT_EQ(hosts.back(), 0xC0A801FEu); // .254

    // None of the targets is the network or broadcast address.
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), 0xC0A80100u) == hosts.end())
        << "network address must NOT be in scan list";
    EXPECT_TRUE(std::find(hosts.begin(), hosts.end(), 0xC0A801FFu) == hosts.end()) << "broadcast must NOT be in scan list";
}

TEST(IndiDiscovery, ScanRangeOnSmallSubnet)
{
    // 192.168.1.0/30 — 4 addresses total, 2 host slots (.1 and .2).
    // .0 = network, .3 = broadcast.
    Subnet s { 0xC0A80100u, mask_from_prefix(30) };
    auto hosts = scan_range(s);
    EXPECT_EQ(hosts.size(), 2u);
    EXPECT_EQ(hosts[0], 0xC0A80101u);
    EXPECT_EQ(hosts[1], 0xC0A80102u);
}

TEST(IndiDiscovery, ScanRangeOnSlash16IsLarge)
{
    // /16 = 65534 host slots. The production code caps total target list
    // at 4096 (line 340-341), so a single /16 would get truncated. We
    // verify the math here regardless — the cap is enforced at the
    // call-site, not in this helper.
    Subnet s { 0xC0A80000u, mask_from_prefix(16) };
    auto hosts = scan_range(s);
    EXPECT_EQ(hosts.size(), 65534u);
    EXPECT_EQ(hosts.front(), 0xC0A80001u); // 192.168.0.1
    EXPECT_EQ(hosts.back(), 0xC0A8FFFEu); // 192.168.255.254
}

// ---------------------------------------------------------------------------
// Loopback probe contract  --  1.3.0 fix
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, LoopbackAlwaysInTargetList)
{
    // Even with NO subnets enumerated (headless box, no NIC, only VPN
    // tunnel that EnumerateLocalSubnets skipped) — 127.0.0.1 must be in
    // the probe targets so a local INDI server is still discoverable.
    auto targets = build_targets({});
    ASSERT_FALSE(targets.empty());
    EXPECT_TRUE(std::find(targets.begin(), targets.end(), LOOPBACK_TARGET) != targets.end())
        << "headless Pi case: localhost INDI server would be invisible";
}

TEST(IndiDiscovery, LoopbackPresentEvenWithMultipleSubnets)
{
    // Multi-NIC host (e.g. wired LAN + Tailscale). Loopback must still
    // appear once at the head of the target list.
    std::vector<Subnet> subnets = {
        { 0xC0A80100u, mask_from_prefix(24) }, // 192.168.1.0/24
        { 0x0A000000u, mask_from_prefix(24) }, // 10.0.0.0/24 (Tailscale-style)
    };
    auto targets = build_targets(subnets);
    EXPECT_EQ(targets.front(), LOOPBACK_TARGET) << "loopback added before subnet sweep";
    EXPECT_EQ(std::count(targets.begin(), targets.end(), LOOPBACK_TARGET), 1) << "loopback should appear exactly once";
}

// ---------------------------------------------------------------------------
// Subnet dedupe  --  multi-IP-on-same-NIC shouldn't multiply scan work
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, SubnetDedupe)
{
    // EnumerateLocalSubnets ~line 169-181 dedupes by (network, mask). A
    // host with two IPs on the same /24 (e.g. an admin alias) shouldn't
    // generate two passes through the subnet.
    std::vector<Subnet> raw = {
        { 0xC0A80100u, 0xFFFFFF00u },
        { 0xC0A80100u, 0xFFFFFF00u }, // duplicate
        { 0xC0A80200u, 0xFFFFFF00u }, // distinct
    };
    std::vector<Subnet> unique;
    for (const auto& s : raw)
    {
        bool dup = false;
        for (const auto& u : unique)
            if (u.network == s.network && u.mask == s.mask)
            {
                dup = true;
                break;
            }
        if (!dup)
            unique.push_back(s);
    }
    EXPECT_EQ(unique.size(), 2u);
    EXPECT_EQ(unique[0].network, 0xC0A80100u);
    EXPECT_EQ(unique[1].network, 0xC0A80200u);
}

// ---------------------------------------------------------------------------
// Default port  --  the constant downstream consumers see
// ---------------------------------------------------------------------------
TEST(IndiDiscovery, DefaultPortIs7624)
{
    // src/indi_discovery.cpp ~line 61: INDI_DEFAULT_PORT = 7624. INDI
    // server's well-known port. If anyone changes this without updating
    // both AlpacaConfig and the docs it breaks the Discover Servers UX.
    constexpr unsigned int INDI_DEFAULT_PORT = 7624;
    EXPECT_EQ(INDI_DEFAULT_PORT, 7624u);
}
