.. meta::
   :description: Tarm-io library Tutorials

Tutorials
=========

Before start please ensure that library is :ref:`built and installed <build_instructions:Build instructions>`.

Almost all tutorials consist of basically 2 files: main.cpp and CMakeLists.txt.
The tutorials and examples below demonstrate C++ code but do not expose CMake lists.
The main reason for this is that all CMake code for almost all samples is identical and basically looks like this.

.. literalinclude:: ../examples/hello_event_loop/CMakeLists.txt
   :caption: CMakeLists.txt
   :language: CMake
   :linenos:
   :lines: 11-19

Please note **find_package** call.
Tarm-io library defines set of `imported targets <https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#imported-targets>`_,
so if library is found, subsequent call to **target_link_libraries** will do the job with populating include directories, preprocessor definitions and setting appropriate linker flags.
The simplest way to find the library is to install it to some system-related folder like "Program Files"
on Windows or "/usr/local" on Linux and Mac OS.
Or install in folder that is referenced in some system-related environment variable like PATH or INCLUDE.
For more details and available options see 'build examples' section of :ref:`build instructions <build_instructions:Build instructions>` for your platform.

Note that on Windows dependency DLLs (tam-io itself and OpenSSL, if any)
should be installed to some system path or copied to an executable folder to be accessible during an application launch.
CMake code to copy DLLs may look like this.

.. raw:: html

   <details>
   <summary>Copy DLLs</summary>

.. literalinclude:: ../examples/hello_event_loop/CMakeLists.txt
   :language: CMake
   :lines: 20-

.. Note::
   Replace **hello_event_loop** everywhere with your own executable target.

.. raw:: html

   </details>

For more details see `Dynamic-Link Library Search Order <https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order>`_.

List of tutorials
-----------------

.. toctree::
   :maxdepth: 1

   articles/tutorials/hello_event_loop
   articles/tutorials/udp_echo_server