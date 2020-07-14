.. meta::
   :description: Tarm-io library design overview

.. role:: cpp_inline(code)
   :language: cpp

Design overview
===============

In this document we give high level look on the tarm-io library.

Design principles
-----------------

The main design principle is **simplicity**.

   Instead of documenting all the possible details of functioning of a certain system (which is difficult),
   it is much easier to build a system according to a predetermined and documented set of principles.
   And remember only them.

   -- Alexander Voitenko

Simple things are hard to make. Networks topic is complicated.
It is generally hard to develop high performance distributed applications.
And tarm-io library manages to hide some complexity of event-driven input-output handling.

The second principle is **throughput over performance**.
This library may not fit in high level demands of low latency applications like automatic trade robots.
And the main goal is to serve as much request per second as possible.
From the author's experience most such applications are built on top of raw sockets to be able fine-tune networking stack.
Or very customized self-developed libraries which are not suitable for generic needs.

Library **does not use exceptions** to handle errors.
Instead error objects are returned or passed as a parameter to callbacks.
More on this in :ref:`error handling <error_handling:Error handling>` section.

Networking protocols are very different from each other and serve different needs,
but the library tries to frame **generic classes structure** for all supported protocols and input-output primitives.

Some platforms have unavoidable differences and limitations which make harder to develop cross-platform applications.
Almost all such **differences are documented**.

Tarm-io library is implemented in relatively old C++11, compatible with old versions of OpenSSL and libuv and
use not the most recent cMake version to achieve **maximum portability**.

**Namespaces structure repeats source files structure.**
This allows easily navigate project via source tree and find required items without constant usage of search.
Also this approach provide a way to see if some namespace became to overloaded with types.
And additionally it provides intuitive understanding of includes.
For example if you know that :cpp:expr:`TcpServer` resides in :cpp:expr:`tarm::io::net` namespace,
this means that :cpp_inline:`<tarm/io/net/TcpServer.h>` file should be included to access the code.

Basic concepts
--------------

It's all about event loop
~~~~~~~~~~~~~~~~~~~~~~~~~

What is event loop? Event loop is an internal cycle around which your application execution is built.
As you can see from the name, this kind of cycle works with events.
And the word "loop" tells us that it works with events repeatedly.
Events could be originated from the system (e.g. network packet is arrived) or a user (like scheduled callback).
One thing that you should keep in mind that loop continues to execute only until it expects something to be processed. Otherwise it exits.