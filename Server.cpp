#include "io/EventLoop.h"
#include "io/TcpServer.h"
#include "io/Timer.h"
#include "io/File.h"
#include "io/Dir.h"

#include <iostream>
#include <cstring>


int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    io::EventLoop loop;
    io::TcpServer server(loop);

    //io::Dir dir(loop);
    //dir.open("/Users/tarm", [&](io::Dir& dir) {
    //    std::cout << "Opened dir: " << dir.path() << std::endl;
    //    dir.read([&] (io::Dir& dir, const char* path, io::DirectoryEntryType type) {
    //        std::cout << type << " " << path << std::endl;
    //    });
    //});

    //io::Timer timer(loop);
    //timer.start([&](Timer& t){ std::cout << "Timer!!!" << std::endl; timer.stop();}, 1000, 0);

    // auto my_file_ptr = new io::File(loop);
    // my_file_ptr->open("/home/tarm/work/source/uv_cpp/build/3", [](io::File& file) {
    //     std::cout << "Opened file: " << file.path() << std::endl;
    //     file.read([](io::File& file, const char* buf, std::size_t size) {
    //         //file.close();
    //         file.schedule_removal();
    //     });
    // });

    auto on_new_connection = [](io::TcpServer& server, io::TcpClient& client) -> bool {
        std::cout << "New connection from " << io::ip4_addr_to_string(client.ipv4_addr()) << ":" << client.port() << std::endl;

        // std::memcpy(write_data_buf.get(), "Hello", 5);
        // client_.send_data(write_data_buf, 5);

        return true;
    };

    auto on_data_read = [&loop](io::TcpServer& server, io::TcpClient& client, const char* data, size_t len) {
        std::string message(data, data + len);
        std::cout << message;

        if (message.find("close") != std::string::npos ) {
            std::cout << "Forcing server shut down..." << std::endl;
            server.shutdown();
            //timer.stop();
        }

        if (message.find("open") != std::string::npos ) {
            int sub_size = 0;
            if (message.size() >= 1 && (message.back() == '\r' || message.back() == '\n')) {
                ++sub_size;
            }

            if (message.size() >= 2 && (message[message.size() - 2] == '\r' || message[message.size() - 2] == '\n')) {
                ++sub_size;
            }

            auto file_name = message.substr(5, message.size() - 5 - sub_size); // TODO: magic values
            auto file_ptr = new io::File(loop);

            file_ptr->open(file_name, [&client](io::File& file) {
                std::cout << "Opened file: " << file.path() << std::endl;
                client.set_close_callback([&file](io::TcpClient& client){
                    //file.close();
                    file.schedule_removal();
                });

                file.read([&client](io::File& file, const char* buf, std::size_t size) {
                    if (client.pending_write_requesets() > 0) {
                        std::cout << "pending_write_requesets " << client.pending_write_requesets() << std::endl;
                    }

                    client.send_data(buf, size, [&file](io::TcpClient& client) {
                        static int counter = 0;
                        std::cout << "TcpClient after send counter: " << counter++ << std::endl;

                        if (file.read_state() == io::File::ReadState::PAUSE) {
                            std::cout << "Continue read" << std::endl;
                            file.continue_read();
                        }
                    });

                    if (client.pending_write_requesets() > 1) {
                        std::cout << "Pause read" << std::endl;
                        file.pause_read();
                    }
                },
                [&client](io::File& file){
                    client.set_close_callback(nullptr);
                    //file.close();
                    file.schedule_removal();
                });
            });
        }
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
    std::cout << "Shutting down..." << std::endl;
}
