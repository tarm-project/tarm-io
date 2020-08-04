# Tarm-io
Tarm-io is general purpose event-driven C++11 input-output library.  

The library is under heavy development.  
Some information you can find at https://tarm.io

## Features

* Full-featured event loop backed by epoll, kqueue, IOCP, event ports.
* Asynchronous networking (TCP, UDP, TLS, DTLS protocols and more)
* IPv4 and IPv6
* Support of various SSL backend libraries
* Asynchronous DNS resolution
* No sockets. Instead library provides higher level abstractions like clients and servers
* Asynchronous file and file system operations
* Paths constructions and manipulations
* File system events
* Thread pool
* Signal handling
* Embedded data producer or consumer pacing
* Guaranteed and auto-tested ABI compatibility between minor and patch versions

## Status

### Build:
![Windows-32bit](https://github.com/tarm-project/tarm-io/workflows/Windows-32bit/badge.svg) 
![Windows-64bit](https://github.com/tarm-project/tarm-io/workflows/Windows-64bit/badge.svg) 
![Linux](https://github.com/tarm-project/tarm-io/workflows/Linux/badge.svg) 
![Mac](https://github.com/tarm-project/tarm-io/workflows/Mac/badge.svg) 

### Analyzers:
![Valgrind](https://github.com/tarm-project/tarm-io/workflows/Valgrind/badge.svg) 
![Thread Sanitizer](https://github.com/tarm-project/tarm-io/workflows/Thread%20Sanitizer/badge.svg) 
![Undefined Behavior Sanitizer](https://github.com/tarm-project/tarm-io/workflows/Undefined%20Behavior%20Sanitizer/badge.svg) 
![CodeQL](https://github.com/tarm-project/tarm-io/workflows/CodeQL/badge.svg) 
<!---
[<img src="https://img.shields.io/coverity/scan/21283.svg">](https://scan.coverity.com/projects/tarm-project-tarm-io) 
-->

## License
[MIT](https://app.fossa.com/api/projects/git%2Bgithub.com%2Ftarm-project%2Ftarm-io.svg?type=large)

[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Ftarm-project%2Ftarm-io.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Ftarm-project%2Ftarm-io?ref=badge_shield)
