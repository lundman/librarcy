#ifndef INTERPOSE_H_INCLUDED
#define INTERPOSE_H_INCLUDED


// This is kinda ick, but for now..
#ifndef HAVE___OFF64_T
typedef uint64_t __off64_t;
#endif
#ifndef HAVE_SSIZE_T
typedef uint32_t ssize_t;
#endif
#ifndef HAVE_STRUCT_DIRENT64
//typedef struct dirent dirent64;
#define dirent64  dirent
#define readdir64 readdir
#endif

#ifndef PATH_MAX
#include <limits.h>
#ifdef _POSIX_PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#endif
#endif

//
// Variables to hold the address of the real libc functions.
//
extern DIR  *(*real_opendir)  (__const char *__name);
extern int   (*real_lstat64)  (__const char *__restrict __file,
							   struct stat64 *__restrict __buf);
extern int   (*real_stat64)   (__const char *__restrict __file,
							   struct stat64 *__restrict __buf);
extern int   (*real_stat)     (__const char *__restrict __file,
							   struct stat *__restrict __buf);
extern struct dirent64
            *(*real_readdir64)(DIR *__dirp);
extern int   (*real_closedir) (DIR *__dirp);
extern int   (*real_open64)   (__const char *__file, int __oflag, ... );
extern int   (*real_open)     (__const char *__file, int __oflag, ... );
extern __off64_t
             (*real_lseek64)  (int __fd, __off64_t __offset, int __whence);
extern ssize_t
             (*real_read)     (int __fd, void *__buf, size_t __nbytes);
extern int   (*real_close)    (int __fd);

extern int   (*real_close)    (int __fd);
extern int   (*real_unlink)   (__const char *__name);





int       lookup_real     ( void **function, char *name );


#endif

