
#define _FILE_OFFSET_BITS 64
#define __USE_LARGEFILE64

#define _GNU_SOURCE
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>


#include "rar_cache.h"


/*
 *
 * opendir when it finds a RAR file will stop, and call popen(unrar) to list
 * and cache the entries it finds. It will then return these entries instead
 * of the original RAR. Although, this means you can no longer copy the files
 * so perhaps it needs to show both.
 * As it is cached, stat() of the files inside RAR will have low impact.
 * We can optionally release the cache at closedir() but it is probably
 * better if we don't.
 * We have a problem if the program calls stat() on a file inside RAR without
 * having called opendir, since we do not know which rar file it belongs to.
 * We may need to seek solutions for this. (Look for the RAR ourselves).
 *
 */

//#define DO_NOTHING   // Just intercept calls, add no logic











