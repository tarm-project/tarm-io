/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

// Important notice: if you change this code, ensure that docs are displaying it correctly,
//                   because lines positions are important.

#include <tarm/io/UdpServer.h>

#include <iostream>

using namespace tarm;

int main() {
    io::EventLoop loop;

    const std::uint16_t port = 1234;

    // TODO: smart pointer will do the job in case of error
    auto server = new io::net::UdpServer(loop);
    auto listen_error = server->start_receive(
        {"0.0.0.0", port},
        [&](io::net::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            if (error) {
                std::cerr << "Data receive error: " << error << std::endl;
                return;
            }
            peer.send_data(std::string(data.buf.get(), data.size));
        }
    );

    if (listen_error) {
        std::cerr << "Listen error: " << listen_error << std::endl;
        server->schedule_removal();
        return 1;
    }

    std::cout << "Started listening on port " << port << std::endl;

    const auto signal_handler_error = loop.handle_signal_once(
        io::EventLoop::Signal::INT,
        [&](io::EventLoop&, const io::Error& error) {
            if (error) {
                std::cerr << "Signal handling error: " << error << std::endl;
                return;
            }

            std::cout << std::endl << "Shutting down..." << std::endl;
            server->schedule_removal();
        }
    );

    if (signal_handler_error) {
        std::cerr << "Signal handling error: " << signal_handler_error << std::endl;
        server->schedule_removal();
        return 1;
    }

    return loop.run();
}
