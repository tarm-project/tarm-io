/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#pragma once

// Forward declarations of all user-level types
namespace tarm {
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
} // namespace tarm
