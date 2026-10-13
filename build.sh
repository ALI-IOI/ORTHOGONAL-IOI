#!/usr/bin/env bash
# build.sh - compiles the C++ code to WebAssembly with Emscripten and makes index.html.
#
# Needs Emscripten (once):
#   git clone https://github.com/emscripten-core/emsdk.git
#   cd emsdk && ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
#
# Output: index.html - ONE file containing the page, the JavaScript glue and the
# WebAssembly (base64). It works on GitHub Pages and offline by double-click.
set -e
cd "$(dirname "$0")"
mkdir -p build

em++ src/*.cpp -std=c++17 -O3 -fexceptions \
  -sEXPORTED_FUNCTIONS=_simulate -sEXPORTED_RUNTIME_METHODS=ccall \
  -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sMODULARIZE=1 -sEXPORT_NAME=createOrthoModule \
  -sMIN_SAFARI_VERSION=150000 -sMIN_CHROME_VERSION=85 -sMIN_FIREFOX_VERSION=79 \
  -sSINGLE_FILE=1 -o build/ortho.js

python3 tools/make_single_file.py web/page.html build/ortho.js index.html
