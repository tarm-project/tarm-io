.. meta::
   :description: Tarm-io EventLoop class reference

:cpp:class:`EventLoop`
======================

.. cpp:class:: EventLoop

   Event loop class. Response for all events handling.
   Usually created one per thread.
   The only class which is able to create on stack.

Members
=======

Signals
~~~~~~~

The library provides handling of very few signals.
The main reason for this is inability to provide cross-platform behavior and
additionally very strict limitations for some Unix signal handlers.

.. cpp:enum-class:: Signal

   .. cpp:enumerator:: INT

   .. cpp:enumerator:: HUP

   .. cpp:enumerator:: WINCH

.. cpp:type:: SignalCallback = std::function<void(EventLoop&, const Error&)>

   Signal handler callback type.

.. cpp:function:: Error handle_signal_once(Signal signal, const SignalCallback& callback)

   Handle signal once and remove handle.

.. cpp:function:: Error add_signal_handler(Signal signal, const SignalCallback& callback)

   Handle signals multiple times. Note that while this handler is present event will not exit.

.. cpp:function::void remove_signal_handler(Signal signal)

   Remove signal handler. If handler was not registered, do nothing.