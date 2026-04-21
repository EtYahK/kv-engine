# kv-engine

A in-memory key/value store written in C++17.

Built to explore how systems like Redis work, mainly TCP networking, concurrency, and persistence.

---

## Features

- TCP server supporting multiple clients
- Commands: SET / GET / DEL / EXISTS / TTL / PING / DBSIZE
- Key expiration (TTL)
- AOF persistence (log replay on startup)
- Benchmark tool

---

## Build

```bash
cmake -S . -B build
cmake --build build
```

---

## Run

```bash
./build/minikv --port 6380
```

---

## Test

```
SET name alice
GET name
PING
```

---

## Notes

- Uses shared_mutex for thread-safe access
- Thread pool handles client requests
- TTL uses lazy + background cleanup
- AOF is append-only (replay on startup)

---

## Benchmark

\~120k QPS on local machine (8 threads, loopback)
