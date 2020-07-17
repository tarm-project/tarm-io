.. meta::
   :description: Tarm-io build instructions for Mac

.. role:: bash(code)
   :language: bash

Build for Mac
=============

CMake on Mac supports various generators. At least Make, Ninja and Xсode are widely used. 
If you just need to build and install the library, "Unix Makefiles" generator will do the job best.
Note that you'll need Xcode command line tools installed.
In most cases to get them execute in terminal:

.. code-block:: bash

   $ xcode-select --install

Install dependencies
--------------------

The simplest way to get required dependencies on Mac is to install package manager like
`Homebrew <https://brew.sh/index>`_.

CMake
~~~~~

You can download CMake dmg installer from the `official site <https://cmake.org/download/#latest>`_.
Or just run to install:

.. code-block:: bash
   :caption: Homebrew

   $ brew install cmake


OpenSSL (optional)
~~~~~~~~~~~~~~~~~~

If you need support of secure protocols like TLS or DTLS you need to install OpenSSL development package.

.. code-block:: bash
   :caption: Homebrew

   $ brew install openssl@1.1

If installing via Homebrew, OpenSSL can not be found by CMake without specifying direct path to the root directory.
See `FindOpenSSL <https://cmake.org/cmake/help/v3.6/module/FindOpenSSL.html#hints>`_ for details.
On my Mac the path is :bash:`/usr/local/opt/openssl@1.1`.

Alternatively OpenSSL could be built from source:

.. TODO: what about certificates in this case??? Get from system????

.. code-block:: bash
   :caption: Build from source

   $ pushd /tmp
   $ curl -L -O https://www.openssl.org/source/openssl-1.1.1g.tar.gz
   $ tar -xf openssl-1.1.1g.tar.gz 
   $ cd openssl-1.1.1g
   $ ./config --prefix=/opt/openssl-1.1
   $ make -j4
   $ sudo make install
   $ popd

.. note::
   Recommended prefix is /usr/local as CMake is able to find OpenSSL there without additional paths settings.

Build and install (Make)
------------------------

Build is performed in separate directory:

.. code-block:: bash

   $ mkdir -p build
   $ cd build
   $ cmake -DCMAKE_BUILD_TYPE=Release ..
   $ make -j $(sysctl hw.physicalcpu | sed 's/hw.physicalcpu: //') # Getting number of CPU cores
   $ sudo make install

Build and install (Xcode)
-------------------------

.. code-block:: bash

   $ mkdir -p build
   $ cd build

CMake 3.15 and above
~~~~~~~~~~~~~~~~~~~~

If you have CMake 3.15+ (which is recommended) you may not specify 
`CMAKE_INSTALL_PREFIX <https://cmake.org/cmake/help/v3.16/variable/CMAKE_INSTALL_PREFIX.html>`_ during CMake configuration step.
Prefix will be defined later during installation step and could be changed without CMake reconfiguration.

.. code-block:: bash
   
   $ cmake -GXcode ..

And to build and install execute:

.. code-block:: bash

   cmake --build . --config Release -j4
   cmake --install . --config Release --prefix /some/path
   cmake --build . --config Debug -j4
   cmake --install . --config Debug --prefix /some/path

Debug and release builds reside in the same prefix folder.
CMake allow automatically to select required one when dependent code is built.
Note, debug build steps may be skipped if you do not need one.

.. note::
   Do not forget to replace '/some/path' with your own installation path.

CMake prior to 3.15
~~~~~~~~~~~~~~~~~~~

Older CMake versions require to specify 
`CMAKE_INSTALL_PREFIX <https://cmake.org/cmake/help/v3.16/variable/CMAKE_INSTALL_PREFIX.html>`_
during CMake configuration step.

.. code-block:: bash
   
   cmake -GXcode -DCMAKE_INSTALL_PREFIX="/some/path" ..

And to build and install execute:

.. code-block:: bash

   cmake --build . --config Release -j4
   cmake --build . --config Release --target install
   cmake --build . --config Debug -j4
   cmake --build . --config Debug --target install

Installation layout
~~~~~~~~~~~~~~~~~~~

Library follows UNIX structure of destination folders. Below is truncated example.

.. code-block:: bash

   .
   |-- bin
   |-- include
   |   `-- tarm
   |       `-- io
   |           |-- BacklogWithTimeout.h
   |           |-- ByteSwap.h
   |           |   ...
   |           |-- UserDataHolder.h
   |           |-- fs
   |           |   |-- Dir.h
   |           |   |-- DirectoryEntryType.h
   |           |   ...
   |           |-- global
   |           |   `-- Configuration.h
   |           `-- net
   |               |-- BufferSizeResult.h
   |               |-- Endpoint.h
   |               ...
   `-- lib
      |-- cmake
      |   |-- tarm-io
      |   |-- tarm-ioConfig.cmake
      |   |-- tarm-ioConfigVersion.cmake
      |   |-- tarm-ioTargets-debug.cmake
      |   |-- tarm-ioTargets-release.cmake
      |   `-- tarm-ioTargets.cmake
      |-- libtarm-io.dylib
      `-- libtarm-io_d.dylib

Build examples
--------------

.. include:: ../../common_chunks/unix_build_examples.rst