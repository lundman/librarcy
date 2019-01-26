
#ifndef RAR_CACHE_T
#define RAR_CACHE_T

#define SAFE_FREE(X) { if ((X)) { free((X)); (X) = NULL; } }


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


// file-descriptor mapping to internal code.
struct rar_fdmap_s
{
	int fd;                   // fd used in open/read/lseek/close.
	FILE *pstream;            // stream from popen / pclose
	uint64_t current_offset;  // Actual position in stream.
	uint64_t wanted_offset;   // offset set/expected from lseek calls.
	uint64_t size;            // filesize for lseek(SEEK_END)
	int lseek_called;         // lseek has been called.
	char *archive_fullpath;   // "/dir/file.rar"
	char *archive_name;       // "file.avi", name inside archive.
	int despawn_closing;      // close() called due to despawn().
	struct rar_fdmap_s *next; // next node, if more than one popen.
};
typedef struct rar_fdmap_s rar_fdmap_t;



#endif
