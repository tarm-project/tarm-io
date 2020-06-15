.. meta::
   :description: Tarm-io EventLoop class reference

EventLoop
=========

What is event loop? Event loop is an internal cycle around which your application execution is built. As you can see from the name, this kind of cycle works with events. And the word "loop" tels us that it works with events repeatedly. Events could be originated from the system (e.g. network packet is arrived) or a user (like scheduled callback). One thing that you should keep in mind that loop continues to execute only until it expects something to be processed. Otherwise it exits.

.. TODO: example nothing to process immediate exit

.. TODO: example, schedule callback and exit

.. TODO: example, schedule long timer, blocks for a while

.. TODO: example, schedule TCP listen, blocks forever

.. TODO: Event Lopp stages fugure (like for libUV)

.. TODO: CPU consumptions and timers