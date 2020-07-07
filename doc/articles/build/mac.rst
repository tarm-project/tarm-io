.. meta::
   :description: Tarm-io build instructions for Mac

.. role:: bash(code)
   :language: bash

Build for Mac
=============

CMake on Mac supports various generators. At least Make, Ninja and Xсode are widely used. 
If you just need to build and install the library, Make will do the job best.
Note that you'll need Xcode command line tools installed.
In most cases to get them execute in terminal:

.. code-block:: bash

   $ xcode-select --install

The simplest way to get required dependencies on Mac is to install package manager like
`Homebrew <https://brew.sh/index>`_. 

Install dependencies
--------------------

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

If installing via Homebrew OpenSSL can not be found by CMake without specifying direct path to the root directory.
On my machine it is :bash:`/usr/local/opt/openssl@1.1`.

Or... it is not so hard to build it from source:

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