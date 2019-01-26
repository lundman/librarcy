
#ifndef FDMAP_H_INCLUDED
#define FDMAP_H_INCLUDED



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

    // buffering logic.
#define FDMAP_NUM_BUFFERS 2   // How many buffers to allocate
    uint32_t buffer_size;     // Allocated size of buffers.
    uint8_t *buffers[ FDMAP_NUM_BUFFERS ];
    uint64_t buffer_offset[ FDMAP_NUM_BUFFERS ];
    uint64_t buffer_amount[ FDMAP_NUM_BUFFERS ];
    uint32_t buffer_current;  // Which buffer to use next.

	struct rar_fdmap_s *next; // next node, if more than one popen.
};
typedef struct rar_fdmap_s rar_fdmap_t;




rar_fdmap_t *rar_fdmap_find     ( int fd );
rar_fdmap_t *rar_fdmap_newnode  ( char *archive_fullpath,
                                  char *archive_name );
void         rar_fdmap_freenode ( rar_fdmap_t *node );
uint64_t     fdmap_have_buffer  ( rar_fdmap_t *fdmap,
								  uint8_t *__buf,
								  uint64_t __nbytes );
void         fdmap_add_buffer   ( rar_fdmap_t *fdmap,
								  uint8_t *__buf,
								  uint64_t __nbytes );



#endif
