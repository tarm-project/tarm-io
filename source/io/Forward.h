/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

// Forward declarations of all user-level types
namespace tarm {
namespace io {
namespace net {

class TcpServer;
class TcpConnectedClient;
class TcpClient;

class TlsServer;
class TlsConnectedClient;
class TlsClient;

class DtlsServer;
class DtlsConnectedClient;
class DtlsClient;

class UdpClient;
class UdpServer;
class UdpPeer;

} // namespace net

class RefCounted;
class Removable;

// ------------------------------- private details -------------------------------

namespace net {
namespace detail {

struct PeerId;

} // namespace detail
} // namespace net

} // namespace io
} // namespace tarm
