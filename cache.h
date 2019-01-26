
#ifndef CACHE_H_INCLUDED
#define CACHE_H_INCLUDED



// It is unlikely that there are more than one media file in a rar archive.
#define ENTRY_ALLOCATE_STEPS 5

// It is unlikely that there are more than one RAR file in a directory
#define ARCHIVE_ALLOCATE_STEPS 5



struct entry_s {
	char *filename;        // Name of entry in archive "test.avi"
	time_t mtime;          // File modification time
	uint64_t size;         // File size in bytes
	uint32_t type;         // 0 file, 1 directory, ...
};
typedef struct entry_s entry_t;



struct archive_s {
	char *rarname;         // Name of archive "test.rar".
	entry_t **entries;     // List of items in rar archive
	uint32_t max_entries;  // current allocated size
	uint32_t num_entries;  // current number of entries archive
	uint32_t next_entry;   // next entry when iterating.
	struct stat64 stat;
};
typedef struct archive_s archive_t;



struct rar_cache {
	DIR *dirp;             // If in opendir/readdir
	char *directory_path;  // absolute path to dir containing this entry
	archive_t **archives;  // any found archive and contents.
	uint32_t max_archives; // current allocated size, when reached, realloc
	uint32_t num_archives; // current number of archives found.
	uint32_t next_archive; // when readdir iterating, next archive.
};
typedef struct rar_cache rar_cache_t;


void             entry_freenode   ( entry_t **entry );
entry_t         *entry_newnode    ( void );
void             entry_addnode    ( archive_t *archive, entry_t *entry );
void             archive_addnode  ( rar_cache_t *cache, archive_t *archive );
archive_t       *archive_newnode  ( char *name );
void             archive_freenode ( archive_t **archive );
void             cache_freenode   ( rar_cache_t **node );
rar_cache_t     *cache_newnode    ( char *path, DIR * );
int              cache_findfile   ( rar_cache_t **cache,
                                    uint32_t *archive_index,
                                    uint32_t *entry_index,
                                    __const char *__file );
int              cache_statfile64 ( __const char *__file,
                                    struct stat64 *__buf);
int              cache_statfile   ( __const char *__file,
                                    struct stat *__buf );
struct dirent64 *cache_nextentry  ( rar_cache_t * );
rar_cache_t     *cache_hasdirp    ( DIR *dirp );
void             cache_clear      ( void );


#endif
