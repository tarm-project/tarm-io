.. tarm-io documentation master file, created by
   sphinx-quickstart on Fri May 15 20:28:30 2020.

.. meta::
   :yandex-verification: 2ee209155a2548b6
   :google-site-verification: uj3is7vssI50kvq-LLgzvEX5ls-MjbhtOg-zRPOrlHI
   :Description: Cross-platform event-driven C++11 input-output library

Welcome to tarm-io's documentation!
===================================
.. warning::
        The library is under heavy development, ETA is Q1 or Q2 2021.

Overview
--------

Tarm-io is a cross-platform general purpose event-driven C++11 input-output library project.

Features
--------
* Full-featured event loop backed by epoll, kqueue, IOCP, event ports.
* Asynchronous networking (TCP, UDP, TLS, DTLS protocols and more)
* IPv4 and IPv6
* Support of various SSL backend libraries
* Asynchronous DNS resolution
* No sockets. Instead library provides higher level abstractions like clients and servers
* Asynchronous file and file system operations
* Paths constructions and manipulations
* File system events
* Thread pool
* Signal handling
* Embedded data producer or consumer pacing
* Guaranteed and auto-tested ABI compatibility between minor versions

Supported platforms
-------------------

* Windows (Visual Studio 2015+)
* Linux (Ubuntu 14.04+, Cent OS 7.3+, Alpine and more)
* Mac (10.10+)
* iOS
* Android

Downloads
---------

Tarm-io can be downloaded from `github page <https://github.com/tarm-project/tarm-io>`_.

Showcase
--------
In this small tutorial we create UDP echo server, which replies back received messages.
Ensure that library is :ref:`built and installed <build_instructions_index:Build instructions>`.
Create 2 following files CMakeLists.txt and main.cpp.

.. code-block:: cmake
   :linenos:
   :caption: CMakeLists.txt

   cmake_minimum_required(VERSION 3.5.0)

   project(tutorial VERSION 1.0.0 LANGUAGES C CXX)

   add_executable(tutorial main.cpp)

   find_package(tarm-io REQUIRED)
   target_link_libraries(tutorial tarm-io::tarm-io)


.. code-block:: c++
   :linenos:
   :caption: main.cpp

   #include <tarm/io/UdpServer.h>

   #include <iostream>

   using namespace tarm;

   int main() {
      io::EventLoop loop;

      auto server = new io::UdpServer(loop);
      auto listen_error = server->start_receive(
         {"0.0.0.0", 1234},
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

      // TODO: signal handler and server deletion
      return loop.run();
   }

Build example:

.. code-block:: bash

   $ mkdir build
   $ cd build
   $ cmake ..
   $ cmake --build .

.. warning::
   TODO: windows search path instruction

And execute server application. As a client you may use utility like *netcat* in Linux:

.. code-block:: bash

   $ netcat -u 127.0.0.1 1234
   Hello!
   Hello!


Documentation
-------------

.. toctree::
   :maxdepth: 1

   build_instructions_index



.. Subsection test
   ~~~~~~~~~~~~~~~
   Level 3 text!

Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
