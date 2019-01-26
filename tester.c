#define _FILE_OFFSET_BITS 64
#define __USE_LARGEFILE64

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	DIR *dirp;
	int red, fd, fd2;
	char buffer[1024];
	struct stat stbf;
	struct dirent *ent;

	dirp = opendir(".");

	if (dirp) {

		while((ent = readdir(dirp))) {
			stat(ent->d_name, &stbf);
		}

		closedir(dirp);
	}

	if (argc > 1) {
		fd = open(argv[1], O_RDONLY);
		fd2 = open(argv[1], O_RDONLY);
		if (fd >= 0 ) {
			while((red = read(fd, buffer, sizeof(buffer))) > 0)
				write(1, buffer, red);
		if (fd2 >= 0 ) {
			while((red = read(fd2, buffer, sizeof(buffer))) > 0)
				write(1, buffer, red);
			close(fd2);
		}
			lseek(fd, 1, SEEK_SET);
			while((red = read(fd, buffer, sizeof(buffer))) > 0)
				write(1, buffer, red);
			close(fd);
		}
	}

	return 0;
}
