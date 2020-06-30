.. meta::
   :description: Tarm-io EventLoop tutorial

Hello event loop
================

In this tutorial we output string to a console in event-driven way.
Also here basic structure of every application which uses tarm-io library is shown.
Most programs contain following steps:

1. EventLoop object creation
2. Adding some work to event loop
3. Starting to execute that work

.. literalinclude:: ../../../examples/hello_event_loop/main.cpp
   :caption: main.cpp
   :language: c++
   :linenos:
   :lines: 8-

This small program outputs string to a terminal and exits immediately
because schedule_callback() method adds a callback which is executed only once.
And when EventLoop has nothing to do, it exits.

Note form of passed callback. It has EventLoop reference as the parameter.
This allows to use the same set of callbacks with different objects and react differently basing on instance of that object.
You can read more about this in the :ref:`design overview <design_overview:Design overview>` section.

.. TODO: schedule 2 callbacks and order of execution mention