.. meta::
   :description: Tarm-io build instructions for Windows

Build for Windows
=================

As usual everything is a bit complicated for Windows.
We will go from the most basic scenarios to more complex topics below.
Most of the work is made here via command line. Code examples here suppose usage of Power Shell.
It has numerous advantages over classic cmd.

Both x86 and x64 builds are supported on both x86 and x64 hosts.
So you can compile and use 64 bit and 32 bit versions of the library on x64 machine.

Install dependencies
--------------------

Visual Studio
~~~~~~~~~~~~~

First, you'll need build tools on your machine if you don't have one.
You can always download and use community edition of Visual Studio from the `Microsoft website <https://visualstudio.microsoft.com/vs/older-downloads/>`_.

During installation do not forget to enable C++ tools for desktop.

.. TODO: screenshot

CMake
~~~~~

Download `CMake <https://cmake.org/download/#latest>`_ installer for your platform.
During installation it is highly recommended to add **cmake** executable in the PATH,
because all following instructions assume that. Of course everything could be done via GUI.

.. image:: windows_cmake_install.png
   :scale: 50%
   :alt: Windows cmake install options

Configure and build
-------------------

Build is performed in separate directory:

.. code-block:: bash

   mkdir build
   cd build

Note that unlike Linux or Mac OS builds with make files you do not need to pass
`CMAKE_BUILD_TYPE <https://cmake.org/cmake/help/v3.15/variable/CMAKE_BUILD_TYPE.html>`_
parameter during configuration.

CMake 3.15 and above
~~~~~~~~~~~~~~~~~~~~

If you have CMake 3.15+ (which is recommended) you may not specify 
`CMAKE_INSTALL_PREFIX <https://cmake.org/cmake/help/v3.16/variable/CMAKE_INSTALL_PREFIX.html>`_ during CMake configuration step.
Prefix will be defined later during installation step and could be changed without CMake reconfiguration.

.. code-block:: bash
   :caption: x64
   
   cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 -DCMAKE_GENERATOR_PLATFORM=x64 ..

or

.. code-block:: bash
   :caption: x86
   
   cmake -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x86 -DCMAKE_GENERATOR_PLATFORM=Win32 ..

And to build and install execute:

.. code-block:: bash

   cmake --build . --config Release -j4
   cmake --install . --config Release --prefix X:\tarm-io-install
   cmake --build . --config Debug -j4
   cmake --install . --config Debug --prefix X:\tarm-io-install

Debug and release builds reside in the same prefix folder.
CMake allow automatically to select required one when dependent code is built.
Note, debug build steps may be skipped if you do not need one.

.. note::
   Do not forget to replace 'X:\\tarm-io-install' with your own installation path.

.. error::
   x86 and x64 platform builds should be installed in separate folders.

CMake prior to 3.15
~~~~~~~~~~~~~~~~~~~

Older CMake versions require to specify `CMAKE_INSTALL_PREFIX <https://cmake.org/cmake/help/v3.16/variable/CMAKE_INSTALL_PREFIX.html>`_
during CMake configuration step.

.. code-block:: bash
   :caption: x64
   
   cmake -DCMAKE_INSTALL_PREFIX=X:\tarm-io-install `
         -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x64 `
         -DCMAKE_GENERATOR_PLATFORM=x64 `
         ..

or

.. code-block:: bash
   :caption: x86
   
   cmake -DCMAKE_INSTALL_PREFIX=X:\tarm-io-install `
         -DCMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE=x86 `
         -DCMAKE_GENERATOR_PLATFORM=Win32 `
         ..

And to build and install execute:

.. code-block:: bash

   cmake --build . --config Release -j4
   cmake --build . --config Release --target install
   cmake --build . --config Debug -j4
   cmake --build . --config Debug --target install

.. TODO: installation layout description?

Build examples
--------------

Examples could be found in the project root 'examples' folder.
As usual, examples are built in a separate folder.

.. code-block:: bash

   mkdir build
   cd build

Examples require library built and installed in some folder.
During configuration step of examples tarm-io library is searched using CMake routines.
As usual this a bit complicated on Windows.
The simplest way to find library is to install it to some system-related folder like "Program Files"
or in folder that is referenced in some system-related environment variable like PATH or INCLUDE.
For more details read `find_library <https://cmake.org/cmake/help/latest/command/find_library.html>`_
command description.

Modification of PATH also may look like:

.. code-block:: bash

   $env:Path += ";X:\tarm-io-install"
   cmake ..

Another approach is to set 'tarm-io_DIR' variable during CMake configuration.
It requires full path to CMake config subfolders in the installation prefix.

.. code-block:: bash

   cmake -Dtarm-io_DIR="X:\tarm-io-install\lib\cmake\tarm-io" ..

And one more approach is to define
`CMAKE_PREFIX_PATH <https://cmake.org/cmake/help/latest/variable/CMAKE_PREFIX_PATH.html>`_

.. code-block:: bash

   cmake -DCMAKE_PREFIX_PATH=X:\tarm-io-install ..