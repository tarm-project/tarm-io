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
            std::cout << "Connected!" << std::endl;
            client.send_data("GET / HTTP/1.0\r\n\r\n");
        },
        [](io::TlsTcpClient& client, const char* buf, size_t size) {
            std::cout.write(buf, size);
        },
        [](io::TlsTcpClient& client, const io::Error& error) {
            client.schedule_removal();
        });

    return loop.run();
}

/*
void print_usage(const std::string& app) {
    std::cerr << app << " <ip addr> <port>" << std::endl;
}

int main(int argc, char* argv[]) {
    io::EventLoop loop;

    if (argc <= 2) {
        print_usage(argv[0]);
        return 1;
    }

    auto client = new io::TcpClient(loop);
    client->connect(argv[1], std::atoi(argv[2]), [](io::TcpClient& client, const io::Error& error) {
        std::cout << "Connected!!!" << std::endl;
        client.send_data("Hello world!\n", [](io::TcpClient& client, const io::Error& error) {
            // TODO: check status
            std::cout << "Data sent!!!" << std::endl;

            client.schedule_removal();
        });
    },
    nullptr);

    return loop.run();
}
*/
