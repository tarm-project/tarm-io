.. meta::
   :description: Tarm-io build instructions for Docker

Build in Docker
===============

Get Docker
----------

If you've never used Docker before I would highly recommend to master this technology as it opens a lot of new possibilities for software development.
In short, docker is a platform for developers and sysadmins to build, run, and share applications with containers. You can read more at the `Docker website <https://docs.docker.com/get-started/>`_.

First `install docker <https://docs.docker.com/get-docker/>`_ for your system. As hosts Linux and Mac OS are supported.

Images
------

Only Linux images are currently supported. There are lot of different Linux distributions and versions are used to test Tarm-io library. Some of them are:

.. code-block:: bash

    tarmio/builders:ubuntu20.04-gcc9.3.0-boost1.71-cmake3.16.3-gtest1.10.0-openssl1.1.1g
    tarmio/builders:alpine3.9.6-gcc8.3.0-boost1.67-cmake3.13.0-gtest1.8.1-openssl1.1.1g
    tarmio/builders:centos7.6.1810-gcc4.8.5-boost1.70-cmake3.15.6-gtest1.8.1-openssl1.1.0

As you can see for image distinction tags (name after the ':' character) are used. Naming scheme is the following:

.. code-block:: bash

    <OS version> - <compiler version> - <boost library> - <cmake> - <google test> - <openssl>

More images you can always find at `Docker Hub <https://hub.docker.com/r/tarmio/builders/tags>`_.

Run container from an image
---------------------------

The library provide handful of scripts that greatly simplify work with docker images for you.
To start container execute:

.. code-block:: bash

    $ ./scripts/execute_in_docker.bash ubuntu20.04-gcc9.3.0-boost1.71-cmake3.16.3-gtest1.10.0-openssl1.1.1g

    ubuntu20.04-gcc9.3.0-boost1.71-cmake3.16.3-gtest1.10.0-openssl1.1.1g: Pulling from tarmio/builders
    Digest: sha256:a2a26a0ede78d4a93a2ad4c19a13547da46f966c091902417bcabd9a47d7a9fc
    Status: Image is up to date for tarmio/builders:ubuntu20.04-gcc9.3.0-boost1.71-cmake3.16.3-gtest1.10.0-openssl1.1.1g
    docker.io/tarmio/builders:ubuntu20.04-gcc9.3.0-boost1.71-cmake3.16.3-gtest1.10.0-openssl1.1.1g
    Running image tarmio/builders:ubuntu20.04-gcc9.3.0-boost1.71-cmake3.16.3-gtest1.10.0-openssl1.1.1g

    You are in docker now!
    tarm@Alexander-vm:/source$

.. note::
   When **execute_in_docker.bash** script is used, *tarmio/builders:* prefix could be omitted.

When you start docker container via script you get:

* Tarm-io source root directory bound to container's /source folder. So all changes made to sources  in container will affect actual source tree
* User which is compatible with the host one. So any changes are made on behalf of the host user
* Passwordless **sudo**, so you can install additional software easily
* Interactive terminal if supported
* Host network
* Other tweaks which allow to use docker comfortably

To exit container press Ctrl+D.

Build library and examples
--------------------------

Once you've got into container, building and installing Tarm-io library is straightforward.

.. code-block:: bash

   $ mkdir build
   $ cd build
   $ cmake ..
   $ make -j(nproc)
   $ sudo make install
   $ cd ../examples
   $ mkdir build
   $ cd build
   $ cmake ..
   $ make -j(nproc)
   $ ./udp_echo_server/udp_echo_server # Run one of the examples
   Started listening on port 1234

In other terminal window of the host you can connect to this server.
`Netcat <https://www.unixmen.com/play-with-netcat-in-ubuntu>`_ is recommended as a test client.
It is also available on Mac.

.. code-block:: bash

   $ nc -u 127.0.0.1 1234
   Hello!
   Hello!

.. note::
   Inside container complete Linux directory structure is available, but modification only of the */source* folder affects your host system.
