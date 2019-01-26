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
#include "interpose.h"
#include "cache.h"






static void set_signals(int deflt)
{
	static struct sigaction sa, restore_chld_sa, restore_pipe_sa;
	static char *preload = NULL;
	// When calling popen, we need to set the PIPE signal back to
	// default, so that when we close the PIPE, it sends a KILL to the
	// child.
	if (deflt) {

		// There is no need for unrar to call this layer as well, so
		// we clear it from the env. (perhaps we should have own popen)
		if (!preload)
			preload = getenv("LD_PRELOAD");
		unsetenv("LD_PRELOAD"); // Stop unrar from triggering us!

		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sa.sa_handler = (void *)SIG_DFL;
		sigemptyset (&sa.sa_mask);
		sigaction (SIGCHLD, &sa, &restore_chld_sa);
		sa.sa_handler = SIG_DFL;
		sigaction (SIGPIPE, &sa, &restore_pipe_sa);

	} else {

		// Set PRELOAD back, so that other children will be affected, like
		// that of "file_play".
		if (preload)
			setenv("LD_PRELOAD", preload, 0);

		// set the signals back to whatever the user had them as.
		sigaction(SIGCHLD, &restore_chld_sa, NULL);
		sigaction(SIGPIPE, &restore_pipe_sa, NULL);

	}
}



//
// list the contents of a rar archive, and add to the cache entry for this
// directory.
//
void rar_expandlist(rar_cache_t *cache, char *archive_name)
{
	archive_t *archive = NULL;
	entry_t *entry = NULL;
	char *archive_path = NULL;
	char *filename = NULL;
	int parse_style = 0, directory;
	char buffer[1024];
	FILE *input;
    char *name, *fsize, *packed, *ratio, *date, *thyme, *attr;
    char *ar, *path, *slash, *root, *line;

	if (!cache) return;

	xdebugf("[expandlist] called, cache path '%s'\r\n", cache->directory_path);

	archive_path = misc_strjoin(cache->directory_path, archive_name);
	if (!archive_path) goto finished;

	// Allocate a new archive node, but we don't insert it until we know we
	// have entries.
	archive = archive_newnode(archive_name);
	if (!archive) goto finished;

	// Lets get some info for the rar
	memset(&archive->stat, 0, sizeof(archive->stat));

	if (!lookup_real((void *)&real_stat64, "stat64")) {
		goto finished;;
	}

	real_stat64(archive_path, &archive->stat); // Only using it to clone

	// Lets call unrar for a listing...
	snprintf(buffer, sizeof(buffer), "%s v -v -c- -p- -y -cfg- -- \"%s\"",
			 rar_getpath(),
			 archive_path);

	xdebugf("[expandlist] spawning '%s' ...\n", buffer);

	//unsetenv("LD_PRELOAD"); // Stop unrar from triggering us!

	set_signals(1);

	input = popen(buffer, "r");

	set_signals(0);

	if (input) {
		while(fgets(buffer, sizeof(buffer), input)) {

			strip(buffer);
			line = buffer;

			switch(parse_style) {

			case 0: // Wait for line with dashes
				if (!strncmp("--------------------", line, 20))
					parse_style++;
				break;

			case 1:  // woohooo, actually parsing entries
				// This is the filename part
				// " directory1/directory2/file2.avi"
				if (!strncmp("--------------------", line, 20)) {
					parse_style = 0;
					break;
				}

				if (*line != ' ') {
					xdebugf("[rar] unable to parse: '%s'\n", line);
					break;
				}

				// Skip that leading space.
				if (*line == ' ')
					line++;

				// Remember this line until next parsing line.
				filename = strdup(line);

				parse_style++;
				break;

			case 2: // parse out filesize, type etc.
				// "   7       17 242% 04-12-07 14:11 -rw-r--r-- 16B28489 m3b 2.9"
				// Alternate filename/data lines, change back to filename
				parse_style = 1;
				ar = line;
				fsize  = misc_digtoken(&ar, " \r\n");
				packed = misc_digtoken(&ar, " \r\n");
				ratio  = misc_digtoken(&ar, " \r\n");
				date   = misc_digtoken(&ar, " \r\n");
				thyme  = misc_digtoken(&ar, " \r\n");
				attr   = misc_digtoken(&ar, " \r\n");

				if (!filename ||
					!fsize|| !*fsize||
					!attr || !*attr) {
					xdebugf("[rar] unable to parse -- skipping\n");
					break;
				}

				// Files spanning Volumes will be repeated, but we can tell by
				// lookip at "ratio" column.
				if (!strcmp("<->", ratio) ||
					!strcmp("<--", ratio)) {

					SAFE_FREE(filename);
					break;
				}

				xdebugf("[rar] parsed name '%s'\n", filename);
				slash = strrchr(filename, '/');
				if (!slash) { // We are in root.
					path = "/";
					name = filename;
				} else { // We are in a subdir
					*slash = 0;
					name = &slash[1];
					path = filename;
				}

				// Now "path" should hold full path, and "name" just the
				// entry name.

				// Fixme to handle subdirectories?
				//if (node->rar_directory) {
				//	root = node->rar_directory;
				// Skip leading "/" as unrar wont start with /
				// while (*root == '/') root++;
				// } else
				root = "/";

				xdebugf("[rar] Checking '%s' == '%s'\n",
					   path, root);

				//  Now check if it is for a path we care about
				if (!strcmp(path, root)) {
					// It is
					//rar attributes. If DOS format, it will be
					// .D.....    : 7 chars.
					// Unix format:
					// d--------- :  10 chars
					directory = 0;
					if ((tolower(*attr) == 'd') ||
						(tolower(attr[1]) == 'd'))
						directory = 1;

					// Here, insert entry
					// FIXME, only handles files
					if (!directory)  {
						xdebugf("[rar] we should insert entry '%s' here: '%s' \n",
							   filename, date);
						entry = entry_newnode();
						if (entry) {
							entry->filename = filename;
							filename = NULL;
							entry->type = 0;
							entry->size = strtoull(fsize, NULL, 10);
							entry->mtime= 0; // fixme
							entry_addnode(archive, entry);
							entry = NULL;
						} // entry

					} // !directory

				} // directory we care about (in our path)

				// Release the filename from previous line.
				SAFE_FREE(filename);
				break;

			case 3:  // seen ----- the rest is fluff
				break;
			} // switch parse_Style

		} // while fgets

		pclose(input);

	} // if popen

	if (archive && archive->num_entries) {
		archive_addnode(cache, archive);
		archive = NULL;
	}


 finished:
	SAFE_FREE(archive_path);
	entry_freenode(&entry);     // OK to pass NULL
	archive_freenode(&archive); // OK to pass NULL


}



//
// Kill any (possibly) running unrar process.
//
int rar_despawn(rar_fdmap_t *fdmap)
{

	if (fdmap && fdmap->pstream) {
		int oldfd;
		xdebugf("[despawn] stopping previous unrar\r\n");
		oldfd = fileno(fdmap->pstream);

		// pclose() calls close() and we end up in our close() thinking we
		// can release fdmap. We need to signal to our close() that this
		// is not a real close from the caller.
		if (pclose(fdmap->pstream) == -1)
			xdebugf("[despawn] pclose failed: %d\r\n", errno);

		fdmap->pstream = NULL;
		// if the pstream oldfd is not that of the fdmap->fd that means we
		// called dup2() to make them match. If so, close "fd" as well,
		// if they were the same, pclose() already closed it.
		if ((oldfd != fdmap->fd) &&
			(fdmap->fd >= 0) ) {
			xdebugf("[despawn] also closing dup2 fd %d\n", fdmap->fd);
			close(fdmap->fd);
		}
		// We keep fdmap->fd for the next spawn, so the fd value do not
		// change for the API caller.

	}

	return 0;
}



//
// Spawn unrar, or re-spawn for a new seek position
//
int rar_spawn(rar_fdmap_t *fdmap)
{
	char buffer[1024];

	if (!fdmap) return -1;

	// tell close() to ignore any calls
	fdmap->despawn_closing = 1;

	// If we already have a rar running, stop it first.
	rar_despawn(fdmap);

	// Get ready to spawn unrar...

	snprintf(buffer, sizeof(buffer),
			 "%s p -inul -c- -p- -y -cfg- -sk%llu -- \"%s\" \"%s\"",
			 rar_getpath(),
			 fdmap->wanted_offset,
			 fdmap->archive_fullpath,
			 fdmap->archive_name);

	xdebugf("[spawn] launching '%s' ...\r\n", buffer);

	// We are now at wanted position, update current.
	fdmap->lseek_called = 0;
	fdmap->current_offset = fdmap->wanted_offset;

	set_signals(1);

	fdmap->pstream = popen(buffer, "r");

	set_signals(0);

	// Woah, it failed, this could be bad.
	if (!fdmap->pstream) goto finished;

	// If we already had a "fd" set, check the new fd is the same, but
	// if it is not, make it so by calling dup2().
	if ((fdmap->fd != -1) &&
		(fdmap->fd != fileno(fdmap->pstream))) {
		// Ok, new fd is used, we need to call dup2.
		xdebugf("[spawn]: fd dup2 call %d->%d\r\n", fileno(fdmap->pstream),
			   fdmap->fd);
		// If dup2() calls close(), don't trigger fdmap release
		dup2(fileno(fdmap->pstream), fdmap->fd);
	} else {
		// fd is new, or the same.
		fdmap->fd = fileno(fdmap->pstream);
		xdebugf("[spawn]: fd %d spawned.\r\n", fdmap->fd);
	}

	// Resume normal close() operations
	fdmap->despawn_closing = 0;

	// File is open, wooo
	return 0;

 finished:

	// Resume normal close() operations
	fdmap->despawn_closing = 0;

	xdebugf("[spawn] failed. %d\n", errno);
	return -1;

}


//
// popen a file inside an archive, for use with read().
//
int rar_openfile(rar_cache_t *cache, archive_t *archive, entry_t *entry)
{
	char *archive_path = NULL;
	rar_fdmap_t *fdmap = NULL;

	archive_path = misc_strjoin(cache->directory_path, archive->rarname);
	if (!archive_path) goto finished;


    fdmap = rar_fdmap_newnode(archive_path, entry->filename);
	if (!fdmap) goto finished;

	// fdmap_newnode copies the string, so we can release this version
	SAFE_FREE(archive_path);


	// Setup the filesize for lseek
	fdmap->size = entry->size;

	// We technically don't need to spawn rar here, but we need
	// to return a "fd" so we might as well

	if (rar_spawn(fdmap))
		goto finished;

	// Return the new fd.
	return fdmap->fd;

 finished:
	xdebugf("[openfile] failed for '%s'\r\n", entry->filename);
	rar_fdmap_freenode(fdmap);
	SAFE_FREE(archive_path);
	errno = ENOENT;
	return -1;

}

