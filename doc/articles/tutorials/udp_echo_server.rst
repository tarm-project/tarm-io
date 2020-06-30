.. meta::
   :description: Tarm-io UDP echo server tutorial

UDP echo server
===============

In this small tutorial we create UDP echo server, which replies back received messages.
Create 2 following files CMakeLists.txt and main.cpp.


.. literalinclude:: ../../../examples/udp_echo_server/CMakeLists.txt
   :caption: CMakeLists.txt
   :language: cmake
   :linenos:


.. literalinclude:: ../../../examples/udp_echo_server/main.cpp
   :caption: main.cpp
   :language: c++
   :linenos:
   :lines: 8-

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
