#include <tarm/io/UdpServer.h>

#include <iostream>

using namespace tarm;

int main() {
   io::EventLoop loop;

   const auto port = 1234;

   auto server = new io::UdpServer(loop);
   auto listen_error = server->start_receive(
      {"0.0.0.0", port},
      [&](io::UdpPeer& peer, const io::DataChunk& data, const io::Error& error) {
            if (error) {
               std::cerr << "Error: " << error << std::endl;
               return;
            }

            peer.send_data(std::string(data.buf.get(), data.size));
      }
   );

   if (listen_error) {
      std::cerr << "Error: " << listen_error << std::endl;
      return 1;
   }

   std::cout << "Started listening on port " << port << std::endl;

   // TODO: signal handler and server deletion
   return loop.run();
}
