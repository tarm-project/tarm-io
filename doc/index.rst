.. tarm-io documentation master file, created by
   sphinx-quickstart on Fri May 15 20:28:30 2020.

.. meta::
   :yandex-verification: 2ee209155a2548b6
   :google-site-verification: uj3is7vssI50kvq-LLgzvEX5ls-MjbhtOg-zRPOrlHI
   :description: Cross-platform event-driven C++11 input-output library

Welcome to tarm-io's documentation!
===================================
.. warning::
   The library is under development, ETA is somewhere in 2021.

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
* Guaranteed and auto-tested ABI compatibility between minor and patch versions

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

Documentation
-------------

.. toctree::
   :maxdepth: 1

   build_instructions
   tutorials
   design_overview
   memory_management
   error_handling
   reference

Quick showcase
--------------

Tarm-io library leverages simplicity as its main design principle. The library hides a lot of networking complexity from user.
Just look at the example below. The very basic version of UDP echo server without error handling and memory management will look like this.

.. literalinclude:: ../examples/very_basic_udp_echo_server/main.cpp
   :caption: main.cpp
   :language: c++
   :linenos:
   :lines: 8-


For a complete example with explanations see :ref:`tutorials <tutorials:Tutorials>` section.

Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
