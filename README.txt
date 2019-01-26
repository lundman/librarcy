
librarcy is a library that interposes the libc calls to standard file IO, and directory listing, functions to enhance these with streaming unrar support. When needed, librarcy will popen() the unrar binary to list the contents, and read data.

librarcy requires the special version on unrar that has the "-sk" option to seek to position inside the archives.

librarcy can only support seeking in RAR archives in "store" method. (No compression).

librarcy was primarily written for SMP8634 media devices, Linux-2.6.15, uClibc, busybox but could be ported to other platforms. Known to work with Dune HD Center, and NetworkedMediaTank.

Environment variables:

LIBRARCY_UNRAR=/path/unrar     : path to unrar, default "/tmp/unrar"
LIBRARCY_DEBUG                             : Write debug log to "/tmp/librarcy.log"


Interpose libc functions:

 open
 open64
 stat
 stat64
 lstat64
 opendir
 readdir64
 closedir
 read
 lseek64
 close


Usage:

You need to set LD_PRELOAD for librarcy to be used. You can chose to do so for spawned programs only.

# LD_PRELOAD=/tmp/librarcy.so.1.0.1 ls -l

You can set it for the shell and ALL future programs:

# export LD_PRELOAD=/tmp/librarcy.so.1.0.1
# ls -l


Jorgen Lundman
<lundman@lundman.net>
http://lundman.net/wiki/index.php/Librarchy
