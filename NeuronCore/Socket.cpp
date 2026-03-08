#include "pch.h"
#include "Socket.h"
#include "Debug.h"

#include <ws2tcpip.h>

namespace Neuron
{
  // ── WSA lifetime ────────────────────────────────────────────────────────────

  namespace
  {
    struct WsaInit
    {
      WsaInit()
      {
        WSADATA wsa{};
        int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rc != 0)
          DebugTrace("WSAStartup failed: {}\n", rc);
      }

      ~WsaInit() { WSACleanup(); }
    };

    // One-time initialization; cleaned up at static destruction.
    [[maybe_unused]] WsaInit& ensureWsa()
    {
      static WsaInit s_wsa;
      return s_wsa;
    }
  } // anonymous

  // ── UDPSocket ───────────────────────────────────────────────────────────────

  static constexpr uintptr_t INVALID_SOCK = ~static_cast<uintptr_t>(0);

  UDPSocket::~UDPSocket() { close(); }

  UDPSocket::UDPSocket(UDPSocket&& other) noexcept
    : m_sock(other.m_sock),
      m_open(other.m_open)
  {
    other.m_sock = INVALID_SOCK;
    other.m_open = false;
  }

  UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept
  {
    if (this != &other)
    {
      close();
      m_sock = other.m_sock;
      m_open = other.m_open;
      other.m_sock = INVALID_SOCK;
      other.m_open = false;
    }
    return *this;
  }

  bool UDPSocket::bind(const std::string& addr, uint16_t port)
  {
    ensureWsa();

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
    {
      DebugTrace("socket() failed: {}\n", WSAGetLastError());
      return false;
    }

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    if (addr == "0.0.0.0" || addr.empty())
      sa.sin_addr.s_addr = INADDR_ANY;
    else
      inet_pton(AF_INET, addr.c_str(), &sa.sin_addr);

    if (::bind(s, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == SOCKET_ERROR)
    {
      DebugTrace("bind() failed: {}\n", WSAGetLastError());
      closesocket(s);
      return false;
    }

    // Set non-blocking mode
    u_long mode = 1;
    if (ioctlsocket(s, FIONBIO, &mode) == SOCKET_ERROR)
    {
      DebugTrace("ioctlsocket(FIONBIO) failed: {}\n", WSAGetLastError());
      closesocket(s);
      return false;
    }

    m_sock = s;
    m_open = true;
    return true;
  }

  bool UDPSocket::sendTo(std::span<const uint8_t> data, const std::string& addr, uint16_t port)
  {
    if (!m_open)
      return false;

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, addr.c_str(), &sa.sin_addr);

    int sent = sendto(m_sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
                      reinterpret_cast<sockaddr*>(&sa), sizeof(sa));

    return sent != SOCKET_ERROR;
  }

  std::optional<Datagram> UDPSocket::recvFrom()
  {
    if (!m_open)
      return std::nullopt;

    uint8_t buf[4096];
    sockaddr_in from{};
    int fromLen = sizeof(from);

    int received = recvfrom(m_sock, reinterpret_cast<char*>(buf), sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);

    if (received == SOCKET_ERROR)
    {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK)
        return std::nullopt; // No data available (non-blocking)
      DebugTrace("recvfrom() error: {}\n", err);
      return std::nullopt;
    }

    if (received <= 0)
      return std::nullopt;

    char ipBuf[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &from.sin_addr, ipBuf, sizeof(ipBuf));

    Datagram dg;
    dg.data.assign(buf, buf + received);
    dg.senderAddr = ipBuf;
    dg.senderPort = ntohs(from.sin_port);
    return dg;
  }

  uint16_t UDPSocket::localPort() const
  {
    if (!m_open)
      return 0;

    sockaddr_in sa{};
    int len = sizeof(sa);
    if (getsockname(m_sock, reinterpret_cast<sockaddr*>(&sa), &len) == SOCKET_ERROR)
      return 0;

    return ntohs(sa.sin_port);
  }

  void UDPSocket::close()
  {
    if (m_open)
    {
      closesocket(m_sock);
      m_sock = INVALID_SOCK;
      m_open = false;
    }
  }
} // namespace Neuron
