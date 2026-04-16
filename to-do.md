# HTTP Server – Future Scope / Roadmap

## ✅ Current Status
- Basic HTTP/1.1 server
- Thread pool with blocking I/O
- GET + basic POST support
- Static file serving
- Keep-alive support
- Basic routing and logging

---

## 🧩 Level 1: Improvements (Polish)

- [ ] Improve error handling (timeouts, malformed requests)
- [ ] Structured logging (log file + timestamps)
- [ ] Config improvements (more flexible config parsing)
- [ ] Better routing system (map paths → handlers)
- [ ] Cleaner code structure (separate parsing, routing, response)

---

## 🌐 Level 2: HTTP Compliance

- [ ] Proper HTTP/1.0 support
- [ ] Full header handling (Host, User-Agent, etc.)
- [ ] Query parameter parsing (`/search?q=abc`)
- [ ] Chunked transfer encoding support
- [ ] Better keep-alive handling
- [ ] HTTP pipelining (multiple requests without waiting)

---

## 📁 Level 2.5: File Serving Enhancements

- [ ] Add caching headers (ETag, Last-Modified)
- [ ] Gzip compression
- [ ] Range requests (partial content / video streaming)
- [ ] MIME type improvements

---

## ⚙️ Level 3: Architecture Upgrade (Major Rewrite)

- [ ] Replace thread pool with event-driven model
- [ ] Implement `select()`-based server
- [ ] Upgrade to `poll()`
- [ ] Upgrade to `epoll()` (Linux)

### Non-blocking I/O
- [ ] Set sockets to non-blocking mode
- [ ] Handle partial `recv()` properly
- [ ] Handle partial `send()` properly

### Connection State Machine
- [ ] Maintain per-connection state:
  - READING_HEADERS
  - READING_BODY
  - WRITING_RESPONSE
- [ ] Persistent connection lifecycle management

---

## 🚀 Level 4: Performance Optimization

- [ ] Use `sendfile()` for zero-copy file transfer
- [ ] Avoid unnecessary string copies
- [ ] Implement buffer reuse / pooling
- [ ] Backpressure handling (slow clients)
- [ ] Optimize memory usage

---

## 🔐 Level 5: Production Features

- [ ] HTTPS support (OpenSSL)
- [ ] Virtual hosting (multiple domains)
- [ ] Rate limiting
- [ ] Request validation & security hardening
- [ ] Logging to file with rotation
- [ ] Basic load balancing concepts

---

## 🧠 Notes

- Current server is **thread-based and blocking**
- Real servers are **event-driven and non-blocking**
- Moving to epoll requires **complete architectural redesign**
- This roadmap is **progressive**, not incremental

---

## 🎯 Learning Path

1. select()
2. poll()
3. epoll()
4. state machine
5. fully non-blocking server

---

##

> Current implementation uses thread pool + blocking I/O.  
> A production-grade server would use an event-driven model (epoll), maintain per-connection state, and handle partial reads/writes efficiently.
