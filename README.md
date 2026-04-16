---

Multithreaded HTTP Server (C++)

Overview
A custom HTTP/1.1 server built from scratch in C++ using POSIX sockets.
The goal of this project is to understand how real web servers work internally — from TCP handling to HTTP parsing and concurrency.

---

Features

Core

* Multithreaded server using a thread pool
* Handles multiple clients concurrently
* Blocking I/O with connection reuse (keep-alive)

HTTP Support

* HTTP/1.1 support
* GET requests for static file serving
* Basic POST request handling
* Persistent connections (keep-alive)
* Case-insensitive header parsing

File Serving

* Serves static files from configurable web root
* MIME type detection (HTML, CSS, JS, images, etc.)
* Binary-safe file handling
* Custom error pages (400, 404, 405)

Request Handling

* Parses request line and headers
* Handles Content-Length for POST requests
* Basic routing (/api/search endpoint placeholder)
* Protection against path traversal (..)

Networking & Reliability

* Proper handling of partial send() and recv()
* Socket timeout handling (SO_RCVTIMEO)
* Graceful shutdown using signals
* Buffer reuse for handling partial and pipelined requests

Logging

* Basic request/response logging to stdout

---

Project Structure

.
├── src/        # source files
├── include/    # header files
├── build/      # object files (generated)
├── bin/        # final binary (generated)
├── Makefile
└── README.md

---

Build

make

---

Run

make run

Optional arguments

make run ARGS="<config_file> <web_root>"

or
make run ARGS="<web_root>"

Example

make run ARGS="../portfolio/host"

Server runs at

[http://localhost:8080]

---

Configuration

The server supports a config file (server.cfg) for:

* Port
* Thread count
* Web root directory

CLI arguments can override config values.

---

Limitations

* Uses blocking I/O (thread pool based)
* No chunked transfer encoding
* No HTTPS (TLS)
* No event-driven architecture (select/poll/epoll not implemented)
* Not optimized for high-scale production use

---

Future Scope

* Event-driven model (select → poll → epoll)
* Non-blocking sockets
* Connection state machine
* HTTP pipelining support
* Gzip compression
* Caching headers (ETag, Last-Modified)
* Range requests (partial content)
* HTTPS support (OpenSSL)

---

Learning Outcomes

* Understanding of TCP vs HTTP
* Building a server without frameworks
* Concurrency using thread pools
* Handling partial reads/writes in network programming
* Basics of HTTP protocol and request lifecycle

---

Notes

This is a learning project and not intended to replace production-grade servers like nginx or Apache.
The focus is on understanding systems-level concepts rather than building a production-ready server.
