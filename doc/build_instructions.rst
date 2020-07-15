.. meta::
   :description: Tarm-io build instructions

Build instructions
==================
Each platfrom's build instructions page has description how to build and install dependencies.

If you are familar with Docker, the fastest way to get started and play around with the library is to use existing `tarmio builder <https://hub.docker.com/r/tarmio/builders/tags>`_ images.

Build for platforms
-------------------
.. toctree::
   :maxdepth: 1

   articles/build/windows
   articles/build/linux
   articles/build/mac
   articles/build/docker


Build system requirements
--------------------------

C++11 compliant compiler.

`CMake <https://cmake.org/download/>`_ at least version 3.5. But the latest version is usually better. All popular generators are supported.

Dependencies
------------

Tarm-io library has very few optional dependencies. And in minimal configuration could be built out of the box.

Runtime dependencies
~~~~~~~~~~~~~~~~~~~~

.. TODO: revise this text if Microsoft secure channels will be supported.

* `libuv <https://github.com/libuv/libuv/releases>`_ (bundled) at least version 1.0.0. The Tarm-io library **has bundled libuv** sources and by default use them during the build.
* `OpenSSL <https://www.openssl.org/source/>`_ (optional) at least version 1.0.0. Version 1.0.2 and greater is recommended. Without OpenSSL TLS and DTLS networking features will not be available.


Tests dependencies
~~~~~~~~~~~~~~~~~~

If you want to build and run tests, you need following libraries:

* `Boost <https://www.boost.org/users/download/>`_ of any recent version. Minimal tested is 1.55.
* `GTest <https://github.com/google/googletest>`_ (bundled) at least 1.8.0. Version 1.10.0+ is recommended.

Useful CMake variables
----------------------

CMake configuration variables below could be used to customize build.

Common variables
~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 20 5 30
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - CMAKE_BUILD_TYPE
     - Enum
     - Build type. Possible values are: Debug, Release, RelWithDebInfo and MinSizeRel
   * - CMAKE_INSTALL_PREFIX
     - Path
     - Installation directory path
   * - BUILD_SHARED_LIBS
     - Bool
     - Build libtarm-io as shared or static library
   * - **TARM_IO_BUILD_TESTS**
     - Bool
     - Build tests for tarm-io
   * - **TARM_IO_USE_EXTERNAL_LIBUV**
     - Bool
     - Use system libuv instead of the bundled one
   * - **TARM_IO_USE_EXTERNAL_GTEST**
     - Bool
     - Use system GTest instead of the bundled one
   * - OPENSSL_ROOT_DIR
     - Path
     - Root directory of an OpenSSL installation
   * - OPENSSL_USE_STATIC_LIBS bool
     - Bool
     - Search for static OpenSSL libraries
   * - OPENSSL_MSVC_STATIC_RT
     - Bool
     - Choose the MT (static runtime) version of the OpenSSL
   * - GTEST_ROOT
     - Path
     - The root directory of the Google Test installation
   * - BOOST_ROOT
     - Path
     - The root directory of the Boost library installation
   * - Boost_USE_MULTITHREADED
     - Bool
     - Set to OFF to use the non-multithreaded libraries ('mt' tag).  Default is ON.
   * - Boost_USE_STATIC_LIBS
     - Bool
     - Set to ON to force the use of the static libraries.  Default is OFF.
   * - Boost_USE_STATIC_RUNTIME
     - Bool
     - Set to ON or OFF to specify whether to use libraries linked statically to the C++ runtime('s' tag).  Default is platform dependent.
   * - Boost_USE_DEBUG_RUNTIME
     - Bool
     - Set to ON or OFF to specify whether to use libraries linked to the MS debug C++ runtime('g' tag).  Default is ON.

Windows variables
~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 23 4 15
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE
     - Enum
     - Visual Studio preferred tool architecture. Possible values are: x86 or x64
   * - CMAKE_GENERATOR_PLATFORM
     - Enum
     - Generator-specific target platform. Possible values are: Win32, x64, ARM, ARM64

Mac OS variables
~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 40 15 60
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - CMAKE_OSX_DEPLOYMENT_TARGET
     - Version
     - Deployment target for Mac OS X. Example: "10.13"

.. raw:: html

   <script type="text/javascript">tarm_io_set_table_tr_labels('table')</script>