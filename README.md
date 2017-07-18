### lvasynctls
A library for asynchronously calling TLS functions from labview and getting responses back through an event interface.

Many/most of LabVIEW's primitive I/O calls are async and don't block, but calling a dll does block an entire running thread. If the dll call itself blocks, this means you've blocked an entire thread. Since LV uses a fixed thread count, any reasonably complex networking code would hang LV (or at least an entire runtime system). This implementation is completely non-blocking and uses events to send data back into LV (or to tell it to read). It does this by utilizing Boost ASIO which provides an async wrapper for openssl/libressl's core library (libssl). The C++ code in this repository is a set of relatively simple wrappers for the boost functionality along with some callbacks to shoot data into LabVIEW. Unfortunately using any of the labview functions (like DSNewHandle for example) requires that a dll be called from the runtime engine, so a lot of the code is really just extra layers to make the functionality testable from within the C environment. That is, its a labview-function-calling layer which wraps a C++ layer which wraps boost asio which in turn wraps openssl.

Currently, expect that each object is *not* thread safe. Thread safety is implemented for some components internally, but the expectation is that each object be locked in a DVR. This is performed by the labview code right now, which is thread safe, except for user events. It seems as though there are conditions where labview does not correctly detect invalid user event refnums (previously valid, but now destroyed). I suggest being sure that all sockets, connectors, and engines are closed before destroying any associated user events. The close functions are synchronous.

Alternatives:
* https://github.com/smithed/lvlibtls uses libressl's libtls layer to perform the ssl work with a custom callback to feed the encrypted data over the normal TCP VIs. Its weird, but about 8-10x faster.
* You can technically feed the labview tcp sockets (using the get raw net object functions here http://digital.ni.com/public.nsf/allkb/7EFCA5D83B59DFDC86256D60007F5839) directly into libressl, but they are completely non-blocking and so you have to poll.
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
//labview
labviewv.lib
//boost asio dependencies
libboost_system-vc141-mt-1_64.lib
//prior dependencies which *should* be gone now (in exchange for c++11)
libboost_thread-vc141-mt-1_64.lib
libboost_serialization-vc141-mt-1_64.lib
libboost_regex-vc141-mt-1_64.lib
libboost_date_time-vc141-mt-1_64.lib
