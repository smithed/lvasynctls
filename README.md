### lvasynctls
A library for asynchronously calling TLS functions from labview and getting responses back through an event interface.

Many/most of LabVIEW's primitive I/O calls are async and don't block, but calling a dll does block an entire running thread. If the dll call itself blocks, this means you've blocked an entire thread. Since LV uses a fixed thread count, any reasonably complex networking code would hang LV (or at least an entire runtime system). This implementation is completely non-blocking and uses events to send data back into LV (or to tell it to read). It does this by utilizing Boost ASIO which provides an async wrapper for openssl/libressl's core library (libssl). The C++ code in this repository is a set of relatively simple wrappers for the boost functionality along with some callbacks to spit code into LabVIEW. Unfortunately using any of the labview functions (like DSNewHandle for example) requires that a dll be called from the runtime engine, so a lot of the code is really just extra layers to make the functionaly testable from within the C environment. That is, its a labview-function-calling layer which wraps a C++ layer which wraps boost asio which in turn wraps openssl.

The C++ code is quite crap right now, as I hardly know C++ well enough to attempt this, but I'm slowly working my way through the bugs/refactoring.

Currently, expect that each object is *not* thread safe.

Alternatives:
* Some libraries like libtls in libressl are easy to use, but are either blocking or require direct socket manipulation in a callback mechanism.
* OpenSSL actually has async capabilities but when I read the docs (example: http://man.openbsd.org/SSL_accept.3 and http://man.openbsd.org/SSL_read.3) it makes my head spin.
ASIO seems to take care of most of the hard work of dealing with the libssl stuff and makes a mostly nice interface.


#### Building
The c code requires the following. The labview code (not yet complete) just depends on the associated dlls.

Include directories:
path to boost
path to /boost/bind
path to /boost/asio
path to libressl include (or openssl)
path to labview cintools directory

Library directories:
path to boost built libraries
path to cintools
path to libressl binaries (or openssl)

Linked libraries:
//libressl, can also use openssl
libssl-43.lib
libcrypto-41.lib
libtls-15.lib //probably not actually required
//boost asio dependencies
libboost_system-vc141-mt-1_64.lib
libboost_thread-vc141-mt-1_64.lib
libboost_serialization-vc141-mt-1_64.lib
libboost_regex-vc141-mt-1_64.lib
libboost_date_time-vc141-mt-1_64.lib
//labview
labviewv.lib
