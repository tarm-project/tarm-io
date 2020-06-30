/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

// Important notice: if you change this code, ensure that docs are displaying it correctly,
//                   because lines positions are important.

#include <tarm/io/EventLoop.h>

#include <iostream>

using namespace tarm;

int main() {
    io::EventLoop loop;

    loop.schedule_callback([](io::EventLoop&) {
        std::cout << "Hello EventLoop!" << std::endl;
    });

    return loop.run();
}
