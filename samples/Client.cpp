#include "io/Common.h"

#include "io/EventLoop.h"
#include "io/TcpClient.h"
#include "io/TlsTcpClient.h"

#include <iostream>

int main(int argc, char* argv[]) {
    io::EventLoop loop;
    auto client = new io::TlsTcpClient(loop);

    client->connect(argv[1], 443,
        [](io::TlsTcpClient& client, const io::Error& error) {
            if (error) {
                std::cerr << error.string() << std::endl;
                return;
            }

            std::cout << "Connected!" << std::endl;
            client.send_data("GET / HTTP/1.0\r\n\r\n");
        },
        [](io::TlsTcpClient& client, const io::DataChunk& data, const io::Error& error) {
            if (error) {
                std::cerr << error.string() << std::endl;
                return;
            }

            std::cout.write(data.buf.get(), data.size);
        },
        [](io::TlsTcpClient& client, const io::Error& error) {
            if (error) {
                std::cerr << error.string() << std::endl;
            }

            client.schedule_removal();
        });

    return loop.run();
}
