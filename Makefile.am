#mipsel-linux-gcc -fPIC -rdynamic -g -c -Wall rar_stat.c
#mipsel-linux-gcc -shared -Wl,-soname,librarcy.so.1 -o librarcy.so.1.0.1 rar_stat.o -lc -ldl

lib_LTLIBRARIES=librarcy.la
librarcy_la_SOURCES=misc.c interpose.c cache.c spawn.c fdmap.c cache.h config.h fdmap.h interpose.h misc.h spawn.h
librarcy_la_CFLAGS=-D_GNU_SOURCE
librarcy_la_LDFLAGS=@RDYNAMIC@ -version-info "1:0:1"

EXTRA_DIST=README.txt


