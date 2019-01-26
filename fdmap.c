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

#include "misc.h"
#include "fdmap.h"


// Mapping of currently open rar files. This is to map a fd (for example "6")
// to that of the unrar FILE * stream, as well as the seek position that we
// are currently on. We also need to remember "6" as the fd, incase we get
// a different fd when re-spawning. (We dup2 back to 6 in that case).
// Generally, this only has one node.
static rar_fdmap_t *rar_fdmap_head = NULL;


//
// This function is called rather a lot, especially since they tend to
// close all fds up to 1000 or similar.
//
rar_fdmap_t *rar_fdmap_find(int fd)
{
	rar_fdmap_t *runner;

	for (runner = rar_fdmap_head;
		 runner;
		 runner=runner->next) {
		if (runner->fd == fd)
			return runner;
	}
	return NULL;
}

//
// Allocate a new node, this also links the new node to head.
//
rar_fdmap_t *rar_fdmap_newnode(char *archive_fullpath,
							   char *archive_name)
{
	rar_fdmap_t *result;
    char *str;

	result = (rar_fdmap_t *) malloc(sizeof(*result));
	if (!result) return NULL;

	memset(result, 0, sizeof(*result));

	result->fd = -1; // not defined yet.

	result->lseek_called = 1; // We expect it to be at 0.
	result->archive_fullpath = strdup(archive_fullpath);
	result->archive_name = strdup(archive_name);

	result->next = rar_fdmap_head;
	rar_fdmap_head = result;

	xdebugf("[fdmap] linked '%s'\r\n", archive_name);

    // Use buffers?
    str = getenv("LIBRARCY_BUFSIZE");
    if (str) {
        int i;
        result->buffer_size = strtoul(str, NULL, 10);
        xdebugf("[fdmap] attempting to allocate buffers of %llu bytes\r\n",
               result->buffer_size);
        for (i = 0; i < FDMAP_NUM_BUFFERS; i++) {

            result->buffers[ i ] = malloc(result->buffer_size);

            if (!result->buffers[ i ])
                result->buffer_size = 0; // disables buffer functions...

        } // for
    }
    // if use buffers

	return result;
}



//
// delink and free node
//
void rar_fdmap_freenode(rar_fdmap_t *node)
{
	rar_fdmap_t *runner;
    int i;

	if (!node) return;

	xdebugf("[fdmap] releasing '%s'\r\n", node->archive_name);
	SAFE_FREE(node->archive_fullpath);
	SAFE_FREE(node->archive_name);

    for (i = 0; i < FDMAP_NUM_BUFFERS; i++) {
        SAFE_FREE(node->buffers[ i ]);
    }

	// Attempt to unlink it here.
	if (rar_fdmap_head == node) {
		// First node.
		rar_fdmap_head = node->next;
	} else {
		// not first node, find it.
		for (runner = rar_fdmap_head;
			 runner;
			 runner = runner->next) {
			if (runner->next == node) {
				runner->next = node->next;
				break;
			} // is node
		} // for all nodes
	} // not first node

}



uint64_t fdmap_have_buffer(rar_fdmap_t *fdmap,
                           uint8_t *__buf,
                           uint64_t __nbytes)
{
    uint64_t result = 0;
    uint64_t offset, bsize, modula;
    int i;

    if (!fdmap->buffer_size) // no buffers allocated, return.
        return 0;

    // We have a (new) read request for data after a call to lseek, check
    // if wanted data are in buffers.

    offset = fdmap->wanted_offset;

    // While they want bytes:
    while( __nbytes) {

        // Clear it so we know if the for managed to copy data.
        bsize = 0;

        // For each buffer...
        for (i = 0; i < FDMAP_NUM_BUFFERS; i++) {

            // Is the wanted offset in this buffer?
            if ((offset >= fdmap->buffer_offset[ i ]) &&
                (offset <  fdmap->buffer_offset[ i ] + fdmap->buffer_amount[ i ])){

                modula = offset - fdmap->buffer_offset[ i ];
                bsize = MIN(__nbytes, fdmap->buffer_amount[ i ] - modula);

				xdebugf("[fdmap] copying %llu bsize to offset %llu, nbytes %llu modula %llu. buffer start %llu amount %llu\r\n",
						bsize, offset, __nbytes,
						modula, fdmap->buffer_offset[i],
						fdmap->buffer_amount[i]);

                // Copy over data...
                memcpy(__buf,
                       &fdmap->buffers[ i ][ modula ],
                       bsize);
                __nbytes -= bsize;
                __buf = &__buf[ bsize ];
                offset += bsize;
                result += bsize;

				// Since we've read, and essentially moved the wanted_seek
				// we need to update that
				fdmap->wanted_offset += bsize;


            } // buffer has data we want!

        } // for buffers

        // If bsize is still 0, we did not have anything (more) in the buffers
        // so it is time to leave...
        if (!bsize) break;

    } // while nbytes

    xdebugf("[fdmap] has_buffers. Found %llu in buffers\r\n", result);

	for (i = 0; i < FDMAP_NUM_BUFFERS; i++) {
		xdebugf("[fdmap] buffer %u: start %llu amount %llu (%llu)\r\n",
			   i, fdmap->buffer_offset[i],fdmap->buffer_amount[i],
			   fdmap->buffer_offset[i] + fdmap->buffer_amount[i]);
	}

    return result;
}


//
// This function is called when we have read new bytes from unrar,
// if we have nothing in the buffers, pick the next free buffer, and copy over.
// if we are "strictly" continuing reading on a previous buffer, append
// if we over-flow on the buffer, start on, and copy to, the next buffer.
//
// IN:  __buf: ptr to new data
// IN:  __nbytes: amount of new data
// USES:  current_offset
// MODIFIES:
// Changes buffer ptrs, buffer_offset, and buffer_amount.
//
void fdmap_add_buffer(rar_fdmap_t *fdmap,
                      uint8_t *__buf,
                      uint64_t __nbytes)
{
    int i;
    uint64_t offset, bsize;

    if (!fdmap->buffer_size)
        return;

    // Where we have just read from (__buf starts at offset, for __nbytes)
    offset = fdmap->current_offset;

    // Check if this is a strict continuation of a previous read?
    for (i = 0; i < FDMAP_NUM_BUFFERS; i++) {

        if (fdmap->buffers[i] &&       // buffer allocated
            fdmap->buffer_amount[i] && // buffer has data
            (fdmap->buffer_offset[i] + fdmap->buffer_amount[i]) == offset) {

            // This buffer "i" is where we could stick the data. woo.
            // Work out how much we can fit.
            bsize = MIN(__nbytes, (fdmap->buffer_size - fdmap->buffer_amount[i]));

            memcpy(&fdmap->buffers[ i ][ fdmap->buffer_amount[i] ],
                   __buf,
                   bsize);

            // Remove the bytes we have already fit
            __nbytes -= bsize;
            __buf = &__buf[ bsize ];  // Advance the buffer by bsize bytes
            offset += bsize;          // Move offset forward by amount copied.
            fdmap->buffer_amount[ i ] += bsize;

            xdebugf("[fdmap] adding %llu bytes to buffer %d: start %llu for %llu bytes\r\n", 
					bsize, i,
					fdmap->buffer_offset[ i ],
					fdmap->buffer_amount[ i ]);

            // If it fit all the data, we are done.
            if (!__nbytes) return;

        } // continuation?
    } // for buffers

    // Ok, we have entirely new data, just take over the next available buffer
    // start copying data until we run out. Pay attention to when buffers fill
    // up and move to the next one automatically. Yes, with large reads we might
    // actually end up over-writing the previous buffers. The trick is to find the
    // right balance with buffer-sizes so that never happens.
    i = fdmap->buffer_current;

    while( __nbytes ) {

        bsize = MIN( __nbytes, fdmap->buffer_size);

        memcpy(fdmap->buffers[ i ],
               __buf,
               bsize);

        // Remember where this buffer starts from
        fdmap->buffer_offset[ i ] = offset;

        __nbytes -= bsize;
        __buf = &__buf[bsize];
        offset += bsize;
        fdmap->buffer_amount[ i ] = bsize;

		xdebugf("[fdmap] add_buffer: buffer %d starts %llu for %llu bytes\r\n",
				i, fdmap->buffer_offset[ i ], fdmap->buffer_amount[ i ] );

        // Rotate next available buffer.
        i++;
        if (i >= FDMAP_NUM_BUFFERS)
            i = 0;

    }// while we have bytes

    fdmap->buffer_current = i;

}


