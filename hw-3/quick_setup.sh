#!/bin/bash
cmake -S . -B build `-D CMAKE_BUILD_TYPE=Release`
cmake --build build  --config Release --target dfa_minim
