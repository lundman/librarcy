#!/bin/sh

gcc -fPIC -rdynamic -g -c -Wall rar_stat.c
gcc -shared -Wl,-soname,librarcy.so.1 -o librarcy.so.1.0.1 rar_stat.o -lc -ldl
