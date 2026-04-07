# Multithreaded HTTP Server

## What it does
Basic HTTP server in C++ using thread pool to handle multiple clients.

## Features
- Thread pool (no thread-per-connection)
- Socket-based server
- Handles multiple clients concurrently

## Build
mkdir -p bin
g++ -Wall -Wextra -g src/*.cpp -I include -o bin/server -pthread

## Run
./bin/server