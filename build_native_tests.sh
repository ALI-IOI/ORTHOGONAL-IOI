#!/usr/bin/env bash
# Builds and runs the native test program with g++ (no Emscripten needed).
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -Wall -Wextra src/*.cpp tests/test_main.cpp -o tests/run_tests
./tests/run_tests
