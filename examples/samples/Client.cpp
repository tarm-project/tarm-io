/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "io/EventLoop.h"
#include "io/TcpClient.h"
#include "io/TlsTcpClient.h"

#include <iostream>

using namespace tarm;

int main(int argc, char* argv[]) {
    io::EventLoop loop;
    auto client = new io::net::TlsClient(loop);

    client->connect({argv[1], 443},
        [](io::net::TlsClient& client, const io::Error& error) {
            if (error) {
                std::cerr << error.string() << std::endl;
                return;
            }

            std::cout << "Connected!" << std::endl;
            client.send_data("GET / HTTP/1.0\r\n\r\n");
        },
        [](io::net::TlsClient& client, const io::DataChunk& data, const io::Error& error) {
            if (error) {
                std::cerr << error.string() << std::endl;
                return;
            }

            std::cout.write(data.buf.get(), data.size);
        },
        [](io::net::TlsClient& client, const io::Error& error) {
            if (error) {
                std::cerr << error.string() << std::endl;
            }

            client.schedule_removal();
        });

    return loop.run();
}
