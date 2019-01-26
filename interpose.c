#if HAVE_CONFIG_H
#include "config.h"
#endif

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
#if HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include "misc.h"
#include "fdmap.h"
#include "interpose.h"
#include "cache.h"
#include "spawn.h"


// Due to the LARGEFILE magic that the Linux headers do, we need to do a little
// extra work to be able to interpose open() and stat().
#ifdef __REDIRECT
extern int __REDIRECT (myopen,(__const char *__file, int __oflag, ...) __THROW,
                       open);
extern int __REDIRECT (mystat,(__const char *__file, struct stat *__restrict __buf) __THROW,
                       stat);
#else
#define mystat stat
#define myopen open
#endif


#ifndef ELIBACC
#define ELIBACC ENOSYS
#endif


//#define VERBOSE


//
// Variables to hold the address of the real libc functions.
//
DIR  *(*real_opendir)  (__const char *__name)                       = NULL;
int   (*real_lstat64)  (__const char *__restrict __file,
                        struct stat64 *__restrict __buf)            = NULL;
int   (*real_stat64)   (__const char *__restrict __file,
                        struct stat64 *__restrict __buf)            = NULL;
int   (*real_stat)     (__const char *__restrict __file,
                        struct stat *__restrict __buf)              = NULL;
struct dirent64
     *(*real_readdir64)(DIR *__dirp)                                = NULL;

int   (*real_closedir) (DIR *__dirp)                                = NULL;

int   (*real_open64)   (__const char *__file, int __oflag, ... )    = NULL;

int   (*real_open)     (__const char *__file, int __oflag, ... )    = NULL;
__off64_t
      (*real_lseek64)  (int __fd, __off64_t __offset, int __whence) = NULL;
ssize_t
      (*real_read)     (int __fd, void *__buf, size_t __nbytes)     = NULL;

int   (*real_close)    (int __fd)                                   = NULL;
int   (*real_unlink)   (__const char *__name);



// stat() has logic to cause opendir() to be triggered, but stat is also
// called from opendir(), so we need to stop the loop.
static int internal_disable_stat = 0;





//
// Lookup the real libc call
//
int lookup_real(void **function, char *name)
{
	void *handle;

	// bad ptr?
	if (!function) return 0;

	// Already have it
	if (*function) return 1;

	handle = dlopen("/lib/libc.so.0", RTLD_LAZY);
	if (handle) {
		// Lets look them all up in one go:
		//*function = dlsym(handle, name);

		real_opendir   = dlsym(handle, "opendir");
		real_lstat64   = dlsym(handle, "lstat64");
		real_stat64    = dlsym(handle, "stat64");
		real_stat      = dlsym(handle, "stat");
		real_readdir64 = dlsym(handle, "readdir64");
		real_closedir  = dlsym(handle, "closedir");
		real_open64    = dlsym(handle, "open64");
		real_open      = dlsym(handle, "open");
		real_lseek64   = dlsym(handle, "lseek64");
		real_read      = dlsym(handle, "read");
		real_close     = dlsym(handle, "close");
		real_unlink    = dlsym(handle, "unlink");

		dlclose(handle);
	}

	if (!*function)
		*function = dlsym(RTLD_NEXT, name);

	if (!*function) {
		fprintf(stderr, "[reload] FAILED TO LOCATE '%s'\r\n", name);
		errno=ELIBACC;
		return 0;
	}

	xdebugf("[preload] looked up '%s' (and everything else)\r\n", name);
	// Found it, and we are happy.
	return 1;
}



int lstat64(__const char *__restrict __file,  struct stat64 *__restrict __buf)
{
	int result;

#ifdef VERBOSE
	fprintf(stderr, "*lstat64*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_lstat64, "lstat64")) {
		return -1;
	}

	// Call the real function and let it verify the arguments, etc.
	result = real_lstat64(__file, __buf);

	// If any failure, return.
	if (result != 0) {

		// If the error is ENOENT, maybe its in the cache?
#ifndef DO_NOTHING
		if ((errno == ENOENT) &&
			__file &&
			*__file) {
			xdebugf("[lstat64] '%s' ... \r\n", __file);
			return cache_statfile64(__file, __buf);
		}
#endif
		return result;
	}

	// Success, we can trust "__buf"

	return result;

}


int stat64(__const char *__restrict __file,  struct stat64 *__restrict __buf)
{
	int result;

#ifdef VERBOSE
	fprintf(stderr, "*stat64*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_stat64, "stat64")) {
		return -1;
	}

	// Call the real function and let it verify the arguments, etc.
	result = real_stat64(__file, __buf);

	// If any failure, return.
	if (result != 0) {

		// If the error is ENOENT, maybe its in the cache?
#ifndef DO_NOTHING
		if ((errno == ENOENT) &&
			__file &&
			*__file) {
			xdebugf("[stat64] '%s'... \n", __file);
			return cache_statfile64(__file, __buf);
		}
#endif
		return result;
	}

	// Success, we can trust "__buf"

	return result;

}



int mystat(__const char *__restrict __file,  struct stat *__restrict __buf)
{
	int result;

#ifdef VERBOSE
	fprintf(stderr, "**stat** '%s'\r\n", __file);
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_stat, "stat")) {
		return -1;
	}

	// Call the real function and let it verify the arguments, etc.
	result = real_stat(__file, __buf);

	// If any failure, return. Also stop us going in circles.
	if ((result != 0) && !internal_disable_stat) {

		// If the error is ENOENT, maybe its in the cache?
#ifndef DO_NOTHING
		if ((errno == ENOENT) &&
			__file &&
			*__file) {
			xdebugf("[stat] '%s'... \n", __file);
			result = cache_statfile(__file, __buf);
		}
#endif
		return result;
	}

	// Success, we can trust "__buf"

	return result;

}



DIR *opendir (__const char *__name)
{
	DIR *result = NULL;
	char absolute[PATH_MAX];

#ifdef VERBOSE
	fprintf(stderr, "*opendir*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_opendir, "opendir")) {
		return NULL;
	}

	// Call the real function and let it verify the arguments, etc.
	// opendir() calls stat() which we need to lookout for.
	internal_disable_stat = 1;
	result = real_opendir(__name);
	internal_disable_stat = 0;

	// If any failure, return.
	if (result == NULL)
		return result;

	// Success, we can trust "__buf"
#ifndef DO_NOTHING
	if (!realpath(__name, absolute))
		absolute[0] = 0;

	xdebugf("[opendir] '%s' => '%s'\r\n", __name, absolute);

	// Lets skip some dirs
	if (skip_directory(__name))
		return result;

	// only keep one cache for now.
    cache_clear();

	cache_newnode(absolute, result);

#endif

	return result;

}


struct dirent64 *readdir64(DIR *__dirp)
{
	struct dirent64 *result;
    rar_cache_t *cache = NULL;

#ifdef VERBOSE
	fprintf(stderr, "*readdir64*\r\n");
#endif


	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_readdir64, "readdir64")) {
		return NULL;
	}

	// Are we iterating entries from inside archive? If so, get next
	// item, and just return it.
#ifndef DO_NOTHING
	if ((cache = cache_hasdirp(__dirp)) &&
        (result = cache_nextentry(cache))) {
		return result;
	}
#endif

	// Call the real function and let it verify the arguments, etc.
	result = real_readdir64(__dirp);

	// If any failure, return.
	if (result == NULL)
		return result;

	// Success, we can trust arguments
	xdebugf("[readdir64] type %d '%s' (cache %p)\r\n", 
		   result->d_type, result->d_name, cache);

	// If we have an archive, and its for this opendir, AND its a rar name.
#ifndef DO_NOTHING
	if (cache &&
		filename_is_rar(result->d_name)) {
		rar_expandlist(cache, result->d_name);
	}
#endif

	return result;

}

int closedir (DIR *__dirp)
{
	int result;
	rar_cache_t *cache;

#ifdef VERBOSE
	fprintf(stderr, "*closedir*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_closedir, "closedir")) {
		return -1;
	}

	// Call the real function and let it verify the arguments, etc.
	result = real_closedir(__dirp);

	// Clear the dirp ptr so it doesn't think we are still inside opendir
	cache = cache_hasdirp(__dirp);
	if (cache) {
		cache->dirp = NULL;
	}

	xdebugf("[closedir] closing. (cache %p)\r\n", cache);

	// If any failure, return.
	if (result != 0)
		return result;

	// Success, we can trust arguments
	return result;

}



int open64(__const char *__file, int __oflag, ... )
{
	int result;

#ifdef VERBOSE
	fprintf(stderr, "*open64*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_open64, "open64")) {
		return -1;
	}

	// Call the real function and let it verify the arguments, etc.
	if (__oflag & O_CREAT) {
		va_list arg;
		mode_t mode;
		va_start(arg, __oflag);
		mode = va_arg(arg, mode_t);
		va_end(arg);

		result = real_open64(__file, __oflag, mode);

	} else {
		result = real_open64(__file, __oflag, 0);
	}

	xdebugf("[open64] called for '%s'\r\n", __file);

	// If the failure is ENOENT, we will also check the cache incase it is
	// an archive file.
	// If any failure, return.
	if (result < 0) {

		if (errno == ENOENT) {

			uint32_t curr_archive, curr_entry;
			archive_t *archive;
			entry_t *entry;
            rar_cache_t *cache = NULL;

			xdebugf("[open64] locating file...\r\n");
			result = cache_findfile(&cache, &curr_archive, &curr_entry, __file);
			if (!result)
				return -1; // Not found
			xdebugf("[open64] result %d...\r\n", result);

			archive = cache->archives[ curr_archive ];
			entry = archive->entries[ curr_entry ];

			xdebugf("[open64] open on archive entry for '%s/%s'.\n",
				   archive->rarname, entry->filename);

			// Create a new node, and spawn first rar.
			result = rar_openfile(cache, archive, entry);

		}

		// Open failed.
		return result;
	}

	// Success, we can trust arguments
	xdebugf("[open64] '%s' => %d\r\n", __file, result);
	return result;

}


int myopen(__const char *__file, int __oflag, ... )
{
	int result;

#ifdef VERBOSE
	fprintf(stderr, "*open*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_open, "open")) {
		return -1;
	}

	errno = 0;

	// Call the real function and let it verify the arguments, etc.

	// open() calls stat()
	if (__oflag & O_CREAT) {
		va_list arg;
		mode_t mode;
		va_start(arg, __oflag);
		mode = va_arg(arg, mode_t);
		va_end(arg);

		result = real_open(__file, __oflag, mode);

	} else {
		result = real_open(__file, __oflag, 0);
	}

	xdebugf("[open] called for '%s' (%d)\r\n", __file, result);

	// If the failure is ENOENT, we will also check the cache incase it is
	// an archive file.
	// If any failure, return.
	if (result < 0) {

		if (errno == ENOENT) {

			uint32_t curr_archive, curr_entry;
			archive_t *archive;
			entry_t *entry;
            rar_cache_t *cache;

			if (!cache_findfile(&cache, &curr_archive, &curr_entry,	__file)) {
				xdebugf("[open] returning failure on '%s'\r\n", __file);
				return result; // Not found
			}
			archive = cache->archives[ curr_archive ];
			entry = archive->entries[ curr_entry ];

			xdebugf("[open] open on archive entry for '%s/%s'.\n",
				   archive->rarname, entry->filename);

			// Create a new node, and spawn first rar.
			result = rar_openfile(cache, archive, entry);

		}

		// Open failed.
		return result;
	}


	// Success, we can trust arguments
	xdebugf("[open] '%s' => %d\r\n", __file, result);

	return result;

}




__off64_t lseek64(int __fd, __off64_t __offset, int __whence)
{
	__off64_t result;
	rar_fdmap_t *fdmap;

#ifdef VERBOSE
	fprintf(stderr, "*lseek64*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_lseek64, "lseek64")) {
		return -1;
	}

	// Check if it is for a file in rar
	fdmap = rar_fdmap_find(__fd);
	if (fdmap) {
		xdebugf("[lseek64] fd %d seek whence %d amount %lld\n", __fd, __whence, __offset);
		xdebugf("[lseek64] current %llu wanted %llu\r\n",
			   fdmap->current_offset, fdmap->wanted_offset);

		switch(__whence) {
		case SEEK_SET:
			fdmap->wanted_offset = (uint64_t) __offset;
			break;
		case SEEK_CUR:
			// read increases both current and wanted, so that multiple
			// lseeks without reads will compute correctly.
			fdmap->wanted_offset = fdmap->wanted_offset + (int64_t) __offset;
			break;
		case SEEK_END:
			fdmap->wanted_offset = fdmap->size + (int64_t) __offset;
			break;
		default:
			errno = EINVAL;
			return (__off64_t) -1;
		}

		xdebugf("[lseek64] computed offset %llu. (filesize %llu)\r\n",
			   fdmap->wanted_offset, fdmap->size);

		// invalid lseek offset? It's unsigned.
		if (fdmap->wanted_offset > fdmap->size) {
			errno = EINVAL;
			return (__off64_t) -1;
		}

		// New seek is valid, mark node dirty
		fdmap->lseek_called = 1;

		return (__off64_t) fdmap->wanted_offset;
	}


	// Call the real function and let it verify the arguments, etc.
	result = real_lseek64(__fd, __offset, __whence);

	// If any failure, return.
	if (result == -1) {
		return result;
	}

	// Success, we can trust arguments
	return result;
}


ssize_t read(int __fd, void *__buf, size_t __nbytes)
{
	ssize_t result;
	rar_fdmap_t *fdmap;

#ifdef VERBOSE
	//fprintf(stderr, "*read*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_read, "read")) {
		return -1;
	}


	// Check if it is for a file in rar
	fdmap = rar_fdmap_find(__fd);

	// check if lseek has been called (and do we need to respawn?)
	// as a special-case speed up, if the seek is forward, but only
	// by a small amount, it is more efficient to read and eat bytes, then
	// to respawn everything.
#if 1
	if (fdmap && fdmap->lseek_called &&
		(fdmap->wanted_offset > fdmap->current_offset) &&
		((fdmap->wanted_offset - fdmap->current_offset) < RAR_SMALLSEEK)) {
		uint64_t diff;

		xdebugf("[read] small skip, eating bytes...\r\n");

		diff = fdmap->wanted_offset - fdmap->current_offset;
		while (diff > 0) {

			// read the smallest of "diff" or "__nbytes"
			result = real_read(__fd, __buf,
							   diff <= __nbytes ? diff : __nbytes);

			// This could actually be EOF here.. but since we are
			// eating bytes to get to an lseek, we will just return failure
			// This is unlikely to happen, we hope
			if (result <= 0) return result; // failed

			// We've read these bytes, we might as well buffer them
			fdmap_add_buffer(fdmap, __buf, (uint64_t) result);

			fdmap->current_offset += (uint64_t) result;
			diff -= (uint64_t) result;

		} // while

		// we should be at the correct place again
		fdmap->lseek_called = 0;
		fdmap->wanted_offset = fdmap->current_offset;

	} // if SMALLSEEK
#endif

	// check if lseek has been called (and do we need to respawn?)
	if (fdmap && fdmap->lseek_called &&
		(fdmap->current_offset != fdmap->wanted_offset)) {

		
		xdebugf("[read] want %lu bytes from %llu\r\n",
				__nbytes, fdmap->wanted_offset);

        // Additionally here, if we are using BUFFERS, lets check if we already
        // have the wanted data, and if so, just return it. (But we don't update
        // seek, nor respawn, nor clear lseek_wanted).
        result = (ssize_t) fdmap_have_buffer(fdmap, __buf, (uint64_t) __nbytes);
        if (result > 0) {
            // We had (some of) the data in the buffers, yay!
#if 0
			if (result < (ssize_t) __nbytes) {
				xdebugf("[read] warning, returning less than wanted. (%lu < %lu)\r\n",
					   (unsigned long)result, (unsigned long)__nbytes);
			} 
			return result;
#else
			// Lets only return data if we could satisfy ALL bytes.
			if (result == (ssize_t) __nbytes)
				return result;
			// Partial read not ok, seek back.
			fdmap->wanted_offset -= (uint64_t) result;
#endif
        }


		errno = 0;
		fdmap->despawn_closing = 1;
		if (rar_spawn(fdmap)) {
			// respawning failed. Do we care? read will fail too..
		}
		fdmap->despawn_closing = 0;
	}

	// Call the real function and let it verify the arguments, etc.
	result = real_read(__fd, __buf, __nbytes);

	// If any failure, return. This is the first call, so just return is ok.
	if (result <= 0)
		return result;

	if (fdmap) {

		xdebugf("[read] fd %d read %d bytes ( %lld->%lld)\r\n", fdmap->fd,
			   result,
			   fdmap->current_offset,
			   fdmap->current_offset + (uint64_t) result);

		if (result < __nbytes)
			xdebugf("[read] calling read again!\r\n");

		while (result < __nbytes) {
			ssize_t result2;
			result2 = real_read(__fd, &((uint8_t *)__buf)[result], __nbytes - result);

			if (result2 <= 0) {
				// future read EOF, so return what we got...
				xdebugf("[read] read really meant it: %d\r\n", result2);
				return result;
			}
			xdebugf("[read] more reading ...  %d\r\n", result2);
			result += result2;
		};

        // If we are to use buffers, we need to keep moving data in.
        fdmap_add_buffer(fdmap, __buf, (uint64_t) result);


		// increase our offset ptr
		fdmap->current_offset += (uint64_t) result;
		fdmap->wanted_offset  += (uint64_t) result;

		xdebugf("[read] finally returning %d bytes read\r\n", result);



	} // fdmap

	// Success, we can trust arguments
	return result;
}

int close (int __fd)
{
	int result;
	rar_fdmap_t *fdmap;

#ifdef VERBOSE
	fprintf(stderr, "*close*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_close, "close")) {
		return -1;
	}

	fdmap = rar_fdmap_find(__fd);
	// Its ours and NOT triggered by rar_despawn()...
	if (fdmap && !fdmap->despawn_closing) {
		xdebugf("[close] finished with fdmap %d\n", fdmap->fd);

		fdmap->despawn_closing = 1;
		rar_despawn(fdmap);
		fdmap->despawn_closing = 0;
		fdmap->fd = -1;
		rar_fdmap_freenode(fdmap);
		return 1; // We could check pclose returncode.
	}


	// Call the real function and let it verify the arguments, etc.
	result = real_close(__fd);

	xdebugf("[close] actually closed %d\n", __fd);

	// If any failure, return.
	if (result <= 0)
		return result;

	// Success, we can trust arguments
	return result;
}


int unlink(__const char *__name)
{
	int result;

#ifdef VERBOSE
	fprintf(stderr, "*unlink*\r\n");
#endif

	// Don't have the real function ptr? Then look it up.
	if (!lookup_real((void *)&real_unlink, "unlink")) {
		return -1;
	}

	// Call the real function and let it verify the arguments, etc.
	result = real_unlink(__name);

	// If we failed due to ENOENT lets check if it is from inside a rar
	// archive, and if so, pretend it was deleted.
	if ((result < 0) && (errno == ENOENT)) {
		uint32_t curr_archive, curr_entry;
		rar_cache_t *cache = NULL;

		if (cache_findfile(&cache, &curr_archive, &curr_entry,	__name)) {
			return 0; // Yeah, we deleted that.
		}
		// Not ours so, return original issue
	}

	// Success, we can trust arguments
	return result;
}

