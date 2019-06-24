#include "io/Common.h"

#include "io/EventLoop.h"
#include "io/TcpClient.h"

#include <iostream>

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
    client->connect(argv[1], std::atoi(argv[2]), [](io::TcpClient& client, const io::Status& status) {
        std::cout << "Connected!!!" << std::endl;
        client.send_data("Hello world!\n", [](io::TcpClient& client) {
            std::cout << "Data sent!!!" << std::endl;

            client.schedule_removal();
        });
    },
    nullptr);

    return loop.run();
}
