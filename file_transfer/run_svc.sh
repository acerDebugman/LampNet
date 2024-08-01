#!/bin/bash

#clang server.c
g++ server.c -o server
./server -p 9999 -d cache

