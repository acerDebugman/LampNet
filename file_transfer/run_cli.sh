#!/bin/bash

g++ client.c -o upload
./upload -p 9999 -f $1
