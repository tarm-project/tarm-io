#include "io/Common.h"

#include "io/EventLoop.h"

int main(int argc, char* argv[]) {
    io::EventLoop loop;

    return loop.run();
}