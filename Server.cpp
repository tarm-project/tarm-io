#include "io/EventLoop.h"
#include "io/TcpServer.h"
#include "io/Timer.h"
#include "io/File.h"

#include <iostream>
#include <cstring>

int main(int argc, char* argv[]) {
    io::EventLoop loop;
    io::TcpServer server(loop);

    io::File file(loop);

    //io::Timer timer(loop);
    //timer.start([&](Timer& t){ std::cout << "Timer!!!" << std::endl; timer.stop();}, 1000, 0);

    std::shared_ptr<char> write_data_buf(new char[io::File::READ_BUF_SIZE], std::default_delete<char[]>());

    auto on_new_connection = [&](const io::TcpServer& server, const io::TcpClient& client) -> bool {
        std::cout << "New connection from " << io::ip4_addr_to_string(client.ipv4_addr()) << ":" << client.port() << std::endl;

        // TODO: this is wrong!!!
        // TcpClient& client_ = const_cast<TcpClient&>(client);
        // std::memcpy(write_data_buf.get(), "Hello", 5);
        // client_.send_data(write_data_buf, 5);

        return true;
    };

    auto on_data_read = [&](const io::TcpServer&, const io::TcpClient& client, const char* data, size_t len) {
        std::string message(data, data + len);

        if (message.find("close") != std::string::npos ) {
            std::cout << "Sutting down server..." << std::endl;
            server.shutdown();
            //timer.stop();
        }

        if (message.find("open") != std::string::npos ) {
            auto pos = message.find("open");
            auto file_name = message.substr(5, message.size() - 5 - 1); // TODO: magic values and strings

            file.open(file_name, [&](io::File& file) {
                std::cout << "Opened file " << file.path() << std::endl;

                file.read([&](io::File& file, const char* buf, std::size_t size) {
                    // TODO: this is wrong!!!
                    io::TcpClient& client_ = const_cast<io::TcpClient&>(client);
                    std::memcpy(write_data_buf.get(), buf, size);
                    client_.send_data(write_data_buf, size);
                });
            });
        }

        std::cout << message;
    };

    auto status = server.bind("0.0.0.0", 1234);
    if (status != 0) {
        std::cerr << "[Server] Failed to bind. Reason: " << uv_strerror(status) << std::endl;
        return 1;
    }

    if ((status = server.listen(on_data_read, on_new_connection)) != 0) {
        std::cerr << "[Server] Failed to listen. Reason: " << uv_strerror(status) << std::endl;
        return 1;
    }

    return loop.run();
}
