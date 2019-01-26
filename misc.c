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
#include "interpose.h"




void xdebugf(const char const *fmt, ...)
{
	va_list ap;
	char msg[1024];
	FILE *fd;

	if (!getenv("LIBRARCY_DEBUG"))
		return;

#if 0
	fd = fopen("/tmp/librarcy.log", "a");

	if (fd) {
		va_start(ap, fmt);
		vsnprintf(msg, sizeof(msg), fmt, ap);
		va_end(ap);
		fputs(msg, fd);
		fclose(fd);
	}
#else

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	fputs(msg, stderr);

#endif
}



char *strip(char *s)
{
	char *r;

	if ((r = strchr(s, '\r'))) *r = 0;
	if ((r = strchr(s, '\n'))) *r = 0;
	return s;
}


// Filled in with the matched character
static char misc_digtoken_optchar = 0;
char *misc_digtoken(char **string,char *match)
{
    misc_digtoken_optchar = 0;

    if(string && *string && **string) {

        // Skip "whitespace", anything in 'match' until we have something
        // that doesn't.
        // However, if we hit a " we stop asap, and match only to next ".
        while(**string && strchr(match,**string)) {
            // If a char is the quote, and we match against quote, stop loop
            (*string)++;
        }

        // If the first char is now a quote, and we match against quotes:
        if (!strchr(match, '"') && (**string == '"')) {
            (*string)++;
            match = "\"";
        }

        if(**string) { /* got something */
            char *token=*string;

            if((*string=strpbrk(*string,match))) {

                misc_digtoken_optchar = *(*string);
                *(*string)++=(char)0;
                while(**string && strchr(match,**string))
                    (*string)++;

            }  else
                *string = ""; /* must be at the end */

            return(token);
        }
    }
    return((char *)0);
}


char *misc_strjoin(char *a, char *b)
{
	char *result;
	int len_a, extra_slash;

	// Check if a ends with / or, b starts with /, if so dont
	// add a '/'.
	len_a = strlen(a);

	if ((a[ len_a - 1 ] != '/') && b[0] != '/')
		extra_slash = 1;
	else
		extra_slash = 0;

	result = (char *) malloc(len_a + strlen(b) + 1 + extra_slash);

	if (!result) {
		perror("malloc");
		exit(2);
	}

	if (extra_slash)
		sprintf(result, "%s/%s", a, b);
	else
		sprintf(result, "%s%s", a, b);

	return result;
}



const char *my_basename(const char *s)
{
	char *r;

	if (!s || !*s) return s;

	r = strrchr(s, '/');
	if (!r) return s;

	return &r[1];
}









// FIXME
// Look for part01.rar, ignore partXX.rar
// Look for .001
// Look for *.rar, that isnt partXX.rar
int filename_is_rar(const char *filename)
{
	int len;

#ifdef DO_NOTHING
	return 0;
#endif

	// part01.rar, part001.rar OK
	// partXX.rar, partXXX.rar, where XXX != 1, skip.
	// file.rar OK
	// file.001 OK
	len = strlen(filename);

	// easy case, ends with 001, then yes.
	if (!strcasecmp(&filename[len - 4], ".001"))
		return 1;

	// next one, if it ISN'T ending with .rar
	if (strcasecmp(&filename[len - 4], ".rar"))
		return 0;

	// Now, its a .rar, check for .partX. .partXX. .partXXX. .partXXXX.
	if (len >= 10) { // there is room for at least ".part1.rar"
		int digits;

		// Point to where the "last" digit should be
		digits = len - 5;
		// Skip past all digits
		while((digits > 0) && (isdigit(filename[digits]))) digits--;
		// now, we should have ".part"
		if (digits < 5) return 1; // not enough room for ".part" then its rar

		if (strncasecmp(&filename[digits - 4], ".part", 5))
			return 1; // it isnt ".part", so just a .rar

		// Check the digits and see if it is 1, or not.
		if (strtoll(&filename[digits + 1], NULL, 10) == 1)
			return 1; // it is ".part001.rar" so rar file

		// it is ".partXXX.rar" but it isnt part 1, not RAR
		return 0;
	}

	// Well, it had .rar extention, so probably yes.
	return 1;
}




char *rar_getpath(void)
{
	char *path;

	path = getenv("LIBRARCY_UNRAR");
	if (path) {
		return path;
	}
	return "/tmp/unrar";
}


//
// Skip likely directories not to contain RAR archives to speed up
// execution.
//
int skip_directory(__const char *__name)
{
	if (!__name) return 1; // Skip

	if (!*__name) return 1; // Skip

	if (!strncasecmp("/persistfs/", __name, 11))
		return 1;

	if (!strcasecmp("/dev", __name))
		return 1;

	if (!strncasecmp("/dev/", __name, 5))
		return 1;

	if (!strncasecmp("/sys/", __name, 5))
		return 1;

	if (!strncasecmp("/lib/", __name, 5))
		return 1;

	if (!strcasecmp("/tmp", __name))
		return 1;

	if (!strncasecmp("/proc/", __name, 6))
		return 1;

	if (!strncasecmp("/bin/", __name, 5))
		return 1;

	if (!strcasecmp("/usr/bin", __name))
		return 1;

	if (!strncasecmp("/usr/bin/", __name, 9))
		return 1;

	if (!strcmp("/", __name))
		return 1;

	return 0; // Check it for RARs
}
