.. meta::
   :description: Tarm-io build instructions

Build instructions
==================
Each platfrom's build instructions page has description how to build and install dependencies.

Build for platforms
-------------------
.. toctree::
   :maxdepth: 1

   articles/build/windows
   articles/build/linux
   articles/build/mac


Build system requirements
--------------------------

C++11 compliant compiler.

`CMake <https://cmake.org/download/>`_ at least version 3.5. But the latest version is usually better. All popular generators are supported.

Dependencies
------------
Tarm-io library has very few optional dependencies.

Runtime dependencies
~~~~~~~~~~~~~~~~~~~~

.. TODO: revise this text if Microsoft secure channels will be supported.

* `libuv <https://github.com/libuv/libuv/releases>`_ (bundled) at least version 1.0.0. The Tarm-io library **has bundled libuv** sources and by default use them during the build.
* `OpenSSL <https://www.openssl.org/source/>`_ (optional) at least version 1.0.0. Version 1.0.2 and greater is recommended. Without OpenSSL TLS and DTLS networking features will not be available.


Tests dependencies
~~~~~~~~~~~~~~~~~~

If you want to build and run tests, you need following libraries:

* `Boost <https://www.boost.org/users/download/>`_ of any recent version. Minimal tested is 1.55.
* `GTest <https://github.com/google/googletest>`_ at least 1.8.0. Version 1.10.0 is recommended.

