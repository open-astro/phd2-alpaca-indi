/*
 *  indi_discovery.cpp
 *  PHD Guiding
 *
 *  Copyright (c) 2026 PHD2 Developers
 *  All rights reserved.
 *
 *  This source code is distributed under the following "BSD" license
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *    Neither the name of openphdguiding.org nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "indi_discovery.h"

#include <vector>
#include <cstring>

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# include <iphlpapi.h>
typedef SOCKET indi_socket_t;
# define INDI_INVALID_SOCKET INVALID_SOCKET
# define indi_closesocket(s) closesocket(s)
#else
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <ifaddrs.h>
# include <net/if.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
typedef int indi_socket_t;
# define INDI_INVALID_SOCKET (-1)
# define indi_closesocket(s) ::close(s)
#endif

static const unsigned int INDI_DEFAULT_PORT = 7624;

// Cap on concurrent in-flight TCP connect()s. Must stay below FD_SETSIZE, which
// is 64 by default on Windows (winsock2.h's fd_set is fixed-size). Raising it
// requires defining FD_SETSIZE before winsock2.h is included, which is awkward
// with PHD2's precompiled header. 60 leaves comfortable margin and is fast enough.
static const size_t INDI_MAX_CONCURRENT = 60;

static wxString AddrToString(uint32_t addrHostOrder)
{
    in_addr a;
    a.s_addr = htonl(addrHostOrder);
    char buf[INET_ADDRSTRLEN] = { 0 };
#ifdef _WIN32
    InetNtopA(AF_INET, &a, buf, INET_ADDRSTRLEN);
#else
    inet_ntop(AF_INET, &a, buf, INET_ADDRSTRLEN);
#endif
    return wxString(buf, wxConvUTF8);
}

struct Subnet
{
    uint32_t network; // host-order
    uint32_t mask; // host-order
};

static std::vector<Subnet> EnumerateLocalSubnets()
{
    std::vector<Subnet> subnets;

#ifdef _WIN32
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 0;
    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size);
    if (ret != ERROR_BUFFER_OVERFLOW)
        return subnets;

    std::vector<BYTE> buffer(size);
    auto *addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &size);
    if (ret != NO_ERROR)
        return subnets;

    for (auto adapter = addresses; adapter; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        for (auto unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next)
        {
            auto sa = reinterpret_cast<sockaddr_in *>(unicast->Address.lpSockaddr);
            if (!sa || sa->sin_family != AF_INET)
                continue;

            uint32_t hostAddr = ntohl(sa->sin_addr.s_addr);
            if ((hostAddr & 0xFF000000u) == 0x7F000000u) // skip 127.0.0.0/8
                continue;

            UINT8 prefix = unicast->OnLinkPrefixLength;
            if (prefix < 16 || prefix > 30) // cap to sane subnet sizes to bound scan
                prefix = 24;

            uint32_t mask = prefix == 32 ? 0xFFFFFFFFu : (0xFFFFFFFFu << (32 - prefix));
            Subnet s { hostAddr & mask, mask };
            subnets.push_back(s);
        }
    }
#else
    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0)
        return subnets;

    for (auto ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK))
            continue;
        if (!ifa->ifa_netmask)
            continue;

        uint32_t hostAddr = ntohl(reinterpret_cast<sockaddr_in *>(ifa->ifa_addr)->sin_addr.s_addr);
        uint32_t mask = ntohl(reinterpret_cast<sockaddr_in *>(ifa->ifa_netmask)->sin_addr.s_addr);

        // Cap to /24 minimum prefix length (255 hosts) so we don't try to scan a /16.
        // Count leading 1-bits in mask:
        int prefix = 0;
        uint32_t m = mask;
        while (m & 0x80000000u)
        {
            prefix++;
            m <<= 1;
        }
        if (prefix < 16 || prefix > 30)
        {
            prefix = 24;
            mask = 0xFFFFFF00u;
        }

        Subnet s { hostAddr & mask, mask };
        subnets.push_back(s);
    }

    freeifaddrs(ifap);
#endif

    // Deduplicate
    std::vector<Subnet> unique;
    for (const auto& s : subnets)
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
    return unique;
}

static bool SetNonBlocking(indi_socket_t s)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
        return false;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

struct PendingConnect
{
    indi_socket_t sock;
    uint32_t addr; // host-order
};

// Probe a list of IP addresses in a single non-blocking batch.
// Returns IPs (host-order) that accepted a TCP connection within the per-batch deadline.
static std::vector<uint32_t> ProbeBatch(const std::vector<uint32_t>& addrs, int perBatchTimeoutMs)
{
    std::vector<uint32_t> hits;
    std::vector<PendingConnect> pending;
    pending.reserve(addrs.size());

    for (uint32_t addr : addrs)
    {
        indi_socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
        if (s == INDI_INVALID_SOCKET)
            continue;
        if (!SetNonBlocking(s))
        {
            indi_closesocket(s);
            continue;
        }

        sockaddr_in dest {};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(INDI_DEFAULT_PORT);
        dest.sin_addr.s_addr = htonl(addr);

        int rc = ::connect(s, reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
        if (rc == 0)
        {
            hits.push_back(addr);
            indi_closesocket(s);
            continue;
        }

#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS)
#else
        if (errno != EINPROGRESS && errno != EWOULDBLOCK)
#endif
        {
            indi_closesocket(s);
            continue;
        }

        pending.push_back({ s, addr });
    }

    // Wait for any of the in-flight connect()s to complete (or timeout).
    auto deadlineMs = perBatchTimeoutMs;
    while (!pending.empty() && deadlineMs > 0)
    {
        fd_set wfds, efds;
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        indi_socket_t maxfd = 0;
        for (const auto& p : pending)
        {
            FD_SET(p.sock, &wfds);
            FD_SET(p.sock, &efds);
            if (p.sock > maxfd)
                maxfd = p.sock;
        }

        int sliceMs = wxMin(deadlineMs, 200);
        timeval tv;
        tv.tv_sec = sliceMs / 1000;
        tv.tv_usec = (sliceMs % 1000) * 1000;

        int n = ::select(static_cast<int>(maxfd) + 1, nullptr, &wfds, &efds, &tv);
        deadlineMs -= sliceMs;
        if (n <= 0)
            continue;

        std::vector<PendingConnect> stillPending;
        stillPending.reserve(pending.size());
        for (const auto& p : pending)
        {
            bool ready = FD_ISSET(p.sock, &wfds) || FD_ISSET(p.sock, &efds);
            if (!ready)
            {
                stillPending.push_back(p);
                continue;
            }

            int soerr = 0;
            socklen_t len = sizeof(soerr);
            if (::getsockopt(p.sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&soerr), &len) == 0 && soerr == 0)
            {
                hits.push_back(p.addr);
            }
            indi_closesocket(p.sock);
        }
        pending.swap(stillPending);
    }

    // Anything still pending past the deadline is treated as a miss.
    for (const auto& p : pending)
        indi_closesocket(p.sock);

    return hits;
}

wxArrayString INDIDiscovery::DiscoverServers(int timeoutSeconds)
{
    wxArrayString result;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        Debug.Write("INDIDiscovery: WSAStartup failed\n");
        return result;
    }
#endif

    auto subnets = EnumerateLocalSubnets();
    Debug.Write(wxString::Format("INDIDiscovery: found %u local subnet(s)\n", static_cast<unsigned int>(subnets.size())));

    // Build the full target list (all host addresses across all subnets).
    std::vector<uint32_t> targets;
    for (const auto& s : subnets)
    {
        uint32_t broadcast = s.network | ~s.mask;
        for (uint32_t a = s.network + 1; a < broadcast; ++a)
            targets.push_back(a);
        if (targets.size() > 4096) // safety cap
            break;
    }

    Debug.Write(wxString::Format("INDIDiscovery: probing %u host(s) for port %u\n", static_cast<unsigned int>(targets.size()),
                                 INDI_DEFAULT_PORT));

    // Split scan budget across batches. Each batch gets a per-host-equivalent slice.
    int totalMs = timeoutSeconds * 1000;
    if (totalMs < 500)
        totalMs = 500;

    size_t batches = (targets.size() + INDI_MAX_CONCURRENT - 1) / INDI_MAX_CONCURRENT;
    if (batches == 0)
        batches = 1;
    int perBatchMs = totalMs / static_cast<int>(batches);
    if (perBatchMs < 300)
        perBatchMs = 300;

    std::vector<uint32_t> allHits;
    for (size_t i = 0; i < targets.size(); i += INDI_MAX_CONCURRENT)
    {
        std::vector<uint32_t> batch(targets.begin() + i, targets.begin() + wxMin(i + INDI_MAX_CONCURRENT, targets.size()));
        auto hits = ProbeBatch(batch, perBatchMs);
        Debug.Write(
            wxString::Format("INDIDiscovery: batch %u/%u (%u hosts, %d ms) -> %u hit(s)\n",
                             static_cast<unsigned int>(i / INDI_MAX_CONCURRENT + 1),
                             static_cast<unsigned int>((targets.size() + INDI_MAX_CONCURRENT - 1) / INDI_MAX_CONCURRENT),
                             static_cast<unsigned int>(batch.size()), perBatchMs, static_cast<unsigned int>(hits.size())));
        for (uint32_t h : hits)
            allHits.push_back(h);
    }

    for (uint32_t addr : allHits)
        result.Add(wxString::Format("%s:%u", AddrToString(addr), INDI_DEFAULT_PORT));

    Debug.Write(wxString::Format("INDIDiscovery: discovered %u server(s)\n", static_cast<unsigned int>(result.GetCount())));

#ifdef _WIN32
    WSACleanup();
#endif

    return result;
}

bool INDIDiscovery::ParseServerString(const wxString& serverStr, wxString& host, long& port)
{
    int colonPos = serverStr.Find(':', true /* from end */);
    if (colonPos == wxNOT_FOUND)
        return false;

    host = serverStr.Left(colonPos);
    wxString portStr = serverStr.Mid(colonPos + 1);
    if (host.IsEmpty() || portStr.IsEmpty())
        return false;
    return portStr.ToLong(&port) && port > 0 && port < 65536;
}
