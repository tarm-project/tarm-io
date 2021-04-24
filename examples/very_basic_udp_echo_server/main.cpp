/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

// Important notice: if you change this code, ensure that docs are displaying it correctly,
//                   because lines positions are important.

#include <tarm/io/net/UdpServer.h>
using namespace tarm;

int main() {
    io::EventLoop loop;

    auto server = loop.allocate<io::net::UdpServer>();
    server->start_receive({"0.0.0.0", 1234},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error&) {
            peer.send_data(std::string(data.buf.get(), data.size));
        }
    );

    return loop.run();
}
