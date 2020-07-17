.. meta::
   :description: Tarm-io build instructions for Linux

.. role:: bash(code)
   :language: bash

Build for Linux
===============

Install dependencies
--------------------

CMake
~~~~~

.. code-block:: bash
   :caption: Ubuntu

   $ sudo apt -y update
   $ sudo apt -y install cmake

.. code-block:: bash
   :caption: CentOS 8

   $ sudo yum install -y cmake

.. code-block:: bash
   :caption: CentOS 7

   # Building from source
   $ sudo yum -y groupinstall "Development Tools"
   $ sudo yum -y install wget
   $ pushd /tmp
   $ wget https://github.com/Kitware/CMake/releases/download/v3.15.6/cmake-3.15.6.tar.gz
   $ tar -xf cmake*
   $ cd cmake*
   $ ./configure --parallel=$(nproc)
   $ make -j $(nporc)
   $ sudo make install
   $ popd

 

OpenSSL (optional)
~~~~~~~~~~~~~~~~~~

If you need support of secure protocols like TLS or DTLS you need to install OpenSSL development package.

.. code-block:: bash
   :caption: Ubuntu

   $ sudo apt -y update
   $ sudo apt -y install libssl-dev

.. code-block:: bash
   :caption: CentOS

   $ sudo yum install libssl-devel

Build and install tarm-io library
---------------------------------

.. code-block:: bash

   $ mkdir -p build
   $ cd build
   $ cmake -DCMAKE_BUILD_TYPE=Release ..
   $ make -j $(nproc)
   $ sudo make install
   $ sudo ldconfig

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
      |-- libtarm-io.so
      `-- libtarm-io_d.so

Build examples
--------------

.. include:: ../../common_chunks/unix_build_examples.rst
