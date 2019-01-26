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
#include "interpose.h"
#include "cache.h"



// Our archive. just the one for now
static rar_cache_t *cache_head     = NULL;




void entry_freenode(entry_t **entry)
{
	if (entry && *entry) {
		SAFE_FREE((*entry)->filename);
		SAFE_FREE(*entry);
	}
}

entry_t *entry_newnode(void)
{
	entry_t *result;

	result = (entry_t *) malloc(sizeof(*result));

	if (!result) return NULL;

	memset(result, 0, sizeof(*result));

	return result;
}



void entry_addnode(archive_t *archive, entry_t *entry)
{
	// reallocate more space?
	if (!archive->entries ||
		(archive->num_entries >= archive->max_entries)) {
		entry_t **tmp;
		tmp = (entry_t **) realloc(archive->entries,
								  sizeof(entry_t *) *
								  (archive->max_entries+ENTRY_ALLOCATE_STEPS)
								  );
		if (!tmp) return;

		archive->entries = tmp;
		archive->max_entries += ENTRY_ALLOCATE_STEPS;
		// We should clear the new nodes here.
	}

	archive->entries[ archive->num_entries ] = entry;
	archive->num_entries++;

}


void archive_addnode(rar_cache_t *cache, archive_t *archive)
{
	// reallocate more space?
	if (!cache->archives ||
		(cache->num_archives >= cache->max_archives)) {
		archive_t **tmp;
		tmp = (archive_t **) realloc(cache->archives,
									 sizeof(archive_t *) *
									 (cache->max_archives+ARCHIVE_ALLOCATE_STEPS)
									 );
		if (!tmp) return;

		cache->archives = tmp;
		cache->max_archives += ARCHIVE_ALLOCATE_STEPS;
		// We should clear the new nodes here.
	}

	cache->archives[ cache->num_archives ] = archive;
	cache->num_archives++;

}

archive_t *archive_newnode(char *name)
{
	archive_t *archive;
	archive = (archive_t *) malloc(sizeof(*archive));
	if (!archive) return NULL;
	memset(archive, 0, sizeof(*archive));
	archive->rarname = strdup(name);
	return archive;
}

void archive_freenode(archive_t **archive)
{
	int entry;

	if (archive && *archive) {

		SAFE_FREE((*archive)->rarname);

		// Release all entries
		for (entry = 0;
			 entry < (*archive)->num_entries;
			 entry++)
			entry_freenode(&(*archive)->entries[ entry ]);

		SAFE_FREE((*archive));
	}
}


void cache_freenode(rar_cache_t **node)
{
	int archive;

	// If ptr and set, its free'd and NULLed. If NULL, its NULL.
	if (node && *node) {
		(*node)->dirp = NULL; // paranoia
		SAFE_FREE((*node)->directory_path);

		// Release all archives
		for (archive = 0;
			 archive < (*node)->num_archives;
			 archive++)
			archive_freenode(&(*node)->archives[ archive ]);

		SAFE_FREE(*node);
	}
}


// Quick test to see if cache has dirp active.
rar_cache_t *cache_hasdirp(DIR *dirp)
{
    //rar_cache_t *runner;
    //
    //    for (runner = cache_head;
    //   runner;
    //   runner = runner->next)
    //if (runner->dirp == dirp) return runner;

    if (cache_head && cache_head->dirp == dirp) return cache_head;

    return NULL;
}



//
// Clear all in cache.
void cache_clear( void )
{
    if (!cache_head) return;

    cache_freenode(&cache_head);

    cache_head = NULL; // paranoia
}



rar_cache_t *cache_newnode(char *path, DIR *__dirp)
{
	rar_cache_t *result;

	// If we have a cache node already, with a dirp set, we are
	// already in a opendir/readdir sequence, so we stop the recursion
	if (cache_head && cache_head->dirp) {
		xdebugf("[cache] newnode recursion stopped\r\n");
		return NULL;
	}

	result = (rar_cache_t *) malloc(sizeof(*result));

	if (!result) return NULL;

	memset(result, 0, sizeof(*result));

	result->directory_path = strdup(path);
    result->dirp = __dirp;

	xdebugf("[cache] create cache node for '%s' : %p\r\n", path, __dirp);

	// Insert into cache.
	if (cache_head) cache_clear();

	cache_head = result;

	return result;
}




//
// Based on filename, locate a file in the cache: FIXME, add absolute path too
// IN: cache should point to the cache node to start searching
// OUT: archive_index is set if matching archive is found
// OUT: entry_index is set if matching entry is found
// OUT: returncode 0 is NOT FOUND
// OOT: returncode > 0 is FOUND.
//
int cache_findfile(rar_cache_t **ret_cache,
				   uint32_t *archive_index,
				   uint32_t *entry_index,
				   __const char *__file)
{
	uint32_t curr_archive, curr_entry;
	archive_t *archive;
	entry_t *entry;
	char *file_copy, *dir;
	const char *just_name;
	char absolute[PATH_MAX];
    rar_cache_t *cache;

#ifdef DO_NOTHING
	return 0;
#endif

    // Handle more than one cache?
    // for all in cache_head ...
	cache = cache_head;
    if (ret_cache)
        *ret_cache = cache;

	if (!strcmp(".",  __file) ||
		(!strcmp("..", __file))) {
		xdebugf("[findfile] skipping dot dirs\r\n");
		return 0;
	}

	file_copy = strdup(__file);
	dir = dirname(file_copy); //dirname modifies src, hence the copy
	if (!realpath(dir, absolute))
		absolute[0] = 0;

	if (skip_directory(dir))
		return 0;
	
	xdebugf("[findfile] '%s': cache %p (%s), absolute '%s'\r\n",
		   __file, cache, cache ? cache->directory_path : "empty",
		   absolute);

	// If empty dir OR ( cache is of a different dir AND
	// but not if we are already inside a opendir/readdir loop ) then
	// rescan the dir looking for it.
	if (!cache || (strcmp(absolute, cache->directory_path) &&
				   !cache->dirp)) {
		DIR *dirp;
		struct dirent64 *dent;

		xdebugf("[findfile] scanning...'%s' \r\n", absolute);

		// Lets perform a opendir/readdir/closedir here for the sake of stat
		// Attempt to work out path used in stat.
		dirp = opendir(absolute); // free's teh cache too
		while(dirp && (dent = readdir64(dirp)));
		if (dirp) closedir(dirp);

		xdebugf("[findfile] done.\r\n");

	}

	SAFE_FREE(file_copy);

	// cache_head is new, re-load it to detect changes
	cache = cache_head;

	// cache still empty? nothing to do...
	if (!cache) return 0;

    if (ret_cache)
        *ret_cache = cache;

	// looking for just the file name
	just_name = my_basename(__file);

	if (!just_name || !*just_name) return 0;

	xdebugf("[findfile] looking for '%s' ... \n", just_name);

	for (curr_archive = 0;
		 curr_archive < cache->num_archives;
		 curr_archive++) {

		archive = cache->archives[ curr_archive ];

		for (curr_entry = 0;
			 curr_entry < archive->num_entries;
			 curr_entry++) {

			entry = archive->entries[ curr_entry ];

			xdebugf("          : '%s' == '%s' ? \r\n",
				   entry->filename, just_name);

			// We have a match!
			if (!strcmp(entry->filename, just_name)) {

				xdebugf("[findfile] archive '%s' has file '%s'.\n",
					   archive->rarname, entry->filename);

				if (archive_index)
					*archive_index = curr_archive;
				if (entry_index)
					*entry_index = curr_entry;

				return 1;

			} // names equal

		} // for entries

	} // for archives

	xdebugf("[findfile] not found '%s'.\r\n", just_name);

	return 0;

}




//
// Simulate stat64() call based on filename, searching the cache.
//
// IN: filename
// OUT: struct stat __buf set
// OUT: returncode changed to be same as stat(). -1 error, 0 OK.
//
int cache_statfile64(__const char *__file,
					 struct stat64 *__buf)
{
	uint32_t curr_archive, curr_entry;
	archive_t *archive;
	entry_t *entry;
    rar_cache_t *cache;

#ifdef DO_NOTHING
	return -1;
#endif

	if (!cache_findfile(&cache, &curr_archive, &curr_entry, __file))
		return -1; // Not found

	archive = cache->archives[ curr_archive ];
	entry = archive->entries[ curr_entry ];

	xdebugf("[statfile] returning stat64 for '%s'.\n",
		   entry->filename);

	// stat64 buf is optional
	if (__buf) {
		memcpy(__buf, &archive->stat, sizeof(*__buf));
		//__buf->st_mode  = archive->stat.st_mode;
		//__buf->st_uid   = archive->stat.st_uid;
		//__buf->st_gid   = archive->stat.st_gid;
		//__buf->st_nlink = archive->stat.st_nlink;
		//__buf->st_mtime = entry->mtime;
		//__buf->st_atime = entry->mtime;
		//__buf->st_ctime = entry->mtime;
		__buf->st_size  = entry->size;
	}

	// Return success.
	return 0;
}


//
// Simulate stat() call based on filename, searching the cache.
//
// IN: filename
// OUT: struct stat __buf set
// OUT: returncode changed to be same as stat(). -1 error, 0 OK.
//
int cache_statfile(__const char *__file,
				   struct stat *__buf)
{
	uint32_t curr_archive, curr_entry;
	archive_t *archive;
	entry_t *entry;
    rar_cache_t *cache;

#ifdef DO_NOTHING
	return -1;
#endif

	if (!cache_findfile(&cache, &curr_archive, &curr_entry, __file))
		return -1; // Not found

	archive = cache->archives[ curr_archive ];
	entry = archive->entries[ curr_entry ];

	xdebugf("[statfile] returning stat for '%s'.\n",
		   entry->filename);

	// stat buf is optional
	if (__buf) {
		//memcpy(__buf, &archive->stat, sizeof(*__buf));
		__buf->st_mode  = archive->stat.st_mode;
		__buf->st_uid   = archive->stat.st_uid;
		__buf->st_gid   = archive->stat.st_gid;
		__buf->st_nlink = archive->stat.st_nlink;
		__buf->st_mtime = entry->mtime;
		__buf->st_atime = entry->mtime;
		__buf->st_ctime = entry->mtime;
		__buf->st_size  = (uint32_t)entry->size;
	}

	// Return success.
	return 0;
}




struct dirent64 *cache_nextentry(rar_cache_t *node)
{
	static struct dirent64 result;
	archive_t *archive;
	entry_t *entry;

#ifdef DO_NOTHING
	return NULL;
#endif

    if (!node) return NULL;

	// next >= num of archives, means we are at the end, nothing to return
	if (node->next_archive >= node->num_archives)
		return NULL;

	archive = node->archives[ node->next_archive ];
	if (!archive) return NULL; // paranoia

	// next >= num of entries, means we are at the end, nothing to return
	if (archive->next_entry >= archive->num_entries)
		return NULL;

	entry = archive->entries[ archive->next_entry ];
	if (!entry) return NULL; // paranoia

	// Build next dirent64 node, and return it.
	memset(&result, 0, sizeof(result));

	// strncpy sometimes does not null-terminate, but we memset to 0 above
	strncpy(result.d_name, entry->filename, sizeof(result.d_name) -1 );
	result.d_reclen = strlen(result.d_name) + 22; // FIXMEE
	result.d_type = DT_REG; // FIXME, support non-files? directories?
	// warning, d_off will be wrong :( so seekdir/rewinddir will not work.

	xdebugf("[nextentry] returning new entry '%s' (%d) \n", 
		   result.d_name, result.d_reclen);

	// Increment the next ptrs.
	archive->next_entry++;
	if (archive->next_entry >= archive->num_entries) {

		// Start from the begining of entries.
		archive->next_entry = 0;

		// Move to next archive, or end.
		node->next_archive++;
	}

	// return new node
	return &result;
}
