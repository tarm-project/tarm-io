#pragma once

// Forward declarations of all user-level types
namespace io {

class TcpServer;
class TcpConnectedClient;
class TcpClient;

class TlsTcpServer;
class TlsTcpConnectedClient;
class TlsTcpClient;

class DtlsServer;
class DtlsConnectedClient;
class DtlsClient;

class UdpClient;
class UdpServer;
class UdpPeer;

class RefCounted;
class Removable;

// ------------------------------- private details -------------------------------

namespace detail {

struct PeerId;

} // namespace detail

} // namespace io
