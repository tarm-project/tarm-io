Examples could be found in the project root *'examples'* folder.
As usual, examples are built in a separate directory.

.. code-block:: bash

   $ cd examples
   $ mkdir -p build
   $ cd build

Examples require library built and installed in some folder.
During configuration step of examples tarm-io library is searched using CMake routines.
The simplest way to find library is to install it to some system-related folder like :bash:`/usr/local`
or in folder that is referenced in some system-related environment variable like PATH or INCLUDE.
For more details read `find_library <https://cmake.org/cmake/help/latest/command/find_library.html>`_
command description.

Modification of PATH may look like:

.. code-block:: bash

   $ export PATH="${PATH}:/some/install/path"
   $ cmake ..

or 

.. code-block:: bash

   $ PATH="${PATH}:/some/install/path" cmake ..

Another approach is to set 'tarm-io_DIR' variable during CMake configuration.
It requires full path to CMake config subfolders in the installation prefix.
This means that supplied path should end with :bash:`lib/cmake/tarm-io`.

.. code-block:: bash

   $ cmake -Dtarm-io_DIR="/some/install/path/lib/cmake/tarm-io" ..

And one more approach is to define
`CMAKE_PREFIX_PATH <https://cmake.org/cmake/help/latest/variable/CMAKE_PREFIX_PATH.html>`_
during cmake configuration:

.. code-block:: bash

   $ cmake -DCMAKE_PREFIX_PATH="/some/install/path" ..

And finally, assuming make files generator (which is default)...

.. code-block:: PowerShell

   $ make -j4
   $ ./hello_event_loop/hello_event_loop
   Hello EventLoop!